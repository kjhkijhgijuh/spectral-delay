// Loads the built plugin (VST3 or AU) like a DAW would and checks it end to end.
// usage: host_test <path to .vst3 or .component>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "ok:  " : "FAIL:", what.toRawUTF8());
    if (! ok) ++failures;
}

static juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name)
            return prm;
    return nullptr;
}

int main (int argc, char* argv[])
{
    if (argc < 2) { std::printf ("usage: host_test <plugin>\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);

    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* f : fm.getFormats())
        f->findAllTypesForFile (types, juce::String::fromUTF8 (argv[1]));
    check (types.size() == 1, "plugin found (" + juce::String (types.size()) + " type)");
    if (types.isEmpty()) return 1;
    std::printf ("      format %s, name '%s'\n", types[0]->pluginFormatName.toRawUTF8(), types[0]->name.toRawUTF8());

    const double sr = 48000.0;
    juce::String error;
    auto plugin = fm.createPluginInstance (*types[0], sr, 512, error);
    check (plugin != nullptr, "instance created " + error);
    if (plugin == nullptr) return 1;

    plugin->setPlayConfigDetails (2, 2, sr, 1024);
    plugin->prepareToPlay (sr, 1024);
    check (plugin->getLatencySamples() == 2048, "latency = " + juce::String (plugin->getLatencySamples()));
    check (plugin->getParameters().size() >= 5, "parameters (incl. host bypass) = " + juce::String (plugin->getParameters().size()));

    auto* time = findParam (*plugin, "Time");
    auto* fb   = findParam (*plugin, "Feedback");
    auto* mix  = findParam (*plugin, "Dry/Wet");
    check (time && fb && mix && findParam (*plugin, "Shift") && findParam (*plugin, "Freeze"), "parameter names");
    if (! (time && fb && mix)) return 1;
    std::printf ("      Time text '%s', Shift text '%s'\n", time->getCurrentValueAsText().toRawUTF8(),
                 findParam (*plugin, "Shift")->getCurrentValueAsText().toRawUTF8());

    // impulse: dry/wet 100 %, feedback 50 %, ~213.3 ms (= 20 FFT hops at 48 kHz)
    mix->setValueNotifyingHost (1.0f);                          // 0..100 % -> 100 %
    fb->setValueNotifyingHost (0.5f);                           // 0..100 % -> 50 %
    time->setValueNotifyingHost (time->getValueForText ("213.3")); // skewed range: let the plugin convert
    std::printf ("      Time set to '%s'\n", time->getCurrentValueAsText().toRawUTF8());

    juce::MidiBuffer midi;
    const int total = 48000 * 2;
    std::vector<float> outL;
    outL.reserve ((size_t) total);
    juce::Random rnd (7);
    int pos = 0;
    bool finite = true;
    while (pos < total)
    {
        const int n = juce::jmin (1 + rnd.nextInt (1024), total - pos);
        juce::AudioBuffer<float> buf (2, n);
        buf.clear();
        if (pos <= 10000 && 10000 < pos + n)
        {
            buf.setSample (0, 10000 - pos, 1.0f);
            buf.setSample (1, 10000 - pos, 1.0f);
        }
        plugin->processBlock (buf, midi);
        for (int i = 0; i < n; ++i)
        {
            finite = finite && std::isfinite (buf.getSample (0, i)) && std::isfinite (buf.getSample (1, i));
            outL.push_back (buf.getSample (0, i));
        }
        pos += n;
    }
    check (finite, "output finite with random block sizes");
    auto argmaxNear = [&] (int c) { int best = c; for (int i = c - 300; i < c + 300; ++i) if (std::abs (outL[(size_t) i]) > std::abs (outL[(size_t) best])) best = i; return best; };
    const int e1 = argmaxNear (10000 + 2048 + 10240), e2 = argmaxNear (10000 + 2048 + 20480);
    check (e1 == 10000 + 2048 + 10240 && std::abs (outL[(size_t) e1] - 1.0f) < 0.02f,
           "1st echo at " + juce::String (e1 - 10000) + " samples, level " + juce::String (outL[(size_t) e1], 3));
    check (e2 == 10000 + 2048 + 20480 && std::abs (outL[(size_t) e2] - 0.5f) < 0.02f,
           "2nd echo at " + juce::String (e2 - 10000) + " samples, level " + juce::String (outL[(size_t) e2], 3));

    // state round trip into a second instance
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    auto second = fm.createPluginInstance (*types[0], sr, 512, error);
    check (second != nullptr, "second instance");
    if (second != nullptr)
    {
        second->setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock state2;
        second->getStateInformation (state2);
        check (state.getSize() > 0 && state == state2, "state save/restore round trip (" + juce::String ((int) state.getSize()) + " bytes)");
        auto* t2 = findParam (*second, "Time");
        check (t2 != nullptr && std::abs (t2->getValue() - time->getValue()) < 1e-4f, "restored Time = " + (t2 ? t2->getCurrentValueAsText() : juce::String()));
    }

    // editor
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (plugin->createEditorAndMakeActive());
        check (ed != nullptr, "editor created");
        if (ed != nullptr)
        {
            check (ed->getWidth() == 760 && ed->getHeight() == 452,
                   "editor size " + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
            juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
            juce::Graphics g (img);
            ed->paintEntireComponent (g, true);
            if (argc > 2)
            {
                juce::File out (juce::String::fromUTF8 (argv[2]));
                out.deleteFile();
                juce::FileOutputStream os (out);
                juce::PNGImageFormat().writeImageToStream (img, os);
            }
            plugin->editorBeingDeleted (ed.get());
        }
    }

    plugin->releaseResources();
    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nall host checks passed\n", failures);
    return failures ? 1 : 0;
}
