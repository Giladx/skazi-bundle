// Renders the BAMBA editor to a PNG without a host, for checking the skin.
//   BambaSkinShot <out.png> [preset name|-] [sample.wav|-] [play]
// With "play" a note is rendered first, so the playhead shows partway through the effect.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File out = cwd.getChildFile (argc > 1 ? argv[1] : "bamba.png");

    BambaProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Let the riser library finish scanning first, so SPLICE presets can pick a sample.
    for (int i = 0; i < 600 && processor.library->isScanning(); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    if (argc > 2 && juce::String (argv[2]) != "-")
    {
        const auto& list = processor.presetList();
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].name == juce::String (argv[2]))
                processor.loadPreset (i);
    }
    if (argc > 3 && juce::String (argv[3]) != "-" && ! processor.loadSample (cwd.getChildFile (argv[3])))
    {
        std::fprintf (stderr, "could not load %s\n", argv[3]);
        return 1;
    }

    // Run the preset fade, and optionally play into the effect so the playhead shows.
    juce::AudioBuffer<float> buf (2, 512);
    const bool play = argc > 4 && juce::String (argv[4]) == "play";
    for (int b = 0; b < 3000; ++b)
    {
        juce::MidiBuffer midi;
        if (play && b == 40) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        processor.processBlock (buf, midi);
        if (b == 40 && ! play) break;
        if (b > 40 && processor.engine.uiProgress.load() > 0.6f) break;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("wrote %s (%dx%d)  %s  sample: %s  library: %d\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight(),
                 processor.getPresetLabel().toRawUTF8(), processor.getSampleFile().getFileName().toRawUTF8(),
                 (int) processor.library->getEntries().size());
    editor.reset();
    return 0;
}
