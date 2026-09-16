#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// ==============================================================================
// BIFILAR OUTPUT TRANSFORMER (Magnetic Weight DSP)
// ==============================================================================
class BifilarTransformer {
public:
  void prepare(double sampleRate) {
    fs = sampleRate > 0.0 ? sampleRate : 44100.0;
    reset();
    updateCoefficients();
  }

  void reset() {
    lowFlux1 = 0.0f;
    lowFlux2 = 0.0f;
    hystState = 0.0f;
  }

  void setParameters(float ironVal) {
    float clamped = juce::jlimit(5.0f, 30.0f, ironVal);
    if (std::abs(clamped - ironWeight) > 0.01f) {
      ironWeight = clamped;
      updateCoefficients();
    }
  }

  float processSample(float inputVoltage) {
    // 1. Extract low-frequency core flux (2-pole smooth filter ~300 Hz)
    // In the transformer core, only the bass and low-mids swing enough magnetic flux to
    // saturate the iron
    lowFlux1 += lpAlpha * (inputVoltage - lowFlux1);
    lowFlux2 += lpAlpha * (lowFlux1 - lowFlux2);
    float coreFlux = lowFlux2;

    // 2. Bifilar Magnetic Core Saturation
    // Symmetrical 3rd harmonic saturation + gentle 2nd harmonic warmth
    float fluxDriven = coreFlux * coreDrive;
    float satFlux = std::tanh(fluxDriven);

    // Additive transformer harmonic bloom:
    // Enriches the low-frequency fundamentals with warm 2nd and 3rd harmonics
    float harmonics = (satFlux - coreFlux) * ironHarmonicGain;

    // 3. Re-combine with the wideband signal
    float combined = inputVoltage + harmonics;

    // 4. Soft Peak Limiting (Analog transformer ceiling)
    // Leaves signals below 0.85 completely linear; softly rounds peaks near 1.0
    float absVal = std::abs(combined);
    float saturated = combined;
    if (absVal > 0.85f) {
      float excess = absVal - 0.85f;
      float compressed = 0.85f + 0.12f * std::tanh(excess * 3.5f);
      saturated = (combined > 0.0f) ? compressed : -compressed;
    }

    // 5. Magnetic hysteresis (warm analog weight and natural low-end damping)
    hystState += hystAlpha * (saturated - hystState);
    float out = (1.0f - hystMix) * saturated + hystMix * hystState;

    // 6. Output normalization: maintains exact mastering loudness across all
    // Iron settings
    return out * outLevelComp;
  }

private:
  void updateCoefficients() {
    // Normalize ironWeight from [5.0, 30.0] -> [0.0, 1.0]
    float normIron = (ironWeight - 5.0f) / 25.0f;

    // Low-pass corner frequency for core saturation (260 Hz to 420 Hz)
    float lpFreq = 260.0f + normIron * 160.0f;
    lpAlpha = juce::jmin(
        0.95f, (2.0f * juce::MathConstants<float>::pi * lpFreq) / (float)fs);

    // Core drive: How deeply the low frequencies saturate the transformer iron core
    // Iron 5 = 1.2x (transparent, pristine), Iron 30 = 3.2x (rich,
    // authoritative transformer iron)
    coreDrive = 1.2f + normIron * 2.0f;

    // Harmonic injection gain: scales the warmth and density
    ironHarmonicGain = 0.08f + normIron * 0.34f;

    // Hysteresis relaxation pole (~400 Hz) and mix
    float hystPole = 400.0f;
    hystAlpha = juce::jmin(
        0.95f, (2.0f * juce::MathConstants<float>::pi * hystPole) / (float)fs);
    hystMix = 0.03f + normIron * 0.07f;

    // Calibrated level compensation to preserve exact LUFS loudness
    outLevelComp = 1.0f / (1.0f + 0.08f * normIron);
  }

  double fs = 44100.0;
  float ironWeight = 10.0f;

  float lpAlpha = 0.05f;
  float coreDrive = 1.8f;
  float ironHarmonicGain = 0.2f;
  float hystAlpha = 0.05f;
  float hystMix = 0.05f;
  float outLevelComp = 0.98f;

  float lowFlux1 = 0.0f;
  float lowFlux2 = 0.0f;
  float hystState = 0.0f;
};

// ==============================================================================
// MASTERING DC BLOCKER (1-Pole High-Pass at 10 Hz)
// ==============================================================================
class DCBlocker {
public:
  void prepare(double sampleRate) {
    float fc = 10.0f;
    R = 1.0f - (2.0f * juce::MathConstants<float>::pi * fc / (float)sampleRate);
    reset();
  }

  void reset() {
    x1 = 0.0f;
    y1 = 0.0f;
  }

  float processSample(float in) {
    float out = in - x1 + R * y1;
    x1 = in;
    y1 = out;
    return out;
  }

private:
  float R = 0.9998f;
  float x1 = 0.0f;
  float y1 = 0.0f;
};

// ==============================================================================
// MAIN PROCESSOR
// ==============================================================================
class FreakTubeMasteringAudioProcessor : public juce::AudioProcessor {
public:
  FreakTubeMasteringAudioProcessor();
  ~FreakTubeMasteringAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "FreakTube Mastering"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int index) override {}
  const juce::String getProgramName(int index) override { return {}; }
  void changeProgramName(int index, const juce::String &newName) override {}
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState apvts;
  std::atomic<float> meterL{-60.0f};      // Left channel meter level
  std::atomic<float> meterR{-60.0f};      // Right channel meter level
  std::atomic<float> rmsLevelOut{-60.0f}; // Combined thread-safe meter level

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

  juce::dsp::Oversampling<float> oversampler;
  BifilarTransformer transformerL, transformerR;
  DCBlocker dcBlockerL, dcBlockerR;

  std::atomic<float> *driveParam = nullptr;
  std::atomic<float> *ironWeightParam = nullptr;
  std::atomic<float> *outGainParam = nullptr;
  std::atomic<float> *powerParam = nullptr;
  std::atomic<float> *warmthParam = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreakTubeMasteringAudioProcessor);
};