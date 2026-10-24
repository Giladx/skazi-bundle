#pragma once

// BULLY's built-in wavetables: 12 tables x 32 frames, band-limited per octave (mip levels)
// so high notes do not alias. Generated once at startup, shared by every instance.
#include <juce_core/juce_core.h>
#include <vector>

namespace bully
{
class WaveBank
{
public:
    static constexpr int kFrames = 32, kMips = 10, kTop = 1024; // mip 0 holds up to 511 harmonics in 1024 samples

    static const WaveBank& get();

    // size of one cycle at mip m (always a power of two)
    static int size (int mip) { return juce::jmax (64, kTop >> mip); }
    // highest harmonic kept at mip m
    static int harmonics (int mip) { return juce::jmax (1, (kTop / 2 - 1) >> mip); }
    // the mip to use for a fundamental of f Hz (times the extra brightness a warp adds)
    static int mipFor (double f, double sampleRate)
    {
        const double allowed = 0.45 * sampleRate / juce::jmax (1.0, f);
        int m = 0;
        while (m < kMips - 1 && harmonics (m) > allowed) ++m;
        return m;
    }

    const float* cycle (int table, int mip, int frame) const
    {
        return data.data() + offsets[(size_t) ((table * kMips + mip) * kFrames + frame)];
    }

    // a smooth preview of one frame (mip 0), x in 0..1
    float preview (int table, float pos, float x) const;

private:
    WaveBank();
    std::vector<float> data;
    std::vector<size_t> offsets;
};
} // namespace bully
