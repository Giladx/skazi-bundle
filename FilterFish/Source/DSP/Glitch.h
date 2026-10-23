#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// Tempo-synced stutter: on every step a coin flip (chance %) decides whether that step gets glitched.
// Modes: Repeat (loop the last slice), Reverse (play the last slice backwards), Gate (chop), Mixed (random per step).
class Glitch
{
public:
    void prepare (double sampleRate)
    {
        size = (int) (sampleRate * 8.0) + 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        write = counter = loopPos = 0;
        active = false;
        gain = 0.0f;
    }

    void setParams (int newMode, double stepSamples, int newRepeats, float probability) noexcept
    {
        mode = newMode;
        stepLen = juce::jlimit (64, size / 3, (int) stepSamples);
        repeats = newRepeats;
        prob = probability;
    }

    // Align step boundaries with the host grid (call once per block while playing).
    void resync (double ppq, double beatsPerStep)
    {
        const double pos = ppq / beatsPerStep;
        const int newCounter = (int) ((pos - std::floor (pos)) * stepLen);
        if (newCounter < counter - stepLen / 2) trigger();
        counter = newCounter;
    }

    void process (float* L, float* R, int numSamples, float mix) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float xl = L[i], xr = R != nullptr ? R[i] : xl;
            buf[0][(size_t) write] = xl;
            buf[1][(size_t) write] = xr;

            if (++counter >= stepLen)
            {
                counter = 0;
                trigger();
            }

            float wl = 0.0f, wr = 0.0f;
            if (active)
            {
                const int pos = loopPos % sliceLen;
                const float env = juce::jmin (1.0f, (float) pos / 32.0f, (float) (sliceLen - pos) / 32.0f);
                switch (currentMode)
                {
                    case 0:
                    {
                        const auto rp = (size_t) wrap (sliceStart + pos);
                        wl = buf[0][rp] * env;
                        wr = buf[1][rp] * env;
                        break;
                    }
                    case 1:
                    {
                        const auto rp = (size_t) wrap (sliceStart + sliceLen - 1 - pos);
                        wl = buf[0][rp] * env;
                        wr = buf[1][rp] * env;
                        break;
                    }
                    default:
                    {
                        const int half = sliceLen / 2;
                        const float g = pos < half ? juce::jmin (1.0f, (float) pos / 32.0f, (float) (half - pos) / 32.0f) : 0.0f;
                        wl = xl * g;
                        wr = xr * g;
                        break;
                    }
                }
                ++loopPos;
            }

            gain += ((active ? 1.0f : 0.0f) - gain) * 0.01f;
            const float a = gain * mix;
            L[i] = xl + (wl - xl) * a;
            if (R != nullptr) R[i] = xr + (wr - xr) * a;

            if (++write >= size) write = 0;
        }
    }

private:
    int wrap (int i) const noexcept
    {
        i %= size;
        return i < 0 ? i + size : i;
    }

    void trigger() noexcept
    {
        active = rng.nextFloat() < prob;
        currentMode = mode == 3 ? rng.nextInt (3) : mode;
        sliceLen = juce::jmax (64, stepLen / juce::jmax (1, repeats));
        sliceStart = wrap (write - sliceLen);
        loopPos = 0;
    }

    std::array<std::vector<float>, 2> buf;
    int size = 1, write = 0, counter = 0, stepLen = 4096, repeats = 4;
    int sliceLen = 1024, sliceStart = 0, loopPos = 0, mode = 0, currentMode = 0;
    float prob = 0.5f, gain = 0.0f;
    bool active = false;
    juce::Random rng;
};
