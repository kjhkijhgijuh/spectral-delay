#include "PluginEditor.h"

namespace
{
    const juce::Colour ink   = juce::Colours::black;
    const juce::Colour paper = juce::Colours::white;
    constexpr float dash[] = { 2.0f, 3.0f };
    constexpr int editorW = 760, editorH = 452;
}

//==============================================================================
BWLookAndFeel::BWLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, paper);
    setColour (juce::TextButton::buttonColourId, paper);
    setColour (juce::TextButton::buttonOnColourId, ink);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, paper);
    setColour (juce::Slider::backgroundColourId, paper);
    setColour (juce::Slider::trackColourId, ink);
    setColour (juce::Slider::textBoxTextColourId, ink);
}

juce::Font BWLookAndFeel::mono (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

void BWLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                      float sliderPos, float minSliderPos, float maxSliderPos,
                                      juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    g.setColour (paper);
    g.fillRect (r);

    // bipolar parameters fill from zero, the others from the left edge
    float originX = (float) x;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        originX = (float) slider.getPositionOfValue (0.0);

    const auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (originX, sliderPos), (float) y,
                                                                  juce::jmax (originX, sliderPos), (float) (y + height));
    g.setColour (ink);
    g.fillRect (fill);
    g.drawRect (r, 1.0f);

    const auto textArea = r.reduced (8.0f, 0.0f);
    const auto name = slider.getName();
    const auto value = slider.getTextFromValue (slider.getValue());

    auto drawTexts = [&] (juce::Colour c)
    {
        g.setColour (c);
        g.setFont (mono (11.0f, true));
        g.drawText (name, textArea, juce::Justification::centredLeft, false);
        g.setFont (mono (11.0f));
        g.drawText (value, textArea, juce::Justification::centredRight, false);
    };

    // text is black on white and white on the black fill
    {
        juce::Graphics::ScopedSaveState s (g);
        g.excludeClipRegion (fill.getSmallestIntegerContainer());
        drawTexts (ink);
    }
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (fill.getSmallestIntegerContainer());
        drawTexts (paper);
    }
}

void BWLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                          bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState() || down;
    g.setColour (on ? ink : paper);
    g.fillRect (r);
    g.setColour (ink);
    g.drawRect (r, 1.0f);
    if (highlighted && ! on)
        g.drawRect (r.reduced (1.0f), 1.0f);
}

void BWLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    g.setColour (on ? paper : ink);
    g.setFont (mono (11.0f, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font BWLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return mono (11.0f, true);
}

//==============================================================================
CurveEditor::CurveEditor (SpectralDelayProcessor& p, int curveId, juce::String t)
    : proc (p), id (curveId), title (std::move (t))
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> CurveEditor::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (18.0f).reduced (0.5f);
}

float CurveEditor::valueToY (float v) const
{
    const auto r = sd::kCurveRanges[id];
    const auto a = plotArea().reduced (0.0f, 6.0f);
    return juce::jmap (v, r.lo, r.hi, a.getBottom(), a.getY());
}

float CurveEditor::yToValue (float y) const
{
    const auto r = sd::kCurveRanges[id];
    const auto a = plotArea().reduced (0.0f, 6.0f);
    return juce::jlimit (r.lo, r.hi, juce::jmap (y, a.getBottom(), a.getY(), r.lo, r.hi));
}

float CurveEditor::indexToX (int i) const
{
    const auto a = plotArea().reduced (3.0f, 0.0f);
    return a.getX() + a.getWidth() * (float) i / (float) (sd::kCurvePoints - 1);
}

int CurveEditor::xToIndex (float x) const
{
    const auto a = plotArea().reduced (3.0f, 0.0f);
    return juce::jlimit (0, sd::kCurvePoints - 1,
                         juce::roundToInt ((x - a.getX()) / a.getWidth() * (float) (sd::kCurvePoints - 1)));
}

float CurveEditor::freqToX (double hz) const
{
    const double bin = hz * sd::kFFTSize / proc.getCurrentSampleRate();
    const double pos = std::log (juce::jmax (1.0, bin)) / std::log ((double) (sd::kBins - 1));
    const auto a = plotArea().reduced (3.0f, 0.0f);
    return a.getX() + a.getWidth() * (float) pos;
}

juce::String CurveEditor::rangeText() const
{
    switch (id)
    {
        case sd::kCurveDelay:    return "0 .. 2x TIME";
        case sd::kCurveFeedback: return "-/+ FEEDBACK";
        case sd::kCurveLevel:    return "-48 .. +6 dB";
        case sd::kCurvePan:      return "L .. R";
        default:                 return {};
    }
}

juce::String CurveEditor::readout (int index) const
{
    const double pos = (double) index / (sd::kCurvePoints - 1);
    const double bin = std::exp (pos * std::log ((double) (sd::kBins - 1)));
    const double hz = bin * proc.getCurrentSampleRate() / sd::kFFTSize;
    const juce::String f = hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + " Hz"
                                       : juce::String (hz / 1000.0, 2) + " kHz";
    const float v = proc.getCurvePoint (id, index);
    juce::String val;

    switch (id)
    {
        case sd::kCurveDelay:
            val = juce::String (juce::roundToInt (juce::jlimit (0.0f, 2.0f, 1.0f + v) * proc.getTimeMs())) + " ms";
            break;
        case sd::kCurveFeedback:
            val = juce::String (juce::roundToInt (juce::jlimit (0.0f, 99.0f, proc.getFeedbackPercent() + v * 100.0f))) + " %";
            break;
        case sd::kCurveLevel:
            val = v <= -47.5f ? juce::String ("OFF") : (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
            break;
        case sd::kCurvePan:
            val = std::abs (v) < 0.02f ? juce::String ("C")
                                       : (v < 0.0f ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100.0f));
            break;
        default: break;
    }
    return f + "   " + val;
}

void CurveEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = plotArea();

    // title strip
    auto strip = bounds.withHeight (16.0f);
    g.setColour (ink);
    g.setFont (BWLookAndFeel::mono (11.0f, true));
    g.drawText (title, strip, juce::Justification::centredLeft, false);
    g.setFont (BWLookAndFeel::mono (10.0f));
    g.drawText (hoverIndex >= 0 ? readout (hoverIndex) : rangeText(), strip, juce::Justification::centredRight, false);

    // plot frame
    g.setColour (paper);
    g.fillRect (plot);
    g.setColour (ink);
    g.drawRect (plot, 1.0f);

    // dashed grid: 100 Hz / 1 kHz / 10 kHz and the neutral (0) line
    g.setFont (BWLookAndFeel::mono (9.0f));
    const std::pair<double, const char*> marks[] = { { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } };
    for (const auto& [hz, label] : marks)
    {
        const float x = freqToX (hz);
        if (x <= plot.getX() + 2.0f || x >= plot.getRight() - 2.0f)
            continue;
        g.drawDashedLine ({ x, plot.getY() + 1.0f, x, plot.getBottom() - 1.0f }, dash, 2, 1.0f);
        g.drawText (label, juce::Rectangle<float> (x + 3.0f, plot.getBottom() - 13.0f, 30.0f, 11.0f),
                    juce::Justification::centredLeft, false);
    }
    const float y0 = valueToY (0.0f);
    g.drawDashedLine ({ plot.getX() + 1.0f, y0, plot.getRight() - 1.0f, y0 }, dash, 2, 1.0f);

    // the curve
    juce::Path path;
    for (int i = 0; i < sd::kCurvePoints; ++i)
    {
        const juce::Point<float> pt (indexToX (i), valueToY (proc.getCurvePoint (id, i)));
        if (i == 0) path.startNewSubPath (pt);
        else        path.lineTo (pt);
    }
    g.strokePath (path, juce::PathStrokeType (1.5f));

    if (hoverIndex >= 0)
    {
        const juce::Point<float> pt (indexToX (hoverIndex), valueToY (proc.getCurvePoint (id, hoverIndex)));
        g.fillRect (juce::Rectangle<float> (5.0f, 5.0f).withCentre (pt));
    }
}

void CurveEditor::drawAt (juce::Point<float> pos)
{
    const int index = xToIndex (pos.x);
    const float value = yToValue (pos.y);

    if (lastIndex < 0 || lastIndex == index)
    {
        proc.setCurvePoint (id, index, value);
    }
    else
    {
        // fill the gap between the previous and the current mouse position
        const int step = index > lastIndex ? 1 : -1;
        for (int i = lastIndex; i != index + step; i += step)
        {
            const float t = (float) (i - lastIndex) / (float) (index - lastIndex);
            proc.setCurvePoint (id, i, lastValue + (value - lastValue) * t);
        }
    }

    lastIndex = index;
    lastValue = value;
    hoverIndex = index;
    repaint();
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    lastIndex = -1;
    drawAt (e.position);
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    drawAt (e.position);
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    lastIndex = -1;
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int index = plotArea().contains (e.position) ? xToIndex (e.position.x) : -1;
    if (index != hoverIndex)
    {
        hoverIndex = index;
        repaint();
    }
}

void CurveEditor::mouseExit (const juce::MouseEvent&)
{
    hoverIndex = -1;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent&)
{
    proc.resetCurve (id);
    repaint();
}

//==============================================================================
SpectralDelayEditor::SpectralDelayEditor (SpectralDelayProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      delayCurve (p, sd::kCurveDelay, "DELAY TIME"),
      feedbackCurve (p, sd::kCurveFeedback, "FEEDBACK"),
      levelCurve (p, sd::kCurveLevel, "LEVEL"),
      panCurve (p, sd::kCurvePan, "PAN")
{
    setLookAndFeel (&lnf);

    for (auto* c : { &delayCurve, &feedbackCurve, &levelCurve, &panCurve })
        addAndMakeVisible (c);

    setupBar (timeBar, "TIME", 375.0);
    setupBar (feedbackBar, "FEEDBACK", 50.0);
    setupBar (mixBar, "DRY/WET", 40.0);
    setupBar (shiftBar, "SHIFT", 0.0);

    timeAtt     = std::make_unique<SliderAttachment> (proc.apvts, "time", timeBar);
    feedbackAtt = std::make_unique<SliderAttachment> (proc.apvts, "feedback", feedbackBar);
    mixAtt      = std::make_unique<SliderAttachment> (proc.apvts, "drywet", mixBar);
    shiftAtt    = std::make_unique<SliderAttachment> (proc.apvts, "shift", shiftBar);

    setupButton (freezeButton, "FREEZE");
    freezeButton.setClickingTogglesState (true);
    freezeAtt = std::make_unique<ButtonAttachment> (proc.apvts, "freeze", freezeButton);

    using CP = SpectralDelayProcessor::CurvePreset;
    setupButton (riseButton, "RISE");
    setupButton (fallButton, "FALL");
    setupButton (randomButton, "RANDOM");
    setupButton (randomPanButton, "RND PAN");
    setupButton (resetButton, "RESET");
    riseButton.onClick      = [this] { proc.applyCurvePreset (CP::Rise); };
    fallButton.onClick      = [this] { proc.applyCurvePreset (CP::Fall); };
    randomButton.onClick    = [this] { proc.applyCurvePreset (CP::RandomDelay); };
    randomPanButton.onClick = [this] { proc.applyCurvePreset (CP::RandomPan); };
    resetButton.onClick     = [this] { proc.applyCurvePreset (CP::ResetAll); };

    setSize (editorW, editorH);
    startTimerHz (30);
}

SpectralDelayEditor::~SpectralDelayEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SpectralDelayEditor::setupBar (juce::Slider& s, const juce::String& name, double defaultValue)
{
    s.setName (name);
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setDoubleClickReturnValue (true, defaultValue);
    s.setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible (s);
}

void SpectralDelayEditor::setupButton (juce::TextButton& b, const juce::String& text)
{
    b.setButtonText (text);
    addAndMakeVisible (b);
}

void SpectralDelayEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);
    g.setColour (ink);

    g.setFont (BWLookAndFeel::mono (13.0f, true));
    g.drawText ("SPECTRAL DELAY", 14, 0, 300, 30, juce::Justification::centredLeft, false);
    g.setFont (BWLookAndFeel::mono (10.0f));
    g.drawText ("FFT 2048 / HOP 512 / LATENCY 2048 SMP", getWidth() - 314, 0, 300, 30,
                juce::Justification::centredRight, false);
    g.fillRect (0, 30, getWidth(), 1);

    g.setFont (BWLookAndFeel::mono (10.0f, true));
    g.drawText ("DELAY CURVE", delayLabelArea, juce::Justification::bottomLeft, false);
    g.drawText ("ALL CURVES", allLabelArea, juce::Justification::bottomLeft, false);

    g.setFont (BWLookAndFeel::mono (10.0f));
    g.drawText ("DRAG = DRAW    DOUBLE-CLICK = RESET    X AXIS = LOW -> HIGH",
                14, getHeight() - 22, getWidth() - 28, 16, juce::Justification::centredLeft, false);
}

void SpectralDelayEditor::resized()
{
    const int top = 44, bottom = getHeight() - 28, gap = 12;
    const int leftW = 520;
    const int curveW = (leftW - gap) / 2;
    const int curveH = (bottom - top - gap) / 2;

    delayCurve.setBounds    (14, top, curveW, curveH);
    feedbackCurve.setBounds (14 + curveW + gap, top, curveW, curveH);
    levelCurve.setBounds    (14, top + curveH + gap, curveW, curveH);
    panCurve.setBounds      (14 + curveW + gap, top + curveH + gap, curveW, curveH);

    auto col = juce::Rectangle<int> (14 + leftW + 14, top + 18, getWidth() - (14 + leftW + 14) - 14, bottom - top - 18);

    for (auto* s : { &timeBar, &feedbackBar, &mixBar, &shiftBar })
    {
        s->setBounds (col.removeFromTop (24));
        col.removeFromTop (8);
    }
    freezeButton.setBounds (col.removeFromTop (28));
    col.removeFromTop (6);

    delayLabelArea = col.removeFromTop (18);
    auto row = col.removeFromTop (24);
    const int bw = (row.getWidth() - 12) / 3;
    riseButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (6);
    fallButton.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (6);
    randomButton.setBounds (row);

    col.removeFromTop (6);
    allLabelArea = col.removeFromTop (18);
    row = col.removeFromTop (24);
    const int hw = (row.getWidth() - 6) / 2;
    randomPanButton.setBounds (row.removeFromLeft (hw));
    row.removeFromLeft (6);
    resetButton.setBounds (row);
}

void SpectralDelayEditor::timerCallback()
{
    const int v = proc.getCurveVersion();
    const float t = proc.getTimeMs(), fb = proc.getFeedbackPercent();
    if (v != lastCurveVersion || ! juce::exactlyEqual (t, lastTime) || ! juce::exactlyEqual (fb, lastFeedback))
    {
        lastCurveVersion = v;
        lastTime = t;
        lastFeedback = fb;
        for (auto* c : { &delayCurve, &feedbackCurve, &levelCurve, &panCurve })
            c->repaint();
    }
}
