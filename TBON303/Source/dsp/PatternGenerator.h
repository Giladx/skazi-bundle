#pragma once

// Rule-based acid line generator: random but musical. Notes stay in a scale, lean
// on the root and fifth, mostly move by small steps, and accents/slides land where
// they groove (accents favour off-beats, slides only into a following note).

#include "Sequencer.h"
#include <algorithm>
#include <random>
#include <vector>

namespace s303
{
struct ScaleDef
{
    const char* name;
    std::vector<int> degrees; // semitones in one octave, starting with 0
};

inline const std::vector<ScaleDef>& scales()
{
    static const std::vector<ScaleDef> list = {
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Minor Penta",    { 0, 3, 5, 7, 10 } },
        { "Blues",          { 0, 3, 5, 6, 7, 10 } },
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Phrygian Dom",   { 0, 1, 4, 5, 7, 8, 10 } },
    };
    return list;
}

struct GenSettings
{
    int scale = 0;
    float density = 0.7f;  // probability of a step sounding
    float accent = 0.3f;
    float slide = 0.2f;
    float octave = 0.25f;  // chance of an octave jump
};

class PatternGenerator
{
public:
    PatternGenerator() : rng (std::random_device {}()) {}
    explicit PatternGenerator (uint32_t seed) : rng (seed) {}

    void generate (Pattern& pat, const GenSettings& g)
    {
        const auto pool = notePool (g.scale);
        int prevIdx = 0;

        for (int i = 0; i < kMaxSteps; ++i)
        {
            Step& s = pat.steps[i];

            // Downbeats are much more likely to play; step 1 always does.
            double pGate = g.density;
            if (i % 4 == 0) pGate = std::min (1.0, pGate + 0.35);
            else if (i % 2 == 1) pGate *= 0.9;
            s.gate = (i == 0) || chance (pGate);

            // Melody: mostly small moves through the scale, with pulls back to the root.
            int idx;
            if (i == 0 || chance (0.3))
                idx = weightedPick (pool);
            else
            {
                const int move = pick ({ -2, -1, -1, 0, 0, 1, 1, 2 });
                idx = std::clamp (prevIdx + move, 0, (int) pool.size() - 1);
            }
            prevIdx = idx;
            s.note = pool[(size_t) idx];

            s.octave = 0;
            if (i != 0 && chance (g.octave))
                s.octave = chance (0.7) ? 1 : -1;
            if (s.note == 12 && s.octave == 1)
                s.octave = 0;

            const double accentBias = (i % 2 == 1) ? 1.35 : (i % 4 == 0 ? 0.8 : 1.0);
            s.accent = s.gate && chance (g.accent * accentBias);
            s.slide = false;
        }

        // Slides only make sense into a following note.
        for (int i = 0; i < kMaxSteps; ++i)
        {
            const Step& next = pat.steps[(i + 1) % kMaxSteps];
            Step& s = pat.steps[i];
            s.slide = s.gate && next.gate && chance (g.slide);
        }
    }

    /** Re-roll a few attributes on a few steps, keeping the line recognisable. */
    void mutate (Pattern& pat, const GenSettings& g)
    {
        const auto pool = notePool (g.scale);
        const int changes = 2 + (int) (rng() % 3);
        for (int c = 0; c < changes; ++c)
        {
            Step& s = pat.steps[rng() % kMaxSteps];
            switch (rng() % 5)
            {
                case 0: s.note = pool[(size_t) weightedPick (pool)]; break;
                case 1: s.accent = ! s.accent; break;
                case 2: s.slide = ! s.slide; break;
                case 3: s.octave = (int) (rng() % 3) - 1; break;
                default: s.gate = ! s.gate; break;
            }
        }
    }

private:
    static std::vector<int> notePool (int scaleIndex)
    {
        const auto& all = scales();
        const auto& sc = all[(size_t) std::clamp (scaleIndex, 0, (int) all.size() - 1)];
        auto pool = sc.degrees;
        pool.push_back (12);
        return pool;
    }

    int weightedPick (const std::vector<int>& pool)
    {
        std::vector<double> w;
        for (int n : pool)
            w.push_back (n == 0 ? 4.0 : n == 12 ? 1.5 : n == 7 ? 2.0 : 1.0);
        std::discrete_distribution<int> d (w.begin(), w.end());
        return d (rng);
    }

    int pick (std::initializer_list<int> v)
    {
        return *(v.begin() + (rng() % v.size()));
    }

    bool chance (double p) { return std::uniform_real_distribution<double> (0.0, 1.0) (rng) < p; }

    std::mt19937 rng;
};
} // namespace s303
