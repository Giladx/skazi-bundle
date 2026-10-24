#pragma once

// BULLY arpeggiator: turns held notes into a tempo-synced pattern before they reach the engine.
// Follows the song position when the host plays, otherwise runs on its own clock from the first key.
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../Common/SkaziCore.h"
#include "Params.h"

namespace bully
{
class Arp
{
public:
    struct Settings { bool on = false; int mode = AUp, rate = 4, octaves = 1; float gate = 0.6f, swing = 0.0f; };

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { held.clear(); sounding.clear(); lastStep = -1; freeBeats = 0.0; pos = 0; dir = 1; }

    // Rewrites `midi` in place. Call every block, on or off (turning it off releases its notes).
    void process (juce::MidiBuffer& midi, int numSamples, const Settings& s, const skazi::HostTime& ht)
    {
        if (! s.on)
        {
            if (! sounding.isEmpty() || ! held.isEmpty())
            {
                juce::MidiBuffer out;
                for (auto& n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n.note), 0);
                out.addEvents (midi, 0, numSamples, 0);
                midi.swapWith (out);
                reset();
            }
            return;
        }

        juce::MidiBuffer out;
        const double stepBeats = skazi::noteBeats (s.rate);
        const double beatsPerSample = ht.bpm / 60.0 / sr;
        auto it = midi.cbegin();
        for (int i = 0; i < numSamples; ++i)
        {
            for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
            {
                const auto m = (*it).getMessage();
                if (m.isNoteOn())
                {
                    if (held.isEmpty()) { freeBeats = 0.0; lastStep = -1; pos = 0; dir = 1; }
                    held.removeIf ([&m] (const Held& h) { return h.note == m.getNoteNumber(); });
                    held.add ({ m.getNoteNumber(), m.getFloatVelocity() });
                }
                else if (m.isNoteOff())
                    held.removeIf ([&m] (const Held& h) { return h.note == m.getNoteNumber(); });
                else
                    out.addEvent (m, i);
            }

            // finish notes whose gate ran out
            for (int k = sounding.size(); --k >= 0;)
                if (--sounding.getReference (k).samplesLeft <= 0)
                {
                    out.addEvent (juce::MidiMessage::noteOff (1, sounding[k].note), i);
                    sounding.remove (k);
                }

            if (held.isEmpty())
            {
                for (auto& n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n.note), i);
                sounding.clear();
                continue;
            }

            const double beat = ht.playing ? ht.ppq + i * beatsPerSample : freeBeats;
            if (! ht.playing) freeBeats += beatsPerSample;
            const double pair = 2.0 * stepBeats, inPair = std::fmod (beat, pair);
            const auto step = (juce::int64) std::floor (beat / pair) * 2 + (inPair < stepBeats * (1.0 + s.swing) ? 0 : 1);
            if (step == lastStep) continue;
            lastStep = step;

            for (auto& n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n.note), i);
            sounding.clear();
            const int gateSamples = juce::jmax (8, (int) (s.gate * stepBeats / beatsPerSample));
            for (auto& n : nextNotes (s))
            {
                out.addEvent (juce::MidiMessage::noteOn (1, n.note, juce::jlimit (0.05f, 1.0f, n.velocity)), i);
                sounding.add ({ n.note, gateSamples });
            }
        }
        midi.swapWith (out);
    }

private:
    struct Held { int note; float velocity; };
    struct Playing { int note; int samplesLeft; };

    juce::Array<Held> nextNotes (const Settings& s)
    {
        juce::Array<Held> order (held);
        if (s.mode != AAsPlayed)
            std::sort (order.begin(), order.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
        const int oct = juce::jlimit (1, 4, s.octaves);
        juce::Array<Held> seq;
        for (int o = 0; o < oct; ++o)
            for (auto h : order)
                if (h.note + 12 * o <= 127) seq.add ({ h.note + 12 * o, h.velocity });
        if (seq.isEmpty()) return {};
        const int n = seq.size();

        if (s.mode == AChord)
        {
            juce::Array<Held> chord;
            const int o = (int) (pos++ % oct);
            for (auto h : order) if (h.note + 12 * o <= 127) chord.add ({ h.note + 12 * o, h.velocity });
            return chord;
        }
        int idx = 0;
        switch (s.mode)
        {
            case ADown:   idx = n - 1 - (int) (pos++ % n); break;
            case ARandom: idx = rng.nextInt (n); break;
            case AUpDown:
            {
                if (n == 1) { idx = 0; break; }
                const int cycle = 2 * n - 2, k = (int) (pos++ % cycle);
                idx = k < n ? k : cycle - k;
                break;
            }
            default:      idx = (int) (pos++ % n); break; // UP, AS PLAYED
        }
        return { seq[idx] };
    }

    double sr = 44100.0, freeBeats = 0.0;
    juce::Array<Held> held;
    juce::Array<Playing> sounding;
    juce::int64 lastStep = -1, pos = 0;
    int dir = 1;
    juce::Random rng;
};
} // namespace bully
