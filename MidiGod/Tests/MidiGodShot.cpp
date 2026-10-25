// Renders the MIDI GOD editor to a PNG without a host, after letting the loop play for a moment.
//   MidiGodShot <out.png> [factory preset index] [audio file to convert]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "midigod.png");

    MidiGodProcessor proc;
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 2) proc.loadPreset (juce::String (argv[2]).getIntValue());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    if (argc > 3)
    {
        proc.convertAudio (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]));
        for (int i = 0; i < 200 && proc.isAnalysing(); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < (int) sr * 2; pos += 512)
    {
        midi.clear();
        proc.processBlock (buf, midi);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    return 0;
}
