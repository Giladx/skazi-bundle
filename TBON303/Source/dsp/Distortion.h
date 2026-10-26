#pragma once

// Drive stages. One Distortion instance is used before the filter (makes the
// resonance scream) and one after it (the classic "303 into a pedal" setup).

#include "Oscillator.h"
#include <algorithm>
#include <cmath>

namespace s303
{
// New types are appended so saved projects and presets keep their old choices.
enum class DistType { Tube = 0, Fuzz, Bitcrush, Wavefold, Overdrive, Distortion, SuperDist, RingMod, Rectifier, Diode,
                      Tape, Transistor, Germanium, Crunch };

class Distortion
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        held = 0.0f;
        holdCounter = 0.0;
        ringPhase = 0.0;
        dcX = dcY = 0.0;
    }

    /** amount 0..1. At 0 the stage is fully bypassed. */
    float process (float x, DistType type, float amount)
    {
        if (amount <= 0.0001f)
            return x;

        const float drive = 1.0f + 24.0f * amount * amount;
        float y = x;

        switch (type)
        {
            case DistType::Tube:
            {
                // Asymmetric: the bias makes positive half clip earlier -> even harmonics.
                constexpr float bias = 0.25f;
                y = (std::tanh (drive * x + bias) - std::tanh (bias)) / std::tanh (std::sqrt (drive));
                break;
            }
            case DistType::Fuzz:
            {
                const float a = drive * x * 1.5f;
                y = a >= 0.0f ? 1.0f - std::exp (-a) : -0.8f * (1.0f - std::exp (a * 1.4f));
                break;
            }
            case DistType::Bitcrush:
            {
                const float bits = 16.0f - 13.0f * amount;         // 16 -> 3 bits
                const double rateDiv = 1.0 + 31.0 * amount * amount; // sample hold
                holdCounter += 1.0;
                if (holdCounter >= rateDiv)
                {
                    holdCounter -= rateDiv;
                    const float steps = std::pow (2.0f, bits - 1.0f);
                    held = std::round (std::clamp (x * (1.0f + amount), -1.0f, 1.0f) * steps) / steps;
                }
                y = held;
                break;
            }
            case DistType::Wavefold:
            {
                // Sine folder: every extra unit of gain folds the peaks back inwards,
                // turning the saw into bright, metallic sidebands.
                y = (float) std::sin (x * (0.5 + drive * 0.6) * kPi * 0.5);
                break;
            }
            case DistType::Overdrive:
            {
                // Smooth arctan curve with a slight lean: warm pedal breakup.
                const float g = 0.8f * drive;
                y = (std::atan (g * x + 0.1f) - std::atan (0.1f)) / std::atan (g);
                break;
            }
            case DistType::Distortion:
            {
                // Smooth-cornered hard clip: tight, bright and aggressive.
                const float z = drive * x * 1.5f;
                y = z / std::pow (1.0f + z * z * z * z, 0.25f);
                break;
            }
            case DistType::SuperDist:
            {
                // Two stages slammed into each other: close to a square wave, maximum grind.
                const float first = std::tanh (drive * 2.0f * x);
                y = std::tanh (6.0f * first + 0.2f * first * first) * 0.9f;
                break;
            }
            case DistType::RingMod:
            {
                // Multiplies by a sine whose pitch rides the knob (30 Hz .. 2.5 kHz): metallic clang.
                const double hz = 30.0 * std::exp2 (6.4 * amount);
                ringPhase += hz / sr;
                if (ringPhase >= 1.0)
                    ringPhase -= 1.0;
                y = x * (float) std::sin (kTwoPi * ringPhase) * 1.4f;
                break;
            }
            case DistType::Rectifier:
            {
                // Full-wave rectifier: folds the negative half up, an octave-up buzz.
                y = 2.0f * std::abs (std::tanh (0.6f * drive * x)) - 0.6f;
                break;
            }
            case DistType::Tape:
            {
                // Gentle, round saturation with a touch of second harmonic.
                const float t = std::tanh (0.7f * drive * x);
                y = (t + 0.08f * t * t) / std::tanh (0.7f * drive);
                break;
            }
            case DistType::Transistor:
            {
                // Single-transistor stage: snappy and lopsided, the positive side squashes first.
                const float z = drive * x;
                y = z >= 0.0f ? 1.0f - std::exp (-1.3f * z) : -0.9f * (1.0f - std::exp (0.8f * z));
                break;
            }
            case DistType::Germanium:
            {
                // Leaky, saggy and warm: lots of even harmonics, a soft woolly edge.
                const float z = 0.8f * drive * x;
                const float w = z + 0.45f * z * std::abs (z);
                y = w / (1.0f + std::abs (w));
                break;
            }
            case DistType::Crunch:
            {
                // Clipped, then roughed up with extra odd harmonics: gritty bite.
                const float c = std::clamp (drive * x, -1.0f, 1.0f);
                y = c - 0.25f * c * c * c + 0.15f * std::sin (5.0f * c);
                break;
            }
            case DistType::Diode:
            {
                // Asymmetric silicon diodes: the positive side clips harder than the negative.
                const float z = drive * x;
                y = z >= 0.0f ? 0.6f * std::tanh (z / 0.6f) : 0.9f * std::tanh (z / 0.9f);
                break;
            }
        }

        // Fade the stage in over the first part of the knob so it starts subtle.
        const float wet = std::min (1.0f, amount * 6.0f);
        y = x + (y - x) * wet;

        // Asymmetric curves create DC; strip it so it doesn't push the filter around.
        const double out = y - dcX + 0.9995 * dcY;
        dcX = y;
        dcY = out;
        return (float) out;
    }

private:
    double sr = 44100.0;
    float held = 0.0f;
    double holdCounter = 0.0, ringPhase = 0.0;
    double dcX = 0.0, dcY = 0.0;
};
/**
    "FAT": a tube-style output stage. Boosts the lows a little, then pushes the
    whole line through an asymmetric tube curve for thickness and warmth.
*/
class FatStage
{
public:
    void prepare (double sampleRate)
    {
        lowCoeff = 1.0 - std::exp (-2.0 * kPi * 120.0 / sampleRate);
        low = dcX = dcY = 0.0;
    }

    float process (float x, float amount)
    {
        if (amount <= 0.0001f)
            return x;

        low += (x - low) * lowCoeff;
        const double boosted = x + low * 1.2 * amount;

        constexpr double bias = 0.15;
        const double drive = 1.0 + 3.0 * amount;
        // Normalised so turning FAT up thickens the sound without making it much louder.
        const double tube = (std::tanh (drive * boosted + bias) - std::tanh (bias)) / std::tanh (drive)
                          / (1.0 + 0.5 * amount);

        const double wet = std::min (1.0, (double) amount * 4.0);
        const double y = boosted + (tube - boosted) * wet;

        const double out = y - dcX + 0.9995 * dcY;
        dcX = y;
        dcY = out;
        return (float) out;
    }

private:
    double lowCoeff = 0.0, low = 0.0, dcX = 0.0, dcY = 0.0;
};
} // namespace s303
