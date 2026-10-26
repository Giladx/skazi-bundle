// Renders the QUAZI editor to a PNG without a host, after playing a beat through it so the screens have something to show.
//   QuaziShot <out.png> [preset index | random]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "quazi.png");

    QuaziProcessor proc;
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 2)
    {
        if (juce::String (argv[2]) == "random") proc.randomizeAll();
        else proc.loadPreset (juce::String (argv[2]).getIntValue());
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < (int) sr * 3; pos += 512)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (pos + i) / sr, inBeat = std::fmod (t, 0.5);
            const float x = (float) (std::sin (juce::MathConstants<double>::twoPi * (50.0 * inBeat + 3.6 * (1.0 - std::exp (-inBeat / 0.03))))
                                     * std::exp (-inBeat / 0.12)) * 0.6f
                          + (float) (2.0 * std::fmod (55.0 * t, 1.0) - 1.0) * 0.1f;
            buf.setSample (0, i, x); buf.setSample (1, i, x);
        }
        proc.processBlock (buf, midi);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

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
