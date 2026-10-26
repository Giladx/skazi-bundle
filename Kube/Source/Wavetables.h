#pragma once

// Band-limited morphing wavetables: KUBE's own bank, built once and shared, plus tables loaded
// from Serum-style .wav files (one 2048-sample cycle per frame).
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <memory>
#include <vector>

namespace kube
{
constexpr int kTableSize = 2048, kLevels = 11, kBankFrames = 16, kBankSize = 24, kMaxUserFrames = 64;
constexpr int kUserTable = kBankSize; // the table choice after the bank: the oscillator's loaded .wav

const juce::StringArray& tableNames(); // the bank, then "USER"

class Wavetable
{
public:
    // cycles holds `frames` single cycles of kTableSize samples each, back to back
    Wavetable (const std::vector<float>& cycles, int frames, juce::String name, bool normalisePerFrame);

    // Mip level for a pitch in cycles per sample: level L keeps 1024 >> L harmonics. Anything that folds
    // back past Nyquist lands above a quarter of the sample rate, where it's masked by the real highs.
    static int levelFor (double cyclesPerSample)
    {
        if (cyclesPerSample <= 0.0) return 0;
        const int l = (int) std::ceil (std::log2 (2048.0 * cyclesPerSample / 1.5));
        return juce::jlimit (0, kLevels - 1, l);
    }

    int numFrames() const { return frames; }
    const juce::String& getName() const { return name; }
    const std::vector<float>& source() const { return cycles; } // what it was built from, for saving

    const float* frame (int f, int level) const
    {
        return data.data() + ((size_t) (f * kLevels + level)) * (kTableSize + 1);
    }

    // pos 0..1 morphs through the frames, phase 0..1 within the cycle
    float read (float pos, double phase, int level) const
    {
        const int last = frames - 1;
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) last;
        const int f0 = juce::jmin ((int) fp, juce::jmax (0, last - 1));
        const float ff = last > 0 ? fp - (float) f0 : 0.0f;
        const double idx = phase * kTableSize;
        const int i = juce::jlimit (0, kTableSize - 1, (int) idx);
        const float fr = (float) (idx - i);
        const float* a = frame (f0, level);
        const float va = a[i] + fr * (a[i + 1] - a[i]);
        if (last == 0) return va;
        const float* b = a + (size_t) kLevels * (kTableSize + 1); // next frame, same level
        const float vb = b[i] + fr * (b[i + 1] - b[i]);
        return va + ff * (vb - va);
    }

private:
    int frames;
    juce::String name;
    std::vector<float> cycles, data;
};

// The built-in bank (index 0..kBankSize-1); the first call builds all of it
const Wavetable& bankTable (int index);

// A Serum-style wavetable: frames of 2048 samples (or the size in a Serum "clm " chunk). A file shorter than
// one frame is taken as a single cycle. Long tables are thinned to kMaxUserFrames frames.
std::unique_ptr<Wavetable> loadWavetable (const juce::File& file, juce::String& error);
std::unique_ptr<Wavetable> wavetableFromAudio (const juce::AudioBuffer<float>& audio, int frameSize, const juce::String& name, juce::String& error);
} // namespace kube
