#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::ParameterID time     { "time", 1 };
    static const juce::ParameterID feedback { "feedback", 1 };
    static const juce::ParameterID drywet   { "drywet", 1 };
    static const juce::ParameterID shift    { "shift", 1 };
    static const juce::ParameterID freeze   { "freeze", 1 };
}

static const char* const curveKeys[sd::kNumCurves] = { "delay", "feedback", "level", "pan" };

//==============================================================================
SpectralDelayProcessor::SpectralDelayProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SpectralDelay", createLayout())
{
    for (auto& c : curves)
        for (auto& v : c)
            v.store (0.0f);

    core = std::make_unique<sd::SpectralDelayCore>();
    timeParam     = apvts.getRawParameterValue ("time");
    feedbackParam = apvts.getRawParameterValue ("feedback");
    mixParam      = apvts.getRawParameterValue ("drywet");
    shiftParam    = apvts.getRawParameterValue ("shift");
    freezeParam   = apvts.getRawParameterValue ("freeze");

    setLatencySamples (sd::SpectralDelayCore::getLatencySamples());
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectralDelayProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::NormalisableRange<float> timeRange (10.0f, 2500.0f, 0.1f);
    timeRange.setSkewForCentre (300.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ids::time, "Time", timeRange, 375.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));

    auto percent = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ids::feedback, "Feedback", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percent)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ids::drywet, "Dry/Wet", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percent)));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        ids::shift, "Shift", -24, 24, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction ([this] (int v, int)
        {
            const double hz = 2.0 * v * currentSampleRate.load() / sd::kFFTSize;
            return (v > 0 ? "+" : "") + juce::String (v) + "  (" + (hz > 0 ? "+" : "") + juce::String (juce::roundToInt (hz)) + " Hz)";
        })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        ids::freeze, "Freeze", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "On" : "Off"; })));

    return layout;
}

//==============================================================================
void SpectralDelayProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
    core->prepare (sampleRate);
    setLatencySamples (sd::SpectralDelayCore::getLatencySamples());
}

bool SpectralDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SpectralDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    for (int c = 0; c < sd::kNumCurves; ++c)
    {
        for (int i = 0; i < sd::kCurvePoints; ++i)
            curveScratch[i] = curves[(size_t) c][(size_t) i].load (std::memory_order_relaxed);
        core->setCurve (c, curveScratch);
    }

    sd::Params p;
    p.timeMs   = timeParam->load();
    p.feedback = feedbackParam->load() * 0.01f;
    p.dryWet   = mixParam->load() * 0.01f;
    p.shift    = shiftParam->load();
    p.freeze   = freezeParam->load() > 0.5f;

    core->process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, p);
}

//==============================================================================
float SpectralDelayProcessor::getCurvePoint (int curve, int index) const
{
    return curves[(size_t) curve][(size_t) index].load (std::memory_order_relaxed);
}

void SpectralDelayProcessor::setCurvePoint (int curve, int index, float value)
{
    const auto r = sd::kCurveRanges[curve];
    curves[(size_t) curve][(size_t) index].store (juce::jlimit (r.lo, r.hi, value), std::memory_order_relaxed);
    ++curveVersion;
}

void SpectralDelayProcessor::resetCurve (int curve)
{
    for (int i = 0; i < sd::kCurvePoints; ++i)
        curves[(size_t) curve][(size_t) i].store (0.0f, std::memory_order_relaxed);
    ++curveVersion;
}

void SpectralDelayProcessor::applyCurvePreset (CurvePreset preset)
{
    const int n = sd::kCurvePoints;
    switch (preset)
    {
        case CurvePreset::Rise:
            for (int i = 0; i < n; ++i) setCurvePoint (sd::kCurveDelay, i, -0.9f + 1.8f * (float) i / (float) (n - 1));
            break;
        case CurvePreset::Fall:
            for (int i = 0; i < n; ++i) setCurvePoint (sd::kCurveDelay, i, 0.9f - 1.8f * (float) i / (float) (n - 1));
            break;
        case CurvePreset::RandomDelay:
            for (int i = 0; i < n; ++i) setCurvePoint (sd::kCurveDelay, i, random.nextFloat() * 2.0f - 1.0f);
            break;
        case CurvePreset::RandomPan:
            for (int i = 0; i < n; ++i) setCurvePoint (sd::kCurvePan, i, random.nextFloat() * 2.0f - 1.0f);
            break;
        case CurvePreset::ResetAll:
            for (int c = 0; c < sd::kNumCurves; ++c) resetCurve (c);
            break;
    }
}

//==============================================================================
void SpectralDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree curveTree ("CURVES");
    for (int c = 0; c < sd::kNumCurves; ++c)
    {
        juce::StringArray values;
        for (int i = 0; i < sd::kCurvePoints; ++i)
            values.add (juce::String (getCurvePoint (c, i), 4));
        curveTree.setProperty (curveKeys[c], values.joinIntoString (" "), nullptr);
    }
    state.appendChild (curveTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SpectralDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (auto curveTree = tree.getChildWithName ("CURVES"); curveTree.isValid())
    {
        for (int c = 0; c < sd::kNumCurves; ++c)
        {
            auto tokens = juce::StringArray::fromTokens (curveTree.getProperty (curveKeys[c]).toString(), " ", "");
            for (int i = 0; i < sd::kCurvePoints; ++i)
                setCurvePoint (c, i, i < tokens.size() ? tokens[i].getFloatValue() : 0.0f);
        }
        tree.removeChild (curveTree, nullptr);
    }
    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessorEditor* SpectralDelayProcessor::createEditor()
{
    return new SpectralDelayEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralDelayProcessor();
}
