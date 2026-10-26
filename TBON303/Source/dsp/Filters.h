#pragma once

// Zero-delay-feedback filters: the 303 diode ladder, a Moog-style transistor
// ladder and a vowel/formant bank. All are trapezoidal (TPT) integrators, so the
// feedback loop is solved within the sample instead of using a unit delay.

#include "Oscillator.h"
#include <algorithm>
#include <cmath>

namespace s303
{
inline double clampCutoff (double hz, double sr) { return std::clamp (hz, 10.0, sr * 0.45); }

/**
    4-pole diode ladder (TB-303 topology). Each capacitor stage is coupled to its
    neighbours, which spreads the poles: ~18 dB/oct around the cutoff, steepening
    to 24 dB/oct far above it. Linear ODEs, normalised to wc:

        y1' = wc (u + y2 - 2 y1)
        y2' = wc/2 (y1 + y3 - 2 y2)
        y3' = wc/2 (y2 + y4 - 2 y3)
        y4' = wc/2 (y3 - y4)          with u = tanh (x - k y4)

    The stage matrix is tridiagonal, so each sample is one Thomas solve plus a short
    Newton iteration on the saturating feedback node.
*/
class DiodeLadder
{
public:
    // Stage coupling: each capacitor also leaks into the next stage up. 0.3 keeps the
    // filter bright at low resonance with ~15-18 dB/oct around the cutoff.
    // Linear model: resonant peak at 1.02 wc, self-oscillation from k = 6.33.
    // The saturating feedback pulls the peak a little lower, measured by the render test.
    static constexpr double coupling = 0.3;
    static constexpr double peakRatio = 0.97;
    static constexpr double kCritical = 6.33;
    static constexpr double feedbackHighpassHz = 60.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double gh = std::tan (kPi * feedbackHighpassHz / sr);
        hpG = gh / (1.0 + gh);
        reset();
    }

    void reset()
    {
        std::fill (std::begin (s), std::end (s), 0.0);
        hpState = 0.0;
    }

    /** cutoffHz = where the resonant peak lands. resonance 0..1. */
    float process (float input, double cutoffHz, double resonance)
    {
        const double fc = clampCutoff (cutoffHz / peakRatio, sr);
        const double g = std::tan (kPi * fc / sr);
        // Resonance curve: gentle at first, strong in the middle, at the edge of self-oscillation
        // around 87%, and past it at the top: the last part of the knob is the hard-acid scream.
        // The saturating feedback (tanh below) keeps the scream musical instead of blowing up.
        const double k = kCritical * 1.2 * std::pow (std::clamp (resonance, 0.0, 1.0), 1.3);
        const double c = coupling;

        // Tridiagonal M = I - g A. sub[i] couples to y[i-1], sup[i] to y[i+1].
        //   y1' = wc (u  - y1 + c (y2 - y1))
        //   yi' = wc (yi-1 - yi + c (yi+1 - yi))   i = 2, 3
        //   y4' = wc (y3 - y4)
        const double diag[4] = { 1.0 + g * (1.0 + c), 1.0 + g * (1.0 + c), 1.0 + g * (1.0 + c), 1.0 + g };
        const double sub[4]  = { 0.0, -g, -g, -g };
        const double sup[4]  = { -g * c, -g * c, -g * c, 0.0 };

        // Solve M beta = s and M alpha = g e0 together (y = beta + alpha * u).
        double cp[4], bp[4], ap[4];
        cp[0] = sup[0] / diag[0];
        bp[0] = s[0] / diag[0];
        ap[0] = g / diag[0];
        for (int i = 1; i < 4; ++i)
        {
            const double m = diag[i] - sub[i] * cp[i - 1];
            cp[i] = sup[i] / m;
            bp[i] = (s[i] - sub[i] * bp[i - 1]) / m;
            ap[i] = (0.0 - sub[i] * ap[i - 1]) / m;
        }
        double beta[4], alpha[4];
        beta[3] = bp[3];
        alpha[3] = ap[3];
        for (int i = 2; i >= 0; --i)
        {
            beta[i] = bp[i] - cp[i] * beta[i + 1];
            alpha[i] = ap[i] - cp[i] * alpha[i + 1];
        }

        // Like the hardware, the resonance feedback passes a high-pass, so the lowest
        // frequencies are not fed back and the bass survives high resonance.
        //   hp = (1 - hpG) (y4 - hpState),  y4 = beta4 + alpha4 u
        // Solve u = tanh (x - k hp) by Newton from the linear guess.
        const double x = input;
        const double hpScale = k * (1.0 - hpG);
        const double a = x - hpScale * (beta[3] - hpState);
        const double b = hpScale * alpha[3];
        double u = std::tanh (a / (1.0 + b));
        for (int it = 0; it < 3; ++it)
        {
            const double t = std::tanh (a - b * u);
            const double f = u - t;
            const double df = 1.0 + b * (1.0 - t * t);
            u -= f / df;
        }

        double y[4];
        for (int i = 0; i < 4; ++i)
        {
            y[i] = beta[i] + alpha[i] * u;
            s[i] = 2.0 * y[i] - s[i];
        }

        const double v = (y[3] - hpState) * hpG;
        hpState += 2.0 * v;

        // Passband make-up: give back part of the level the resonance takes, so the
        // squelch stays loud and the line keeps its weight.
        return (float) (y[3] * std::pow (1.0 + k, 0.45));
    }

private:
    double sr = 44100.0;
    double s[4] {};
    double hpG = 0.0, hpState = 0.0;
};

/** 24 dB/oct transistor ladder (Zavalishin's ZDF form) with saturating input stage. */
class MoogLadder
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { std::fill (std::begin (s), std::end (s), 0.0); }

    float process (float input, double cutoffHz, double resonance)
    {
        const double g = std::tan (kPi * clampCutoff (cutoffHz, sr) / sr);
        const double G = g / (1.0 + g);
        const double k = 4.1 * resonance;
        const double h = 1.0 - G; // stage state weight

        const double S = G * G * G * s[0] * h + G * G * s[1] * h + G * s[2] * h + s[3] * h;
        const double G4 = G * G * G * G;
        double u = (input * (1.0 + 0.5 * k) - k * S) / (1.0 + k * G4);
        u = std::tanh (u);

        double v = u;
        for (int i = 0; i < 4; ++i)
        {
            const double d = (v - s[i]) * G;
            const double y = d + s[i];
            s[i] = y + d;
            v = y;
        }
        return (float) v;
    }

private:
    double sr = 44100.0;
    double s[4] {};
};

/** TPT state-variable filter (Simper). Band output is normalised to unity peak. */
class SVF
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { ic1 = ic2 = 0.0; }

    void setup (double hz, double q)
    {
        const double g = std::tan (kPi * clampCutoff (hz, sr) / sr);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double band (double x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return v1 * k;
    }

private:
    double sr = 44100.0, ic1 = 0.0, ic2 = 0.0, k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
};

/** Three parallel band-passes morphing through A-E-I-O-U, so the line can "talk". */
class FormantFilter
{
public:
    void prepare (double sampleRate)
    {
        for (auto& f : bands)
            f.prepare (sampleRate);
    }

    void reset()
    {
        for (auto& f : bands)
            f.reset();
    }

    /** vowel 0..4 (A,E,I,O,U), shift: formant frequency multiplier, resonance 0..1. */
    float process (float x, double vowel, double shift, double resonance)
    {
        static constexpr double table[5][3] = {
            { 730.0, 1090.0, 2440.0 }, // A
            { 530.0, 1840.0, 2480.0 }, // E
            { 270.0, 2290.0, 3010.0 }, // I
            { 570.0,  840.0, 2410.0 }, // O
            { 300.0,  870.0, 2240.0 }, // U
        };
        static constexpr double gains[3] = { 1.0, 0.6, 0.35 };

        vowel = std::clamp (vowel, 0.0, 4.0);
        const int i0 = std::min ((int) vowel, 3);
        const double t = vowel - i0;
        const double q = 3.0 + resonance * 17.0;

        double out = 0.0;
        for (int b = 0; b < 3; ++b)
        {
            const double hz = (table[i0][b] * (1.0 - t) + table[i0 + 1][b] * t) * shift;
            bands[b].setup (hz, q);
            out += bands[b].band (x) * gains[b];
        }
        return (float) (out * 4.0);
    }

private:
    SVF bands[3];
};
} // namespace s303
