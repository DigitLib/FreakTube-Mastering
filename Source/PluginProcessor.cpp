#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
FreakTubeMasteringAudioProcessor::createParameters() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      "DRIVE", "Drive", 0.0f, 24.0f, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      "IRON", "Weight", 5.0f, 30.0f, 10.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      "OUT_GAIN", "Output Gain", -24.0f, 12.0f, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      "POWER", "Power / Active", true));
  params.push_back(
      std::make_unique<juce::AudioParameterBool>("WARMTH", "Warmth", true));
  return {params.begin(), params.end()};
}

FreakTubeMasteringAudioProcessor::FreakTubeMasteringAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "Parameters", createParameters()),
      oversampler(2, 3,
                  juce::dsp::Oversampling<
                      float>::filterHalfBandFIREquiripple) // 8x Oversampling
{
  driveParam = apvts.getRawParameterValue("DRIVE");
  ironWeightParam = apvts.getRawParameterValue("IRON");
  outGainParam = apvts.getRawParameterValue("OUT_GAIN");
  powerParam = apvts.getRawParameterValue("POWER");
  warmthParam = apvts.getRawParameterValue("WARMTH");
}

FreakTubeMasteringAudioProcessor::~FreakTubeMasteringAudioProcessor() {}

void FreakTubeMasteringAudioProcessor::prepareToPlay(double sampleRate,
                                                    int samplesPerBlock) {
  oversampler.initProcessing(samplesPerBlock);
  setLatencySamples(juce::roundToInt(oversampler.getLatencyInSamples()));

  double osSampleRate =
      sampleRate * (double)oversampler.getOversamplingFactor();
  transformerL.prepare(osSampleRate);
  transformerR.prepare(osSampleRate);
  dcBlockerL.prepare(osSampleRate);
  dcBlockerR.prepare(osSampleRate);
}

void FreakTubeMasteringAudioProcessor::releaseResources() {}

void FreakTubeMasteringAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();
  if (numChannels == 0 || numSamples == 0)
    return;

  bool isPowerOn = (powerParam != nullptr) ? (powerParam->load() > 0.5f) : true;
  bool isWarmthOn =
      (warmthParam != nullptr) ? (warmthParam->load() > 0.5f) : true;

  if (!isPowerOn) {
    // Bypassed: update meters on dry audio and exit
    float rmsL = buffer.getRMSLevel(0, 0, numSamples);
    float rmsR =
        (numChannels > 1) ? buffer.getRMSLevel(1, 0, numSamples) : rmsL;
    float dbL = juce::Decibels::gainToDecibels(rmsL, -80.0f);
    float dbR = juce::Decibels::gainToDecibels(rmsR, -80.0f);
    meterL.store(dbL);
    meterR.store(dbR);
    rmsLevelOut.store(std::max(dbL, dbR));
    return;
  }

  float driveDb = driveParam->load();
  float outGainDb = outGainParam->load();
  float ironWidth = ironWeightParam->load();

  transformerL.setParameters(ironWidth);
  transformerR.setParameters(ironWidth);

  // 1. Digital Gain Staging for Tube Mastering:
  // Calibrated for exact unity loudness and pristine headroom through the tube
  // & transformer chain.
  float driveGain = juce::Decibels::decibelsToGain(driveDb);
  float outGain = juce::Decibels::decibelsToGain(outGainDb);

  buffer.applyGain(driveGain);

  // 2. Oversampling (8x equiripple FIR)
  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::AudioBlock<float> osBlock = oversampler.processSamplesUp(block);

  auto *leftData = osBlock.getChannelPointer(0);
  auto *rightData = (numChannels > 1) ? osBlock.getChannelPointer(1) : nullptr;
  const int osSamples = static_cast<int>(osBlock.getNumSamples());

  for (int i = 0; i < osSamples; ++i) {
    // ------------------ LEFT CHANNEL ------------------
    float l = leftData[i];

    // Stage 1: 12AX7 Input Triode Stage
    float l_pre = isWarmthOn ? (l + 0.025f * l * std::abs(l)) : l;

    // Stage 2: KT88 Push-Pull Unity-Coupled Stage
    float l_p2 = l_pre * l_pre;
    float l_power = l_pre / std::sqrt(1.0f + 0.18f * l_p2 * l_p2);

    // Stage 3: Bifilar Output Transformer with Core Weight
    float l_trans = transformerL.processSample(l_power);

    // Stage 4: DC Blocker (removes sub-audible DC drift)
    leftData[i] = dcBlockerL.processSample(l_trans);

    // ------------------ RIGHT CHANNEL -----------------
    if (rightData != nullptr) {
      float r = rightData[i];
      float r_pre = isWarmthOn ? (r + 0.025f * r * std::abs(r)) : r;
      float r_p2 = r_pre * r_pre;
      float r_power = r_pre / std::sqrt(1.0f + 0.18f * r_p2 * r_p2);
      float r_trans = transformerR.processSample(r_power);
      rightData[i] = dcBlockerR.processSample(r_trans);
    }
  }

  // Downsample back to original rate
  oversampler.processSamplesDown(block);
  buffer.applyGain(outGain);

  // Update Stereo Meters (independent Left and Right channels)
  float rmsL = buffer.getRMSLevel(0, 0, numSamples);
  float rmsR = (numChannels > 1) ? buffer.getRMSLevel(1, 0, numSamples) : rmsL;
  float dbL = juce::Decibels::gainToDecibels(rmsL, -80.0f);
  float dbR = juce::Decibels::gainToDecibels(rmsR, -80.0f);

  float curL = meterL.load();
  if (dbL > curL)
    curL += 0.8f * (dbL - curL);
  else
    curL += 0.1f * (dbL - curL);
  meterL.store(curL);

  float curR = meterR.load();
  if (dbR > curR)
    curR += 0.8f * (dbR - curR);
  else
    curR += 0.1f * (dbR - curR);
  meterR.store(curR);

  rmsLevelOut.store(std::max(curL, curR));
}

void FreakTubeMasteringAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void FreakTubeMasteringAudioProcessor::setStateInformation(const void *data,
                                                          int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor *FreakTubeMasteringAudioProcessor::createEditor() {
  return new FreakTubeMasteringAudioProcessorEditor(*this);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new FreakTubeMasteringAudioProcessor();
}