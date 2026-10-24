#pragma once

// BULLY's FX rack, in signal order: HYPER, DIST, MODFX, FSHIFT, OTT, EQ, DELAY, REVERB.
// Called in short slices so modulated settings move smoothly.
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace bully
{
struct FxValues
{
    bool hyOn = false; int hyVoices = 4; float hyDetune = 0.3f, hyRate = 0.3f, hyMix = 0.5f;
    bool dsOn = false; int dsType = 0; float dsDrive = 0.3f, dsTone = 14000.0f, dsMix = 1.0f;
    bool moOn = false; int moType = 0; float moRate = 0.5f, moDepth = 0.4f, moFb = 0.2f, moMix = 0.5f;
    bool fsOn = false; float fsFreq = 0.0f, fsMix = 0.5f;
    bool otOn = false; float otDepth = 0.5f, otTime = 0.5f, otUp = 0.6f, otDown = 0.6f;
    bool eqOn = false; float eqLow = 0, eqMidF = 1000, eqMid = 0, eqHigh = 0;
    bool dlOn = false; float dlSeconds = 0.3f, dlFb = 0.35f, dlTone = 0.7f, dlMix = 0.25f; bool dlPingPong = true;
    bool rvOn = false; float rvSize = 0.6f, rvDecay = 0.5f, rvDamp = 0.4f, rvPre = 10.0f, rvWidth = 1.0f, rvMix = 0.25f;
    bool spOn = false; float spLow = 300.0f, spHigh = 2500.0f;
};

class FxRack
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        auto sizeFor = [this] (double seconds) { int n = 1; while (n < (int) (seconds * sr) + 4) n <<= 1; return n; };
        for (auto& b : hyBuf) b.assign ((size_t) sizeFor (0.06), 0.0f);
        for (auto& b : moBuf) b.assign ((size_t) sizeFor (0.05), 0.0f);
        for (auto& b : dlBuf) b.assign ((size_t) sizeFor (4.1), 0.0f);
        for (auto& b : rvPreBuf) b.assign ((size_t) sizeFor (0.3), 0.0f);
        static const int base[8] = { 1031, 1153, 1279, 1381, 1523, 1663, 1811, 1949 };
        for (int i = 0; i < 8; ++i) rvLine[(size_t) i].assign ((size_t) sizeFor (base[i] * 1.7 / 44100.0 + 0.01), 0.0f);
        for (auto& b : rvDiff) b.assign ((size_t) sizeFor (0.02), 0.0f);
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        for (auto* f : { &otLow, &otHigh }) { f->prepare (spec); f->setType (juce::dsp::LinkwitzRileyFilterType::lowpass); }
        otLow.setCutoffFrequency (120.0f);
        otHigh.setCutoffFrequency (2500.0f);
        for (auto* f : { &spLowX, &spHighX }) { f->prepare (spec); f->setType (juce::dsp::LinkwitzRileyFilterType::lowpass); }
        spCache[0] = spCache[1] = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto* set : { &hyBuf, &moBuf, &dlBuf, &rvPreBuf })
            for (auto& b : *set) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : rvLine) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : rvDiff) std::fill (b.begin(), b.end(), 0.0f);
        hyW = moW = dlW = rvW = rvPreW = 0;
        for (auto& v : rvDamp) v = 0.0f;
        for (auto& c : eq) c = {};
        for (auto& c : fsState) c = {};
        for (auto& a : phaserState) a.fill (0.0f);
        otLow.reset(); otHigh.reset(); spLowX.reset(); spHighX.reset();
        for (auto& e : otEnv) e = 0.0f;
        for (auto& g : otGain) g = 1.0f;
        dsHold[0] = dsHold[1] = 0.0f; dsCount = 0; dsLp[0] = dsLp[1] = 0; dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
        dlLp[0] = dlLp[1] = dlHp[0] = dlHp[1] = 0;
        dlSmooth = -1.0f;
    }

    void process (float* L, float* R, int n, const FxValues& v)
    {
        if (v.hyOn) hyper (L, R, n, v);
        if (v.dsOn) dist (L, R, n, v);
        if (v.moOn) modFx (L, R, n, v);
        if (v.fsOn) shifter (L, R, n, v);
        if (v.otOn) ott (L, R, n, v);
        if (v.eqOn) equalizer (L, R, n, v);
        if (! v.spOn)
        {
            if (v.dlOn) delay (L, R, n, v);
            if (v.rvOn) reverb (L, R, n, v);
            return;
        }
        // splitter: low band to mono, then delay + reverb on the high band only
        if (spCache[0] != v.spLow) { spLowX.setCutoffFrequency (juce::jlimit (30.0f, 0.4f * (float) sr, v.spLow)); spCache[0] = v.spLow; }
        if (spCache[1] != v.spHigh) { spHighX.setCutoffFrequency (juce::jlimit (60.0f, 0.4f * (float) sr, v.spHigh)); spCache[1] = v.spHigh; }
        float hiL[kMaxSlice], hiR[kMaxSlice], loL[kMaxSlice], loR[kMaxSlice];
        jassert (n <= kMaxSlice);
        for (int i = 0; i < n; ++i)
        {
            float lo[2], rest[2], below[2], above[2];
            spLowX.processSample (0, L[i], lo[0], rest[0]);
            spLowX.processSample (1, R[i], lo[1], rest[1]);
            const float mono = 0.5f * (lo[0] + lo[1]);
            spHighX.processSample (0, rest[0], below[0], above[0]);
            spHighX.processSample (1, rest[1], below[1], above[1]);
            loL[i] = mono + below[0]; loR[i] = mono + below[1];
            hiL[i] = above[0]; hiR[i] = above[1];
        }
        if (v.dlOn) delay (hiL, hiR, n, v);
        if (v.rvOn) reverb (hiL, hiR, n, v);
        for (int i = 0; i < n; ++i) { L[i] = loL[i] + hiL[i]; R[i] = loR[i] + hiR[i]; }
    }

    static constexpr int kMaxSlice = 64; // process() takes at most this many samples per call

private:
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;
    double sr = 44100.0;
    juce::dsp::LinkwitzRileyFilter<float> spLowX, spHighX;
    float spCache[2] { -1.0f, -1.0f };

    static float readFrac (const std::vector<float>& b, int w, float delay)
    {
        const int mask = (int) b.size() - 1;
        const float rp = (float) w - delay;
        const int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        const float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
        return a + (c - a) * f;
    }

    //== HYPER: a cloud of detuned, drifting copies spread across the stereo field
    std::array<std::vector<float>, 2> hyBuf;
    int hyW = 0;
    std::array<double, 7> hyPh { 0.0, 0.31, 0.57, 0.13, 0.83, 0.42, 0.68 };
    void hyper (float* L, float* R, int n, const FxValues& v)
    {
        const int voices = juce::jlimit (2, 7, v.hyVoices), mask = (int) hyBuf[0].size() - 1;
        const float depthMs = 0.4f + v.hyDetune * 7.0f, baseMs = 9.0f;
        const float norm = 1.0f / std::sqrt ((float) voices);
        for (int i = 0; i < n; ++i)
        {
            hyBuf[0][(size_t) hyW] = L[i];
            hyBuf[1][(size_t) hyW] = R[i];
            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < voices; ++k)
            {
                const float rate = (0.15f + 2.5f * v.hyRate) * (0.7f + 0.13f * (float) k);
                hyPh[(size_t) k] += rate / sr;
                if (hyPh[(size_t) k] >= 1.0) hyPh[(size_t) k] -= 1.0;
                const float d = (baseMs + (float) k * 1.7f + depthMs * (0.5f + 0.5f * std::sin (twoPi * (float) hyPh[(size_t) k]))) * 0.001f * (float) sr;
                const float pan = voices == 1 ? 0.0f : (float) k / (float) (voices - 1) * 2.0f - 1.0f;
                const float s = readFrac (hyBuf[(size_t) (k & 1)], hyW, d);
                wl += s * (0.5f - 0.5f * pan);
                wr += s * (0.5f + 0.5f * pan);
            }
            hyW = (hyW + 1) & mask;
            L[i] += (wl * norm * 1.4f - L[i]) * v.hyMix * 0.6f;
            R[i] += (wr * norm * 1.4f - R[i]) * v.hyMix * 0.6f;
        }
    }

    //== DIST
    float dsHold[2] {}, dsLp[2] {}, dcX[2] {}, dcY[2] {};
    int dsCount = 0;
    void dist (float* L, float* R, int n, const FxValues& v)
    {
        const float g = std::pow (10.0f, v.dsDrive * 36.0f / 20.0f);
        const float lpA = 1.0f - std::exp (-twoPi * v.dsTone / (float) sr);
        const float dcA = 1.0f - twoPi * 12.0f / (float) sr;
        const int hold = 1 + (int) (v.dsDrive * v.dsDrive * 40.0f);
        const float bits = 16.0f - v.dsDrive * 13.5f, q = std::pow (2.0f, bits - 1.0f);
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const bool take = (dsCount++ % hold) == 0;
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                float y;
                switch (v.dsType)
                {
                    case 0:  y = std::tanh (g * x + 0.15f * v.dsDrive) - std::tanh (0.15f * v.dsDrive); break;  // TUBE (asymmetric)
                    case 1:  { const float s = g * x; y = s / (1.0f + std::abs (s)); break; }                     // SOFT
                    case 2:  y = juce::jlimit (-1.0f, 1.0f, g * x); break;                                         // HARD
                    case 3:  { const float s = g * x; y = s > 0 ? 1.0f - std::exp (-s) : -0.6f * (1.0f - std::exp (s / 0.6f)); break; } // DIODE
                    case 4:  y = std::sin (juce::jlimit (-40.0f, 40.0f, g * x * 1.2f)); break;                     // FOLD
                    case 5:  y = std::round (x * q) / q; break;                                                     // BITCRUSH
                    case 6:  { if (take) dsHold[c] = x; y = dsHold[c]; break; }                                    // DOWNSAMPLE
                    default: { const float s = std::tanh (g * x); y = std::abs (s) * 1.6f - 0.5f; break; }         // RECTIFY
                }
                // tone, then remove any DC the shaper made
                dsLp[c] += (y - dsLp[c]) * lpA;
                const float dc = dsLp[c] - dcX[c] + dcA * dcY[c];
                dcX[c] = dsLp[c]; dcY[c] = dc;
                const float wet = v.dsType >= 5 ? dc : dc * 0.75f;
                ch[c][i] = x + (wet - x) * v.dsMix;
            }
        }
    }

    //== MODFX: chorus, flanger, phaser
    std::array<std::vector<float>, 2> moBuf;
    int moW = 0;
    double moPh = 0.0;
    std::array<std::array<float, 6>, 2> phaserState {};
    float phaserFb[2] {};
    void modFx (float* L, float* R, int n, const FxValues& v)
    {
        const int mask = (int) moBuf[0].size() - 1;
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            moPh += v.moRate / sr;
            if (moPh >= 1.0) moPh -= 1.0;
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                const float lfo = 0.5f + 0.5f * std::sin (twoPi * (float) moPh + (c == 0 ? 0.0f : juce::MathConstants<float>::halfPi));
                float wet;
                if (v.moType == 2) // PHASER: 6 allpasses swept 150 Hz .. 5 kHz
                {
                    const float fc = 150.0f * std::pow (2.0f, (0.3f + v.moDepth * 4.7f) * lfo);
                    const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, 0.45f * (float) sr) / (float) sr);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    float z = x + phaserFb[c] * v.moFb * 0.9f;
                    for (auto& st : phaserState[(size_t) c])
                    {
                        const float o = a * z + st;
                        st = z - a * o;
                        z = o;
                    }
                    phaserFb[c] = z;
                    wet = z;
                }
                else
                {
                    const bool fl = v.moType == 1;
                    const float ms = fl ? 0.4f + v.moDepth * 4.5f * lfo : 7.0f + v.moDepth * 14.0f * lfo;
                    const float d = ms * 0.001f * (float) sr;
                    wet = readFrac (moBuf[(size_t) c], moW, d);
                    moBuf[(size_t) c][(size_t) moW] = x + (fl ? wet * v.moFb * 0.95f : wet * v.moFb * 0.3f);
                }
                ch[c][i] = x + (wet - x) * v.moMix * 0.5f;
            }
            moW = (moW + 1) & mask;
        }
    }

    //== FREQUENCY SHIFTER: Hilbert pair (two allpass chains) and a quadrature oscillator
    struct Hilbert { float xa[4][2] {}, ya[4][2] {}, xb[4][2] {}, yb[4][2] {}, delayB = 0.0f; };
    std::array<Hilbert, 2> fsState {};
    double fsPh = 0.0;
    static float ap2 (float x, float a2, float (&xs)[2], float (&ys)[2])
    {
        const float y = a2 * (x + ys[1]) - xs[1];
        xs[1] = xs[0]; xs[0] = x;
        ys[1] = ys[0]; ys[0] = y;
        return y;
    }
    void shifter (float* L, float* R, int n, const FxValues& v)
    {
        static const float ca[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static const float cb[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            fsPh += v.fsFreq / sr;
            fsPh -= std::floor (fsPh);
            const float c0 = std::cos (twoPi * (float) fsPh), s0 = std::sin (twoPi * (float) fsPh);
            for (int c = 0; c < 2; ++c)
            {
                auto& h = fsState[(size_t) c];
                const float x = ch[c][i];
                float a = x, b = x;
                for (int k = 0; k < 4; ++k) a = ap2 (a, ca[k] * ca[k], h.xa[k], h.ya[k]);
                for (int k = 0; k < 4; ++k) b = ap2 (b, cb[k] * cb[k], h.xb[k], h.yb[k]);
                const float bd = h.delayB;
                h.delayB = b;
                const float wet = a * c0 - bd * s0;
                ch[c][i] = x + (wet - x) * v.fsMix;
            }
        }
    }

    //== OTT: three bands, each pushed up when quiet and down when loud
    juce::dsp::LinkwitzRileyFilter<float> otLow, otHigh;
    float otEnv[3] {}, otGain[3] { 1.0f, 1.0f, 1.0f };
    void ott (float* L, float* R, int n, const FxValues& v)
    {
        const float att = std::exp (-1.0f / (float) ((0.001 + 0.03 * v.otTime) * sr));
        const float rel = std::exp (-1.0f / (float) ((0.03 + 0.4 * v.otTime) * sr));
        const float gSm = std::exp (-1.0f / (float) (0.003 * sr));
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            float band[2][3];
            for (int c = 0; c < 2; ++c)
            {
                float lo, rest, mid, hi;
                otLow.processSample (c, ch[c][i], lo, rest);
                otHigh.processSample (c, rest, mid, hi);
                band[c][0] = lo; band[c][1] = mid; band[c][2] = hi;
            }
            for (int b = 0; b < 3; ++b)
            {
                const float lv = juce::jmax (std::abs (band[0][b]), std::abs (band[1][b]));
                otEnv[b] = lv > otEnv[b] ? att * otEnv[b] + (1 - att) * lv : rel * otEnv[b] + (1 - rel) * lv;
                const float db = juce::Decibels::gainToDecibels (otEnv[b], -100.0f);
                float gdb = 0.0f;
                if (db > -14.0f) gdb -= (db + 14.0f) * 0.75f * v.otDown;
                else if (db > -75.0f && db < -34.0f) gdb += juce::jmin (18.0f, (-34.0f - db) * 0.6f) * v.otUp;
                const float target = juce::Decibels::decibelsToGain (gdb + 4.0f * v.otUp);
                otGain[b] = gSm * otGain[b] + (1 - gSm) * target;
            }
            for (int c = 0; c < 2; ++c)
            {
                const float wet = band[c][0] * otGain[0] + band[c][1] * otGain[1] + band[c][2] * otGain[2];
                ch[c][i] += (wet - ch[c][i]) * v.otDepth;
            }
        }
    }

    //== EQ: low shelf, mid bell, high shelf
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {}; };
    std::array<Biquad, 3> eq {};
    float eqCache[4] { -999, -999, -999, -999 };
    static void rbj (Biquad& q, int kind, double fs, double f, double gainDb, double Q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
        const double cw = std::cos (w), sw = std::sin (w), al = sw / (2.0 * Q);
        double b0, b1, b2, a0, a1, a2;
        if (kind == 1) { b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; }
        else
        {
            const double sq = 2.0 * std::sqrt (A) * al, s = kind == 0 ? 1.0 : -1.0;
            b0 = A * ((A + 1) - s * (A - 1) * cw + sq);
            b1 = s * 2 * A * ((A - 1) - s * (A + 1) * cw);
            b2 = A * ((A + 1) - s * (A - 1) * cw - sq);
            a0 = (A + 1) + s * (A - 1) * cw + sq;
            a1 = -s * 2 * ((A - 1) + s * (A + 1) * cw);
            a2 = (A + 1) + s * (A - 1) * cw - sq;
        }
        q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0); q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
    }
    void equalizer (float* L, float* R, int n, const FxValues& v)
    {
        if (v.eqLow != eqCache[0]) { rbj (eq[0], 0, sr, 120.0, v.eqLow, 0.707); eqCache[0] = v.eqLow; }
        if (v.eqMid != eqCache[1] || v.eqMidF != eqCache[2]) { rbj (eq[1], 1, sr, juce::jmin ((double) v.eqMidF, 0.45 * sr), v.eqMid, 0.9); eqCache[1] = v.eqMid; eqCache[2] = v.eqMidF; }
        if (v.eqHigh != eqCache[3]) { rbj (eq[2], 2, sr, juce::jmin (7000.0, 0.4 * sr), v.eqHigh, 0.707); eqCache[3] = v.eqHigh; }
        float* ch[2] = { L, R };
        for (auto& q : eq)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float x = ch[c][i];
                    const float y = q.b0 * x + q.z1[c];
                    q.z1[c] = q.b1 * x - q.a1 * y + q.z2[c];
                    q.z2[c] = q.b2 * x - q.a2 * y;
                    ch[c][i] = y;
                }
    }

    //== DELAY: stereo or ping-pong, darker repeats with TONE
    std::array<std::vector<float>, 2> dlBuf;
    int dlW = 0;
    float dlLp[2] {}, dlHp[2] {}, dlSmooth = -1.0f;
    void delay (float* L, float* R, int n, const FxValues& v)
    {
        const int mask = (int) dlBuf[0].size() - 1;
        const float target = juce::jlimit (8.0f, (float) mask - 4.0f, v.dlSeconds * (float) sr);
        if (dlSmooth < 0.0f) dlSmooth = target;
        const float lpA = 1.0f - std::exp (-twoPi * (800.0f * std::pow (25.0f, v.dlTone)) / (float) sr);
        const float hpA = 1.0f - std::exp (-twoPi * 90.0f / (float) sr);
        const float sm = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            dlSmooth += (target - dlSmooth) * sm;
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                float s = readFrac (dlBuf[(size_t) c], dlW, dlSmooth);
                dlLp[c] += (s - dlLp[c]) * lpA;
                dlHp[c] += (dlLp[c] - dlHp[c]) * hpA;
                y[c] = dlLp[c] - dlHp[c];
            }
            const float inL = L[i], inR = R[i];
            if (v.dlPingPong)
            {
                dlBuf[0][(size_t) dlW] = 0.5f * (inL + inR) + y[1] * v.dlFb;
                dlBuf[1][(size_t) dlW] = y[0] * v.dlFb;
            }
            else
            {
                dlBuf[0][(size_t) dlW] = inL + y[0] * v.dlFb;
                dlBuf[1][(size_t) dlW] = inR + y[1] * v.dlFb;
            }
            dlW = (dlW + 1) & mask;
            L[i] = inL * (1.0f - 0.4f * v.dlMix) + y[0] * v.dlMix;
            R[i] = inR * (1.0f - 0.4f * v.dlMix) + y[1] * v.dlMix;
        }
    }

    //== REVERB: 8-line feedback delay network with damping
    std::array<std::vector<float>, 8> rvLine;
    std::array<std::vector<float>, 2> rvPreBuf, rvDiff;
    std::array<float, 8> rvDamp {};
    int rvW = 0, rvPreW = 0, rvDiffW = 0;
    void reverb (float* L, float* R, int n, const FxValues& v)
    {
        static const int base[8] = { 1031, 1153, 1279, 1381, 1523, 1663, 1811, 1949 };
        const float scale = (0.35f + 1.3f * v.rvSize) * (float) (sr / 44100.0);
        const float rt = 0.3f + 11.0f * v.rvDecay * v.rvDecay;
        int len[8];
        float fb[8];
        for (int k = 0; k < 8; ++k)
        {
            len[k] = juce::jlimit (16, (int) rvLine[(size_t) k].size() - 2, (int) (base[k] * scale));
            fb[k] = std::pow (10.0f, -3.0f * (float) len[k] / (rt * (float) sr));
        }
        const float damp = 1.0f - std::exp (-twoPi * (16000.0f * std::pow (0.06f, v.rvDamp)) / (float) sr);
        const int preMask = (int) rvPreBuf[0].size() - 1, dMask = (int) rvDiff[0].size() - 1;
        const int pre = juce::jlimit (0, preMask - 1, (int) (v.rvPre * 0.001f * (float) sr));
        const int diffLen[2] = { (int) (0.0047 * sr), (int) (0.0071 * sr) };
        for (int i = 0; i < n; ++i)
        {
            rvPreBuf[0][(size_t) rvPreW] = L[i];
            rvPreBuf[1][(size_t) rvPreW] = R[i];
            float in[2] = { rvPreBuf[0][(size_t) ((rvPreW - pre) & preMask)], rvPreBuf[1][(size_t) ((rvPreW - pre) & preMask)] };
            rvPreW = (rvPreW + 1) & preMask;
            for (int c = 0; c < 2; ++c) // a short allpass smears the attack
            {
                const float d = rvDiff[(size_t) c][(size_t) ((rvDiffW - diffLen[c]) & dMask)];
                const float x = in[c] + 0.6f * d;
                rvDiff[(size_t) c][(size_t) rvDiffW] = x;
                in[c] = d - 0.6f * x;
            }
            rvDiffW = (rvDiffW + 1) & dMask;

            float o[8], sum = 0.0f;
            for (int k = 0; k < 8; ++k)
            {
                auto& line = rvLine[(size_t) k];
                const int mask = (int) line.size() - 1;
                float s = line[(size_t) ((rvW - len[k]) & mask)];
                rvDamp[(size_t) k] += (s - rvDamp[(size_t) k]) * damp;
                o[k] = rvDamp[(size_t) k] * fb[k];
                sum += o[k];
            }
            const float h = sum * 0.25f; // Householder mix
            for (int k = 0; k < 8; ++k)
            {
                auto& line = rvLine[(size_t) k];
                const int mask = (int) line.size() - 1;
                line[(size_t) (rvW & mask)] = o[k] - h + (k & 1 ? in[1] : in[0]) * 0.35f;
            }
            rvW++;
            const float wl = o[0] + o[2] + o[4] + o[6], wr = o[1] + o[3] + o[5] + o[7];
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * v.rvWidth;
            L[i] = L[i] * (1.0f - 0.6f * v.rvMix) + (mid + side) * 0.6f * v.rvMix;
            R[i] = R[i] * (1.0f - 0.6f * v.rvMix) + (mid - side) * 0.6f * v.rvMix;
        }
    }
};
} // namespace bully
