#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

namespace fx
{
class Distortion
{
public:
    void reset() noexcept
    {
        hold.fill (0.0f);
        holdCount.fill (0);
    }

    float process (int ch, float x, int type, float drive) noexcept
    {
        const float pre = 1.0f + drive * 24.0f;
        switch (type)
        {
            case 0: return std::tanh (x * pre) * 0.8f;
            case 1: return juce::jlimit (-1.0f, 1.0f, x * pre) * 0.7f;
            case 2:
            {
                // Asymmetric "tube": softer on the negative half -> even harmonics.
                const float v = x * pre;
                return (v >= 0.0f ? 1.0f - std::exp (-v) : -0.8f * (1.0f - std::exp (v * 1.25f))) * 0.85f;
            }
            case 3: return std::sin (x * pre) * 0.7f;
            case 4:
            {
                const int holdLen = 1 + (int) (drive * drive * 40.0f);
                auto& c = holdCount[(size_t) ch];
                if (++c >= holdLen)
                {
                    c = 0;
                    const float q = std::exp2 (15.0f - drive * 13.0f);
                    hold[(size_t) ch] = std::round (x * q) / q;
                }
                return hold[(size_t) ch];
            }
            case 5: return std::tanh (std::abs (x) * pre) * 0.8f;
            default: return x;
        }
    }

private:
    std::array<float, 2> hold {};
    std::array<int, 2> holdCount {};
};

struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f;
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + 0.995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

// Inflator-style loudness curve. Polynomial taken from the publicly documented analysis of the
// Oxford Inflator transfer curve (curve -50..50, effect = wet amount).
inline float inflate (float x, float effect, float curve) noexcept
{
    const float c = curve / 100.0f;
    const float A = 1.5f + c, B = -2.0f * c, C = c - 0.5f, D = 0.0625f - 0.25f * c + 0.25f * c * c;
    const float s = juce::jmin (1.0f, std::abs (x));
    const float s2 = s * s, s3 = s2 * s, s4 = s3 * s;
    const float shaped = A * s + B * s2 + C * s3 - D * (s2 - 2.0f * s3 + s4);
    const float dry = juce::jlimit (-1.0f, 1.0f, x);
    return dry + (std::copysign (shaped, x) - dry) * effect;
}

// Mid/side stereo expander: 0 = mono, 1 = unchanged, 2 = extra wide.
inline void applyWidth (float& l, float& r, float width) noexcept
{
    const float m = 0.5f * (l + r);
    const float s = 0.5f * (l - r) * width;
    l = m + s;
    r = m - s;
}

class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        size = (int) (sampleRate * 4.0) + 4;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        timeSm.reset (sampleRate, 0.15);
        timeSm.setCurrentAndTargetValue ((float) sampleRate * 0.3f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        tone.fill (0.0f);
        write = 0;
    }

    void process (float* L, float* R, int numSamples, float delaySamples, float feedback, bool pingPong, float mix) noexcept
    {
        timeSm.setTargetValue (juce::jlimit (1.0f, (float) size - 3.0f, delaySamples));
        for (int i = 0; i < numSamples; ++i)
        {
            const float d = timeSm.getNextValue();
            const float dl = read (0, d), dr = read (1, d);
            tone[0] += 0.35f * (dl - tone[0]); // darker repeats, like a real echo
            tone[1] += 0.35f * (dr - tone[1]);

            const float xl = L[i], xr = R != nullptr ? R[i] : xl;
            if (pingPong && R != nullptr)
            {
                buf[0][(size_t) write] = 0.5f * (xl + xr) + feedback * tone[1];
                buf[1][(size_t) write] = feedback * tone[0];
            }
            else
            {
                buf[0][(size_t) write] = xl + feedback * tone[0];
                buf[1][(size_t) write] = xr + feedback * tone[1];
            }

            L[i] = xl + (dl - xl) * mix;
            if (R != nullptr) R[i] = xr + (dr - xr) * mix;
            if (++write >= size) write = 0;
        }
    }

private:
    float read (int ch, float delay) const noexcept
    {
        float rp = (float) write - delay;
        if (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float f = rp - (float) i0;
        const auto& b = buf[(size_t) ch];
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    std::array<std::vector<float>, 2> buf;
    std::array<float, 2> tone {};
    int size = 4, write = 0;
    juce::SmoothedValue<float> timeSm;
};
} // namespace fx
