// Renders the TAHINI editor to a PNG without a host, after playing a beat through it so the screens have something to show.
//   TahiniShot <out.png> [factory preset index] [out.wav: 8 s of a psy stab through the preset]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "tahini.png");

    TahiniProcessor proc;
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 2)
        proc.loadPreset (juce::String (argv[2]).getIntValue());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < (int) sr * 3; pos += 512)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (pos + i) / sr, inBeat = std::fmod (t, 1.0);
            const float x = inBeat < 0.12 ? (float) (std::sin (juce::MathConstants<double>::twoPi * 330.0 * t) * std::exp (-inBeat / 0.04)) * 0.5f : 0.0f;
            buf.setSample (0, i, x); buf.setSample (1, i, x);
        }
        proc.processBlock (buf, midi);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    if (argc > 3)
    {
        // a short saw chord stab every bar (120 bpm, no host), twice, then silence for the tail
        proc.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> all (2, (int) sr * 8);
        all.clear();
        for (int pos = 0; pos < all.getNumSamples(); pos += 512)
        {
            const int len = juce::jmin (512, all.getNumSamples() - pos);
            juce::AudioBuffer<float> blk (2, len);
            for (int i = 0; i < len; ++i)
            {
                const double t = (pos + i) / sr, inBar = std::fmod (t, 2.0);
                float x = 0.0f;
                if (t < 4.0 && inBar < 0.18)
                    for (double hz : { 220.0, 330.0, 440.0 * 1.189 })
                        x += (float) (2.0 * std::fmod (hz * t, 1.0) - 1.0) * 0.12f * (float) std::exp (-inBar / 0.06);
                blk.setSample (0, i, x); blk.setSample (1, i, x);
            }
            proc.processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, blk, c, 0, len);
        }
        const juce::File wav = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        std::printf ("wrote %s\n", wav.getFullPathName().toRawUTF8());
    }

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
