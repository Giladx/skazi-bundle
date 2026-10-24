// Loads a KICKASS VST3 like a DAW would and checks it shows up as a MIDI
// instrument, plays a kick from a note-on, and opens its editor.
//
// Usage: KickassHostTest "<path to KICKASS.vst3>"

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

/** Peak of `seconds` of audio, with a note-on at the start when note >= 0. */
float render (juce::AudioPluginInstance& p, double seconds, int note)
{
    constexpr int block = 512;
    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), block);
    float peak = 0.0f;
    for (int done = 0; done < (int) (seconds * 44100.0); done += block)
    {
        juce::MidiBuffer midi;
        if (done == 0 && note >= 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
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
        std::printf ("usage: KickassHostTest <KICKASS.vst3>\n");
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
    check (desc.name == "KICKASS", "name is KICKASS");
    check (desc.manufacturerName == "SKAZI", "vendor is SKAZI (listed with the SKAZI Bundle)");
    check (desc.isInstrument, "registered as an instrument");

    juce::String error;
    auto plugin = format.createInstanceFromDescription (desc, 44100.0, 512, error);
    check (plugin != nullptr, "instantiated " + error);
    if (plugin == nullptr)
        return 1;

    check (plugin->acceptsMidi(), "accepts MIDI");
    plugin->prepareToPlay (44100.0, 512);

    check (render (*plugin, 0.3, -1) < 1.0e-6f, "silent with no MIDI");
    const float peak = render (*plugin, 0.5, 36);
    check (peak > 0.2f && peak <= 1.0f, "note-on plays a kick (peak " + juce::String (peak, 2) + ")");
    render (*plugin, 2.5, -1); // let the tail ring out
    check (render (*plugin, 0.5, -1) < 0.001f, "kick dies away afterwards");

    std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    check (editor != nullptr && editor->getWidth() > 0, "editor opens");
    editor.reset();

    plugin->releaseResources();
    plugin.reset();
    std::printf (failures == 0 ? "ALL PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
