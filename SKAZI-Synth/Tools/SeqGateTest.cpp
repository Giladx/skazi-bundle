// Checks the SKAZI sequencer never plays by itself while the DAW transport runs:
// it must be silent with no keys, play while a key is held, and stop when the key is released.
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <cstdio>

namespace
{
    // A host transport that is playing at 140 bpm
    struct PlayingHost : juce::AudioPlayHead
    {
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (140.0);
            p.setIsPlaying (true);
            p.setPpqPosition (ppq);
            return p;
        }
    };

    int failures = 0;
    void expect (bool ok, const char* what, float level)
    {
        if (! ok) ++failures;
        std::printf ("%-52s %s  level %.4f\n", what, ok ? "OK  " : "FAIL", level);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    AcidanceProcessor proc;
    PlayingHost host;
    proc.setPlayHead (&host);
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // a sequencer preset: SEQ switched on
    for (int i = 0; i < Presets::count(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.fader.flush(); // no message loop here: apply the preset now
        if (proc.apvts.getRawParameterValue ("seq_on")->load() > 0.5f) break;
    }
    std::printf ("preset: %s\n", proc.currentPresetName.toRawUTF8());

    juce::AudioBuffer<float> buf (2, 512);
    auto run = [&] (int blocks, juce::MidiBuffer first)
    {
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) midi = first;
            proc.processBlock (buf, midi);
            host.ppq += 512.0 / 48000.0 * 140.0 / 60.0;
            if (b >= blocks / 2) peak = std::max (peak, buf.getMagnitude (0, 0, 512));
        }
        return peak;
    };

    const float alone = run (400, {});
    expect (alone < 1.0e-4f, "song playing, no keys: sequencer stays silent", alone);

    juce::MidiBuffer down;
    down.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
    const float held = run (200, down);
    expect (held > 0.01f, "key held: the pattern plays", held);

    juce::MidiBuffer up;
    up.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    const float released = run (400, up);
    expect (released < 1.0e-3f, "key released: the pattern stops", released);

    // RUN must not make it play by itself either
    proc.apvts.getParameter ("run")->setValueNotifyingHost (1.0f);
    const float runInDaw = run (400, {});
    expect (runInDaw < 1.0e-4f, "RUN on, no MIDI: still silent", runInDaw);

    // a MIDI clip from the DAW (note on ... note off) plays it, then it stops again
    juce::MidiBuffer clip;
    clip.addEvent (juce::MidiMessage::noteOn (1, 62, 0.8f), 100);
    const float clipPlays = run (100, clip);
    expect (clipPlays > 0.01f, "MIDI note from a DAW clip plays the pattern", clipPlays);
    juce::MidiBuffer clipEnd;
    clipEnd.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
    const float clipStops = run (400, clipEnd);
    expect (clipStops < 1.0e-3f, "clip note ends: silent again", clipStops);

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    proc.setPlayHead (nullptr);
    return failures == 0 ? 0 : 1;
}
