#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SpectralDelayCore.h"

class SpectralDelayProcessor : public juce::AudioProcessor
{
public:
    enum class CurvePreset { Rise, Fall, RandomDelay, RandomPan, ResetAll };

    SpectralDelayProcessor();
    ~SpectralDelayProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Curves (shared with the editor; 0 = neutral for every curve)
    float getCurvePoint (int curve, int index) const;
    void setCurvePoint (int curve, int index, float value);
    void resetCurve (int curve);
    void applyCurvePreset (CurvePreset preset);
    int getCurveVersion() const { return curveVersion.load(); }

    double getCurrentSampleRate() const { return currentSampleRate.load(); }
    float getTimeMs() const { return timeParam->load(); }
    float getFeedbackPercent() const { return feedbackParam->load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<double> currentSampleRate { 48000.0 };   // declared before apvts (used by a text lambda)

public:
    juce::AudioProcessorValueTreeState apvts;

private:
    std::array<std::array<std::atomic<float>, sd::kCurvePoints>, sd::kNumCurves> curves;
    std::atomic<int> curveVersion { 0 };

    std::unique_ptr<sd::SpectralDelayCore> core;
    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* shiftParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    float curveScratch[sd::kCurvePoints] {};
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDelayProcessor)
};
