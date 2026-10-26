// Loads the KUBE VST3 like a DAW would: checks it registers as a SKAZI instrument, plays notes,
// survives a state save/restore and opens its editor.
//
// Usage: KubeHostTest "<path to KUBE.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

// Peak of `seconds` of audio, with a chord held from the start when play is set
float render (juce::AudioPluginInstance& p, double seconds, bool play)
{
    constexpr int block = 512;
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), block);
    float peak = 0.0f;
    const int total = (int) (seconds * 44100.0);
    for (int done = 0; done < total; done += block)
    {
        juce::MidiBuffer midi;
        if (play && done == 0)
            for (int n : { 45, 52, 57 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 110), 0);
        if (play && done + block >= total)
            for (int n : { 45, 52, 57 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < block; ++i)
                if (! std::isfinite (buffer.getSample (c, i))) return 99.0f;
        peak = juce::jmax (peak, buffer.getMagnitude (0, block));
    }
    return peak;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: KubeHostTest <KUBE.vst3>\n");
        return 2;
    }

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, juce::String::fromUTF8 (argv[1]));
    check (found.size() == 1, "VST3 scanned (" + juce::String (found.size()) + " plugin found)");
    if (found.isEmpty())
        return 1;

    const auto& desc = *found[0];
    std::printf ("       %s by %s, %s\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8());
    check (desc.name == "KUBE", "name is KUBE");
    check (desc.manufacturerName == "SKAZI", "vendor is SKAZI (listed with the SKAZI Bundle)");
    check (desc.isInstrument, "registered as an instrument");

    juce::String error;
    auto plugin = format.createInstanceFromDescription (desc, 44100.0, 512, error);
    check (plugin != nullptr, "instantiated " + error);
    if (plugin == nullptr)
        return 1;

    check (plugin->acceptsMidi(), "accepts MIDI");
    plugin->prepareToPlay (44100.0, 512);

    check (render (*plugin, 0.3, false) < 1.0e-6f, "silent with no MIDI");
    const float peak = render (*plugin, 0.8, true);
    check (peak > 0.05f && peak <= 1.0f, "a chord plays (peak " + juce::String (peak, 2) + ")");
    render (*plugin, 3.0, false);
    check (render (*plugin, 0.5, false) < 0.001f, "notes die away after release");

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    plugin->setStateInformation (state.getData(), (int) state.getSize());
    check (state.getSize() > 100, "state saves and restores (" + juce::String ((int) state.getSize()) + " bytes)");
    check (render (*plugin, 0.5, true) > 0.05f, "still plays after restoring state");

    std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    check (editor != nullptr && editor->getWidth() > 0, "editor opens");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    editor.reset();

    plugin->releaseResources();
    plugin.reset();
    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
