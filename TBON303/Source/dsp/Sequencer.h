#pragma once

// 16-step acid sequencer with variable loop length (polyrhythm against the bar),
// swing, and 303 gate/slide semantics. Position comes in as an absolute step count,
// so a 15-step loop drifts against the host's bar deterministically.

#include <cmath>
#include <cstdint>

namespace s303
{
constexpr int kMaxSteps = 16;

struct Step
{
    int note = 0;    // semitone 0..12 above the root
    int octave = 0;  // -1, 0, +1
    bool gate = true;
    bool accent = false;
    bool slide = false;
};

struct Pattern
{
    Step steps[kMaxSteps];
    int length = 16;
};

struct SeqEvent
{
    enum Type { None, NoteOn, NoteOff } type = None;
    int note = 0;
    bool accent = false;
    bool slide = false;
};

class StepSequencer
{
public:
    void reset()
    {
        lastStep = INT64_MIN;
        holding = false;
        releaseAtGateEnd = false;
        currentIndex = -1;
    }

    /**
        posSteps: absolute position in steps (e.g. ppq * 4 for 16ths).
        swing 0..1 delays every second step by up to a quarter of a step pair.
        gateLength 0..1 is the fraction of a step a non-sliding note is held.
    */
    SeqEvent tick (double posSteps, const Pattern& pat, float swing, float gateLength, int baseNote)
    {
        SeqEvent ev;
        const int len = pat.length < 1 ? 1 : (pat.length > kMaxSteps ? kMaxSteps : pat.length);

        const double pairPos = posSteps * 0.5;
        const double pair = std::floor (pairPos);
        const double frac = pairPos - pair;
        const double split = 0.5 + 0.25 * swing;

        int64_t k;
        double local;
        if (frac < split)
        {
            k = (int64_t) pair * 2;
            local = frac / split;
        }
        else
        {
            k = (int64_t) pair * 2 + 1;
            local = (frac - split) / (1.0 - split);
        }

        if (k != lastStep)
        {
            lastStep = k;
            const int idx = wrap (k, len);
            const Step& s = pat.steps[idx];
            const Step& prev = pat.steps[wrap (k - 1, len)];
            const Step& next = pat.steps[wrap (k + 1, len)];
            currentIndex = idx;

            if (s.gate)
            {
                ev.type = SeqEvent::NoteOn;
                ev.note = baseNote + s.note + 12 * s.octave;
                ev.accent = s.accent;
                ev.slide = holding && prev.gate && prev.slide;
                holding = true;
                // A slide step keeps the gate high into the next note.
                releaseAtGateEnd = ! (s.slide && next.gate);
            }
            else if (holding)
            {
                ev.type = SeqEvent::NoteOff;
                holding = false;
            }
        }
        else if (holding && releaseAtGateEnd && local >= gateLength)
        {
            ev.type = SeqEvent::NoteOff;
            holding = false;
        }

        return ev;
    }

    /** Stop sounding (transport stopped). Returns true if a note-off is needed. */
    bool stop()
    {
        const bool wasHolding = holding;
        reset();
        return wasHolding;
    }

    int getCurrentIndex() const { return currentIndex; }

private:
    static int wrap (int64_t k, int len)
    {
        const int64_t m = k % len;
        return (int) (m < 0 ? m + len : m);
    }

    int64_t lastStep = INT64_MIN;
    bool holding = false, releaseAtGateEnd = false;
    int currentIndex = -1;
};
} // namespace s303
