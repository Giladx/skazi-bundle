#pragma once
#include <atomic>
#include <cstdint>
#include <random>
#include <juce_core/juce_core.h>

constexpr int kNumPatterns = 8;
constexpr int kNumSteps = 16;

// Pattern data is shared between the editor (writes) and the audio thread
// (reads), so every field is a relaxed atomic.
struct BassStep
{
    std::atomic<int8_t> note { 0 };    // 0..12 (C..C)
    std::atomic<int8_t> octave { 1 };  // 0 = down, 1 = mid, 2 = up
    std::atomic<int8_t> gate { 0 };
    std::atomic<int8_t> accent { 0 };
    std::atomic<int8_t> slide { 0 };

    int midiNote() const { return 36 + note.load() + (octave.load() - 1) * 12; }
};

struct BassBank
{
    BassStep steps[kNumPatterns][kNumSteps];

    void clear (int p)
    {
        for (auto& s : steps[p])
        {
            s.note = 0; s.octave = 1; s.gate = 0; s.accent = 0; s.slide = 0;
        }
    }

    void randomise (int p, uint32_t seed)
    {
        std::mt19937 rng (seed);
        auto chance = [&rng] (int pct) { return (int) (rng() % 100) < pct; };
        static const int scale[] = { 0, 0, 0, 3, 5, 7, 10, 12, 12 };
        for (auto& s : steps[p])
        {
            s.gate = chance (78) ? 1 : 0;
            s.note = (int8_t) scale[rng() % 9];
            s.octave = (int8_t) (chance (20) ? 2 : (chance (12) ? 0 : 1));
            s.accent = chance (28) ? 1 : 0;
            s.slide = chance (22) ? 1 : 0;
        }
    }

    // Rolling arpeggio-style line built on a minor-seventh chord
    void randomiseArp (int p, uint32_t seed)
    {
        std::mt19937 rng (seed);
        auto chance = [&rng] (int pct) { return (int) (rng() % 100) < pct; };
        static const int chord[] = { 0, 3, 7, 10, 12 };
        const int dir = (int) (rng() % 3);
        for (int i = 0; i < kNumSteps; ++i)
        {
            auto& s = steps[p][i];
            int idx = dir == 0 ? i % 5 : (dir == 1 ? 4 - (i % 5) : (int) (rng() % 5));
            s.gate = chance (88) ? 1 : 0;
            s.note = (int8_t) chord[idx];
            s.octave = (int8_t) ((i % 8) >= 6 && chance (60) ? 2 : 1);
            s.accent = (i % 4 == 0 && chance (50)) ? 1 : 0;
            s.slide = chance (6) ? 1 : 0;
        }
    }

    juce::String serialise() const
    {
        juce::String out;
        for (auto& pat : steps)
            for (auto& s : pat)
            {
                out << juce::String::toHexString ((int) s.note.load());
                out << (int) s.octave.load() << (int) s.gate.load() << (int) s.accent.load() << (int) s.slide.load();
            }
        return out;
    }

    void deserialise (const juce::String& str)
    {
        if (str.length() < kNumPatterns * kNumSteps * 5) return;
        int i = 0;
        for (auto& pat : steps)
            for (auto& s : pat)
            {
                s.note = (int8_t) juce::jlimit (0, 12, str.substring (i, i + 1).getHexValue32());
                s.octave = (int8_t) juce::jlimit (0, 2, (int) str[i + 1] - '0');
                s.gate = (int8_t) (str[i + 2] == '1');
                s.accent = (int8_t) (str[i + 3] == '1');
                s.slide = (int8_t) (str[i + 4] == '1');
                i += 5;
            }
    }
};

