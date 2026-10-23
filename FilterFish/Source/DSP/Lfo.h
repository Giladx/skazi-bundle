#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include "Parameters.h"

// Bipolar (-1..1) LFO with classic and random shapes, free running or locked to the host bar position.
class Lfo
{
public:
    using Steps = std::array<float, 16>;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        phase = 0.0;
        smoothed = 0.0f;
        smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));
    }

    void setShape (int s) noexcept           { shape = s; }
    void setFrequency (double hz) noexcept   { inc = hz / sr; }
    double getPhase() const noexcept         { return phase; }

    void setSeed (int seed)
    {
        if (seed != currentSeed)
        {
            currentSeed = seed;
            steps = makeSteps (seed);
        }
    }

    static Steps makeSteps (int seed)
    {
        juce::Random r (seed * 31 + 7);
        Steps s {};
        for (auto& v : s) v = r.nextFloat() * 2.0f - 1.0f;
        return s;
    }

    // Lock the phase to the host's musical position (call once per block while the transport runs).
    void syncTo (double ppq, double beatsPerCycle)
    {
        const double pos = ppq / beatsPerCycle;
        const double newPhase = pos - std::floor (pos);
        if (newPhase < phase - 0.5) newCycle();
        phase = newPhase;
    }

    float next() noexcept
    {
        float out = 0.0f;
        const float ph = (float) phase;
        switch (shape)
        {
            case ls::SampleHold:   out = randNext; break;
            case ls::SmoothRandom: out = randPrev + (randNext - randPrev) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * ph)); break;
            case ls::Chaos:        out = randPrev + (randNext - randPrev) * ph; break;
            default:               out = evaluate (shape, ph, steps); break;
        }

        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            newCycle();
        }

        smoothed += (out - smoothed) * smoothCoef; // de-click hard edges (square, steps, S&H)
        return smoothed;
    }

    // Shape value for a phase 0..1. Random shapes are previewed with the step pattern so the UI can draw them.
    static float evaluate (int shape, float ph, const Steps& steps) noexcept
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        switch (shape)
        {
            case ls::Sine:         return std::sin (twoPi * ph);
            case ls::Triangle:     return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case ls::SawUp:        return 2.0f * ph - 1.0f;
            case ls::SawDown:      return 1.0f - 2.0f * ph;
            case ls::Square:       return ph < 0.5f ? 1.0f : -1.0f;
            case ls::StepSeq:      return steps[(size_t) juce::jlimit (0, 15, (int) (ph * 16.0f))];
            case ls::Bounce:       return std::abs (std::sin (twoPi * ph)) * 2.0f - 1.0f;
            case ls::SampleHold:   return steps[(size_t) juce::jlimit (0, 7, (int) (ph * 8.0f))];
            case ls::SmoothRandom:
            case ls::Chaos:
            {
                const float pos = ph * 8.0f;
                const int i = juce::jlimit (0, 7, (int) pos);
                const float t = pos - (float) i;
                return steps[(size_t) i] + (steps[(size_t) i + 1] - steps[(size_t) i]) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t));
            }
            default: return 0.0f;
        }
    }

private:
    void newCycle() noexcept
    {
        randPrev = randNext;
        if (shape == ls::Chaos)
        {
            chaosX = 3.99f * chaosX * (1.0f - chaosX); // logistic map
            randNext = chaosX * 2.0f - 1.0f;
        }
        else
        {
            randNext = rng.nextFloat() * 2.0f - 1.0f;
        }
    }

    double sr = 44100.0, phase = 0.0, inc = 0.0;
    int shape = ls::Sine, currentSeed = -1;
    Steps steps {};
    float randPrev = 0.0f, randNext = 0.0f, chaosX = 0.37f;
    float smoothed = 0.0f, smoothCoef = 1.0f;
    juce::Random rng;
};
