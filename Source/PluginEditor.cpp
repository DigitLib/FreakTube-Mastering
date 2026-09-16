#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
FreakTubeMasteringAudioProcessorEditor::FreakTubeMasteringAudioProcessorEditor (FreakTubeMasteringAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&freakTubeLF);

    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix) {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (2);
        addAndMakeVisible (s);

        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xffb8c2cc));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupSlider (driveSlider,   driveLabel,   "TUBE DRIVE", " dB");
    setupSlider (ironSlider,    ironLabel,    "WEIGHT", " %");
    setupSlider (outGainSlider, outGainLabel, "OUTPUT TRIM", " dB");

    addAndMakeVisible (powerButton);
    addAndMakeVisible (warmthButton);

    auto& apvts = audioProcessor.apvts;
    driveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "DRIVE", driveSlider);
    ironAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "IRON", ironSlider);
    outGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "OUT_GAIN", outGainSlider);
    powerAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "POWER", powerButton);
    warmthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "WARMTH", warmthButton);

    setSize (780, 480);
    startTimerHz (60);
}

FreakTubeMasteringAudioProcessorEditor::~FreakTubeMasteringAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FreakTubeMasteringAudioProcessorEditor::timerCallback()
{
    float dbL = audioProcessor.meterL.load();
    float dbR = audioProcessor.meterR.load();

    float targetL = juce::jmap (juce::jlimit (-40.0f, 3.0f, dbL), -40.0f, 3.0f, 0.0f, 1.0f);
    float targetR = juce::jmap (juce::jlimit (-40.0f, 3.0f, dbR), -40.0f, 3.0f, 0.0f, 1.0f);

    // D'Arsonval spring-mass-damping ballistic simulation (60 FPS)
    const float dt = 1.0f / 60.0f;
    float accL = (targetL - leftNeedlePos) * 180.0f - leftNeedleVel * 16.0f;
    leftNeedleVel += accL * dt;
    leftNeedlePos += leftNeedleVel * dt;

    float accR = (targetR - rightNeedlePos) * 180.0f - rightNeedleVel * 16.0f;
    rightNeedleVel += accR * dt;
    rightNeedlePos += rightNeedleVel * dt;

    leftNeedlePos  = juce::jlimit (0.0f, 1.05f, leftNeedlePos);
    rightNeedlePos = juce::jlimit (0.0f, 1.05f, rightNeedlePos);

    repaint();
}

void FreakTubeMasteringAudioProcessorEditor::drawAnalogMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float needleNorm, const juce::String& channelName)
{
    // Outer Bezel: Deep black obsidian frame with brushed titanium trim
    juce::ColourGradient bezelGrad (juce::Colour (0xff32363b), bounds.getX(), bounds.getY(),
                                   juce::Colour (0xff101214), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Chrome/Titanium inner bezel trim
    g.setColour (juce::Colour (0xff484f56));
    g.drawRoundedRectangle (bounds, 8.0f, 1.5f);

    auto inner = bounds.reduced (6.0f);

    // STRICT CLIPPING TO PREVENT ANY NEEDLE BLEED ACROSS BEZELS OR CHASSIS
    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (inner.toNearestInt());

    // 1. Vintage Illuminated Phosphor-Green Meter Face
    juce::ColourGradient greenGrad (juce::Colour (0xff03bb60), inner.getCentreX(), inner.getY(),
                                   juce::Colour (0xff013318), inner.getCentreX(), inner.getBottom(), false);
    g.setGradientFill (greenGrad);
    g.fillRoundedRectangle (inner, 4.0f);

    // Diffuse corner incandescent lamp highlights (warm golden-mint glow simulation)
    juce::ColourGradient lampL (juce::Colour (0x65a7ffeb), inner.getX() + 30.0f, inner.getY() + 10.0f,
                                juce::Colour (0x00013318), inner.getX() + 90.0f, inner.getY() + 90.0f, true);
    g.setGradientFill (lampL);
    g.fillEllipse (inner.getX() - 30.0f, inner.getY() - 30.0f, 180.0f, 150.0f);

    juce::ColourGradient lampR (juce::Colour (0x65a7ffeb), inner.getRight() - 30.0f, inner.getY() + 10.0f,
                                juce::Colour (0x00013318), inner.getRight() - 90.0f, inner.getY() + 90.0f, true);
    g.setGradientFill (lampR);
    g.fillEllipse (inner.getRight() - 150.0f, inner.getY() - 30.0f, 180.0f, 150.0f);

    // Inset glass depth shadow
    g.setColour (juce::Colour (0x45000000));
    g.drawRoundedRectangle (inner, 4.0f, 2.0f);

    // 2. Calibrated Mastering Scale Geometry (Lowered and Centered)
    float pivotX     = inner.getCentreX();
    float pivotY     = inner.getBottom() - 6.0f; // Mechanical pivot directly inside pivot cap
    float arcR_Peak  = 120.0f;                   // Upper Arc: Peak dBFS (Lowered from 142)
    float arcR_VU    = 94.0f;                    // Lower Arc: VU (-18 dBFS ref) (Lowered from 114)

    // Angle range: -36 deg to +36 deg (0.20 * pi)
    float startAngle = -juce::MathConstants<float>::pi * 0.20f;
    float endAngle   =  juce::MathConstants<float>::pi * 0.20f;

    // Base Arcs (All crisp white)
    g.setColour (juce::Colour (0xd0ffffff));
    juce::Path arcPeak, arcVU;
    arcPeak.addCentredArc (pivotX, pivotY, arcR_Peak, arcR_Peak, 0.0f, startAngle, endAngle, true);
    g.strokePath (arcPeak, juce::PathStrokeType (1.5f));

    arcVU.addCentredArc (pivotX, pivotY, arcR_VU, arcR_VU, 0.0f, startAngle, endAngle, true);
    g.strokePath (arcVU, juce::PathStrokeType (1.3f));

    // Overload Portion of Arcs (Bright pure white instead of red)
    float whiteOverloadStartPeak = startAngle + 0.88f * (endAngle - startAngle);
    float whiteOverloadStartVU   = startAngle + 0.85f * (endAngle - startAngle);

    juce::Path arcOverloadPeak, arcOverloadVU;
    arcOverloadPeak.addCentredArc (pivotX, pivotY, arcR_Peak, arcR_Peak, 0.0f, whiteOverloadStartPeak, endAngle, true);
    arcOverloadVU.addCentredArc (pivotX, pivotY, arcR_VU, arcR_VU, 0.0f, whiteOverloadStartVU, endAngle, true);
    g.setColour (juce::Colour (0xffffffff));
    g.strokePath (arcOverloadPeak, juce::PathStrokeType (2.4f));
    g.strokePath (arcOverloadVU, juce::PathStrokeType (2.0f));

    // Upper Arc: Peak dBFS Ticks & Text (All white)
    struct ScaleTick { float pos; const char* text; bool isMajor; };
    ScaleTick ticksPeak[] = {
        { 0.00f, "-30", true },
        { 0.20f, "-20", true },
        { 0.40f, "-14", true },
        { 0.60f, "-8",  true },
        { 0.76f, "-4",  false },
        { 0.88f, "0",   true },
        { 1.00f, "+2",  true }
    };

    g.setFont (juce::Font (9.5f, juce::Font::bold));
    for (const auto& tk : ticksPeak)
    {
        float a = startAngle + tk.pos * (endAngle - startAngle);
        float tickLen = tk.isMajor ? 6.0f : 3.5f;
        float x1 = pivotX + std::sin (a) * arcR_Peak;
        float y1 = pivotY - std::cos (a) * arcR_Peak;
        float x2 = pivotX + std::sin (a) * (arcR_Peak + tickLen);
        float y2 = pivotY - std::cos (a) * (arcR_Peak + tickLen);

        g.setColour (juce::Colour (0xeeffffff));
        g.drawLine (x1, y1, x2, y2, tk.isMajor ? 1.6f : 1.0f);

        if (tk.isMajor)
        {
            float tx = pivotX + std::sin (a) * (arcR_Peak + 12.0f);
            float ty = pivotY - std::cos (a) * (arcR_Peak + 12.0f);
            g.setColour (juce::Colour (0xffffffff));
            g.drawFittedText (tk.text, static_cast<int>(tx - 15.0f), static_cast<int>(ty - 7.0f), 30, 14, juce::Justification::centred, 1);
        }
    }

    // Lower Arc: VU (-18 dBFS ref) Ticks & Text (All white)
    ScaleTick ticksVU[] = {
        { 0.00f, "-20", true },
        { 0.28f, "-10", true },
        { 0.48f, "-6",  false },
        { 0.65f, "-3",  true },
        { 0.77f, "-1",  false },
        { 0.85f, "0",   true },
        { 0.93f, "+1",  false },
        { 1.00f, "+3",  true }
    };

    for (const auto& tk : ticksVU)
    {
        float a = startAngle + tk.pos * (endAngle - startAngle);
        float tickLen = tk.isMajor ? 6.0f : 3.5f;
        float x1 = pivotX + std::sin (a) * arcR_VU;
        float y1 = pivotY - std::cos (a) * arcR_VU;
        float x2 = pivotX + std::sin (a) * (arcR_VU - tickLen);
        float y2 = pivotY - std::cos (a) * (arcR_VU - tickLen);

        g.setColour (juce::Colour (0xeeffffff));
        g.drawLine (x1, y1, x2, y2, tk.isMajor ? 1.6f : 1.0f);

        // Mid-corridor number placement between arcs
        float numR = (arcR_Peak + arcR_VU) * 0.5f;
        float tx = pivotX + std::sin (a) * numR;
        float ty = pivotY - std::cos (a) * numR;
        g.setColour (juce::Colour (0xffffffff));
        g.drawFittedText (tk.text, static_cast<int>(tx - 13.0f), static_cast<int>(ty - 7.0f), 26, 14, juce::Justification::centred, 1);
    }

    // Functional Mastering Headers in Crisp White (Lowered and Centered)
    g.setColour (juce::Colour (0xffffffff));
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawFittedText ("PEAK dBFS", static_cast<int>(inner.getX()), static_cast<int>(inner.getY() + 20), static_cast<int>(inner.getWidth()), 14, juce::Justification::centred, 1);
    g.drawFittedText ("VU (-18 REF)", static_cast<int>(inner.getX()), static_cast<int>(inner.getY() + 94), static_cast<int>(inner.getWidth()), 14, juce::Justification::centred, 1);

    // Channel Label ("LEFT CHANNEL" / "RIGHT CHANNEL") - High contrast pure white with drop shadow, clear of pivot
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.setColour (juce::Colour (0x90000000));
    g.drawFittedText (channelName, static_cast<int>(inner.getX() + 1), static_cast<int>(inner.getBottom() - 39), static_cast<int>(inner.getWidth()), 16, juce::Justification::centred, 1);
    g.setColour (juce::Colour (0xffffffff));
    g.drawFittedText (channelName, static_cast<int>(inner.getX()), static_cast<int>(inner.getBottom() - 40), static_cast<int>(inner.getWidth()), 16, juce::Justification::centred, 1);

    // Needle Mechanics
    float needleAngle  = startAngle + needleNorm * (endAngle - startAngle);
    float needleLength = arcR_Peak + 5.0f;

    float tipX = pivotX + std::sin (needleAngle) * needleLength;
    float tipY = pivotY - std::cos (needleAngle) * needleLength;

    // Needle Shadow
    g.setColour (juce::Colour (0x35000000));
    g.drawLine (pivotX + 2.0f, pivotY + 2.0f, tipX + 3.0f, tipY + 2.0f, 2.0f);

    // Tapered fine aluminum needle body anchored directly inside the pivot hub
    juce::Path needlePath;
    float baseW = 1.8f;
    float perpX = std::cos (needleAngle);
    float perpY = std::sin (needleAngle);

    needlePath.startNewSubPath (pivotX - perpX * baseW, pivotY - perpY * baseW);
    needlePath.lineTo (pivotX + perpX * baseW, pivotY + perpY * baseW);
    needlePath.lineTo (tipX, tipY);
    needlePath.closeSubPath();

    g.setColour (juce::Colour (0xff141414));
    g.fillPath (needlePath);

    // Crisp white needle pointer tip
    g.setColour (juce::Colour (0xffffffff));
    g.drawLine (tipX - std::sin (needleAngle) * 12.0f, tipY + std::cos (needleAngle) * 12.0f, tipX, tipY, 1.8f);

    // Center Pivot Cap (Drawn on top of needle base, centered at pivotX, pivotY)
    g.setColour (juce::Colour (0xff141618));
    g.fillEllipse (pivotX - 14.0f, pivotY - 14.0f, 28.0f, 28.0f);
    g.setColour (juce::Colour (0xff484f56));
    g.drawEllipse (pivotX - 14.0f, pivotY - 14.0f, 28.0f, 28.0f, 1.5f);

    // Specular Glass Reflection Glare
    juce::Path glare;
    glare.startNewSubPath (inner.getX(), inner.getY());
    glare.lineTo (inner.getRight(), inner.getY());
    glare.lineTo (inner.getX(), inner.getY() + inner.getHeight() * 0.70f);
    glare.closeSubPath();

    juce::ColourGradient glareGrad (juce::Colour (0x24ffffff), inner.getX(), inner.getY(),
                                   juce::Colour (0x00ffffff), inner.getX() + 80.0f, inner.getY() + 80.0f, false);
    g.setGradientFill (glareGrad);
    g.fillPath (glare);
}

void FreakTubeMasteringAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Deep Obsidian Black High-Gloss Glass Chassis Faceplate
    g.fillAll (juce::Colour (0xff090a0d));

    // Luxury Beveled Edge
    g.setColour (juce::Colour (0xff1a1d22));
    g.drawRect (getLocalBounds().toFloat(), 1.5f);
    g.setColour (juce::Colour (0x18ffffff));
    g.drawRect (getLocalBounds().toFloat().reduced (1.5f), 1.0f);

    // High-Gloss Glass Top Highlight
    juce::ColourGradient glassGloss (juce::Colour (0x1cffffff), 0, 0,
                                    juce::Colour (0x00000000), 0, 80, false);
    g.setGradientFill (glassGloss);
    g.fillRect (0.0f, 0.0f, (float)getWidth(), 80.0f);

    // Clean Sans-Serif "FreakTube" with Emerald Glow (No Italic)
    g.setColour (juce::Colour (0x4500e676));
    g.setFont (juce::Font (juce::Font::getDefaultSansSerifFontName(), 27.0f, juce::Font::bold));
    g.drawFittedText ("FreakTube", 0, 11, getWidth(), 30, juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xff00e676)); // Vibrant Illuminated Emerald
    g.setFont (juce::Font (juce::Font::getDefaultSansSerifFontName(), 25.0f, juce::Font::bold));
    g.drawFittedText ("FreakTube", 0, 12, getWidth(), 30, juce::Justification::centred, 1);

    // Subtitle: STEREO MASTERING
    g.setColour (juce::Colour (0xffa8b5c2));
    g.setFont (juce::Font (juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold));
    g.drawFittedText ("STEREO MASTERING", 0, 39, getWidth(), 16, juce::Justification::centred, 1);

    // Dual Analog Emerald Green VU Meters
    float meterY   = 56.0f;
    float meterH   = 194.0f;
    float meterW   = 356.0f;
    float meterL_X = 22.0f;
    float meterR_X = (float)getWidth() - 22.0f - meterW;

    drawAnalogMeter (g, juce::Rectangle<float> (meterL_X, meterY, meterW, meterH), leftNeedlePos, "LEFT CHANNEL");
    drawAnalogMeter (g, juce::Rectangle<float> (meterR_X, meterY, meterW, meterH), rightNeedlePos, "RIGHT CHANNEL");

    // Polished Dark Chrome Separator Bar
    float barY = 258.0f;
    juce::ColourGradient barGrad (juce::Colour (0xff2d3138), 0, barY,
                                 juce::Colour (0xff6e7885), 0, barY + 1.5f, false);
    g.setGradientFill (barGrad);
    g.fillRect (20.0f, barY, (float)getWidth() - 40.0f, 2.0f);

    // Knob Index Ticks in Emerald Glow
    auto drawKnobTicks = [&g](float cx, float cy, float r) {
        g.setColour (juce::Colour (0x6000e676));
        for (int i = 0; i <= 10; ++i)
        {
            float norm = (float)i / 10.0f;
            float a = -juce::MathConstants<float>::pi * 0.75f + norm * juce::MathConstants<float>::pi * 1.5f - juce::MathConstants<float>::halfPi;
            float x1 = cx + std::cos (a) * (r + 4.0f);
            float y1 = cy + std::sin (a) * (r + 4.0f);
            float x2 = cx + std::cos (a) * (r + (i % 5 == 0 ? 9.0f : 6.0f));
            float y2 = cy + std::sin (a) * (r + (i % 5 == 0 ? 9.0f : 6.0f));
            g.drawLine (x1, y1, x2, y2, i % 5 == 0 ? 1.8f : 1.1f);
        }
    };

    drawKnobTicks (170.0f, 385.0f, 40.0f);
    drawKnobTicks (390.0f, 380.0f, 48.0f);
    drawKnobTicks (610.0f, 385.0f, 40.0f);
}

void FreakTubeMasteringAudioProcessorEditor::resized()
{
    // Tactile Button Row: Perfectly centered under each VU meter (Left center 200, Right center 580)
    int btnY = 274;
    int btnW = 150;
    int btnH = 34;
    powerButton.setBounds  (200 - btnW / 2, btnY, btnW, btnH); // X: 125, W: 150
    warmthButton.setBounds (580 - btnW / 2, btnY, btnW, btnH); // X: 505, W: 150

    // Rotary Sliders Row
    int driveSize = 82;
    driveSlider.setBounds (170 - driveSize / 2, 385 - driveSize / 2, driveSize, driveSize);
    driveLabel.setBounds  (170 - 70, 434, 140, 20);

    int ironSize = 98;
    ironSlider.setBounds (390 - ironSize / 2, 380 - ironSize / 2, ironSize, ironSize);
    ironLabel.setBounds  (390 - 70, 436, 140, 20);

    int outSize = 82;
    outGainSlider.setBounds (610 - outSize / 2, 385 - outSize / 2, outSize, outSize);
    outGainLabel.setBounds  (610 - 70, 434, 140, 20);
}