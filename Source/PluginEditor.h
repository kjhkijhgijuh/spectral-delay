#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Black-and-white look: white paper, 1px black ink, one monospaced font.
class BWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BWLookAndFeel();

    static juce::Font mono (float height, bool bold = false);

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

//==============================================================================
// One drawable curve (64 points on a log-frequency axis).
class CurveEditor : public juce::Component
{
public:
    CurveEditor (SpectralDelayProcessor& p, int curveId, juce::String title);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float valueToY (float v) const;
    float yToValue (float y) const;
    float indexToX (int i) const;
    int xToIndex (float x) const;
    float freqToX (double hz) const;
    void drawAt (juce::Point<float> pos);
    juce::String rangeText() const;
    juce::String readout (int index) const;

    SpectralDelayProcessor& proc;
    const int id;
    const juce::String title;
    int lastIndex = -1;
    float lastValue = 0.0f;
    int hoverIndex = -1;
};

//==============================================================================
class SpectralDelayEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpectralDelayEditor (SpectralDelayProcessor&);
    ~SpectralDelayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupBar (juce::Slider& s, const juce::String& name, double defaultValue);
    void setupButton (juce::TextButton& b, const juce::String& text);

    BWLookAndFeel lnf;   // must outlive every child component
    SpectralDelayProcessor& proc;

    CurveEditor delayCurve, feedbackCurve, levelCurve, panCurve;

    juce::Slider timeBar, feedbackBar, mixBar, shiftBar;
    juce::TextButton freezeButton, riseButton, fallButton, randomButton, randomPanButton, resetButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> timeAtt, feedbackAtt, mixAtt, shiftAtt;
    std::unique_ptr<ButtonAttachment> freezeAtt;

    int lastCurveVersion = -1;
    float lastTime = -1.0f, lastFeedback = -1.0f;

    juce::Rectangle<int> delayLabelArea, allLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDelayEditor)
};
