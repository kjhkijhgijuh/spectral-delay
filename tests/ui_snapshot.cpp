// Renders the plugin editor to a PNG (for checking the look without a DAW).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    SpectralDelayProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.applyCurvePreset (SpectralDelayProcessor::CurvePreset::Rise);
    proc.applyCurvePreset (SpectralDelayProcessor::CurvePreset::RandomPan);
    for (int i = 0; i < sd::kCurvePoints; ++i)
    {
        proc.setCurvePoint (sd::kCurveFeedback, i, 0.3f * std::sin ((float) i * 0.2f));
        proc.setCurvePoint (sd::kCurveLevel, i, i > 48 ? -48.0f : -2.0f * (float) (i % 8));
    }
    if (auto* p = proc.apvts.getParameter ("freeze")) p->setValueNotifyingHost (1.0f);
    if (auto* p = proc.apvts.getParameter ("shift"))  p->setValueNotifyingHost (p->convertTo0to1 (3.0f));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    juce::Image img (juce::Image::ARGB, ed->getWidth() * 2, ed->getHeight() * 2, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        ed->paintEntireComponent (g, true);
    }
    juce::File out (juce::String::fromUTF8 (argc > 1 ? argv[1] : "ui.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
