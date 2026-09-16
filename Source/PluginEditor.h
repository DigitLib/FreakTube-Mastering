#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==============================================================================
// CUSTOM FREAKTUBE LOOK AND FEEL (EMERALD & OBSIDIAN HI-FI)
// ==============================================================================
class FreakTubeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FreakTubeLookAndFeel()
    {
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff00e676));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // Outer Shadow
        g.setColour(juce::Colour(0x60000000));
        g.fillEllipse(centre.x - radius + 2.0f, centre.y - radius + 3.0f, radius * 2.0f, radius * 2.0f);

        // 1. Heavy Knurled Outer Ring (Dark Gunmetal)
        juce::ColourGradient knurlGrad(juce::Colour(0xff444444), centre.x, centre.y - radius,
                                      juce::Colour(0xff181818), centre.x, centre.y + radius, false);
        g.setGradientFill(knurlGrad);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // Knurled notch marks along outer rim
        g.setColour(juce::Colour(0xff0d0d0d));
        for (int i = 0; i < 36; ++i)
        {
            float a = (float)i * (juce::MathConstants<float>::twoPi / 36.0f);
            float x1 = centre.x + std::cos(a) * (radius - 4.0f);
            float y1 = centre.y + std::sin(a) * (radius - 4.0f);
            float x2 = centre.x + std::cos(a) * radius;
            float y2 = centre.y + std::sin(a) * radius;
            g.drawLine(x1, y1, x2, y2, 1.5f);
        }

        // 2. Beveled Chrome Chamfer Ring
        float innerR1 = radius - 5.0f;
        juce::ColourGradient chromeGrad(juce::Colour(0xffe8e8e8), centre.x - innerR1, centre.y - innerR1,
                                        juce::Colour(0xff555555), centre.x + innerR1, centre.y + innerR1, false);
        g.setGradientFill(chromeGrad);
        g.fillEllipse(centre.x - innerR1, centre.y - innerR1, innerR1 * 2.0f, innerR1 * 2.0f);

        // 3. Concentric Brushed Aluminum Face
        float faceR = innerR1 - 4.0f;
        juce::ColourGradient faceGrad(juce::Colour(0xff2a2a2a), centre.x, centre.y - faceR,
                                      juce::Colour(0xff121212), centre.x, centre.y + faceR, false);
        g.setGradientFill(faceGrad);
        g.fillEllipse(centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);

        // Subtle concentric rings
        g.setColour(juce::Colour(0x1fffffff));
        g.drawEllipse(centre.x - faceR * 0.7f, centre.y - faceR * 0.7f, faceR * 1.4f, faceR * 1.4f, 1.0f);
        g.drawEllipse(centre.x - faceR * 0.4f, centre.y - faceR * 0.4f, faceR * 0.8f, faceR * 0.8f, 1.0f);

        // 4. Center Cap / Bevel
        g.setColour(juce::Colour(0xff222222));
        g.fillEllipse(centre.x - faceR * 0.25f, centre.y - faceR * 0.25f, faceR * 0.5f, faceR * 0.5f);

        // 5. Glowing Emerald Pointer Indicator Line & Dot
        float pX = centre.x + std::cos(toAngle - juce::MathConstants<float>::halfPi) * (faceR - 2.0f);
        float pY = centre.y + std::sin(toAngle - juce::MathConstants<float>::halfPi) * (faceR - 2.0f);
        float pInnerX = centre.x + std::cos(toAngle - juce::MathConstants<float>::halfPi) * (faceR * 0.35f);
        float pInnerY = centre.y + std::sin(toAngle - juce::MathConstants<float>::halfPi) * (faceR * 0.35f);

        g.setColour(juce::Colour(0xff00e676)); // Vibrant Emerald Pointer
        g.drawLine(pInnerX, pInnerY, pX, pY, 2.5f);
        g.fillEllipse(pX - 2.0f, pY - 2.0f, 4.0f, 4.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        bool isOn = button.getToggleState();

        // 1. Tactile Push-Button Outer Bezel / Recess
        g.setColour(juce::Colour(0x60000000));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 5.0f);

        juce::ColourGradient bezelGrad(juce::Colour(0xff2d3138), 0, bounds.getY(),
                                       juce::Colour(0xff121417), 0, bounds.getBottom(), false);
        g.setGradientFill(bezelGrad);
        g.fillRoundedRectangle(bounds, 5.0f);

        // Subtle bezel rim highlight
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xff555d68) : juce::Colour(0xff2b2f36));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        // 2. Inner Tactile Button Cap
        auto capBounds = bounds.reduced(3.0f);
        if (shouldDrawButtonAsDown || isOn)
            capBounds = capBounds.translated(0.0f, 0.8f);

        if (isOn)
        {
            // Active State: Emerald Backlit Halo
            juce::ColourGradient capOnGrad(juce::Colour(0xff143824), 0, capBounds.getY(),
                                          juce::Colour(0xff091f13), 0, capBounds.getBottom(), false);
            g.setGradientFill(capOnGrad);
            g.fillRoundedRectangle(capBounds, 3.5f);

            // Glowing emerald border
            g.setColour(juce::Colour(0x8000e676));
            g.drawRoundedRectangle(capBounds, 3.5f, 1.2f);
        }
        else
        {
            // Inactive State: Smoked Obsidian Cap
            juce::ColourGradient capOffGrad(juce::Colour(shouldDrawButtonAsHighlighted ? 0xff24282e : 0xff1c1f24), 0, capBounds.getY(),
                                           juce::Colour(0xff0e1013), 0, capBounds.getBottom(), false);
            g.setGradientFill(capOffGrad);
            g.fillRoundedRectangle(capBounds, 3.5f);

            g.setColour(juce::Colour(0xff282b31));
            g.drawRoundedRectangle(capBounds, 3.5f, 1.0f);
        }

        // Top specular edge on button cap
        g.setColour(juce::Colour(0x20ffffff));
        g.drawLine(capBounds.getX() + 4.0f, capBounds.getY() + 0.5f, capBounds.getRight() - 4.0f, capBounds.getY() + 0.5f, 1.0f);

        // 3. Status Indicator Pilot Lamp + Centered Label
        float lampSize = 9.0f;
        float spacing = 9.0f;

        juce::Font font (12.0f, juce::Font::bold);
        g.setFont(font);
        float textW = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
        float totalW = lampSize + spacing + textW;

        // Perfectly centered content inside the button cap
        float startX = capBounds.getCentreX() - totalW * 0.5f;
        float lampX = startX;
        float lampY = capBounds.getCentreY() - lampSize * 0.5f;
        float textX = lampX + lampSize + spacing;

        // Lamp housing ring
        g.setColour(juce::Colour(0xff0a0b0d));
        g.fillEllipse(lampX - 1.5f, lampY - 1.5f, lampSize + 3.0f, lampSize + 3.0f);

        if (isOn)
        {
            // Radiant emerald bloom
            g.setColour(juce::Colour(0x5500e676));
            g.fillEllipse(lampX - 3.0f, lampY - 3.0f, lampSize + 6.0f, lampSize + 6.0f);

            // Lit phosphor lens
            juce::ColourGradient lampLit(juce::Colour(0xffc8ffeb), lampX + 2.5f, lampY + 2.5f,
                                        juce::Colour(0xff00e676), lampX + lampSize, lampY + lampSize, true);
            g.setGradientFill(lampLit);
            g.fillEllipse(lampX, lampY, lampSize, lampSize);

            // Specular reflection
            g.setColour(juce::Colour(0xb0ffffff));
            g.fillEllipse(lampX + 2.0f, lampY + 2.0f, 3.0f, 2.0f);
        }
        else
        {
            // Unlit dark vintage green lens
            juce::ColourGradient lampUnlit(juce::Colour(0xff182e22), lampX + 2.0f, lampY + 2.0f,
                                          juce::Colour(0xff08110b), lampX + lampSize, lampY + lampSize, true);
            g.setGradientFill(lampUnlit);
            g.fillEllipse(lampX, lampY, lampSize, lampSize);

            g.setColour(juce::Colour(0xff333a40));
            g.drawEllipse(lampX, lampY, lampSize, lampSize, 0.8f);
        }

        // 4. Button Typography
        g.setColour(isOn ? juce::Colour(0xff00e676) : (shouldDrawButtonAsHighlighted ? juce::Colour(0xffd0d6dc) : juce::Colour(0xff8f99a3)));
        g.drawFittedText(button.getButtonText(), 
                         static_cast<int>(textX), 
                         static_cast<int>(capBounds.getY()), 
                         static_cast<int>(textW + 4.0f), 
                         static_cast<int>(capBounds.getHeight()), 
                         juce::Justification::centredLeft, 1);
    }
};

// ==============================================================================
// MAIN EDITOR
// ==============================================================================
class FreakTubeMasteringAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    FreakTubeMasteringAudioProcessorEditor (FreakTubeMasteringAudioProcessor&);
    ~FreakTubeMasteringAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void drawAnalogMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float needleNorm, const juce::String& channelName);

    FreakTubeMasteringAudioProcessor& audioProcessor;
    FreakTubeLookAndFeel freakTubeLF;

    // Controls
    juce::Slider driveSlider, ironSlider, outGainSlider;
    juce::Label driveLabel, ironLabel, outGainLabel;

    juce::ToggleButton powerButton { "POWER / ACTIVE" };
    juce::ToggleButton warmthButton { "WARMTH" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ironAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> warmthAttach;

    // Needle Ballistics (Inertia and Spring Damping)
    float leftNeedlePos = 0.0f;
    float rightNeedlePos = 0.0f;
    float leftNeedleVel = 0.0f;
    float rightNeedleVel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreakTubeMasteringAudioProcessorEditor);
};