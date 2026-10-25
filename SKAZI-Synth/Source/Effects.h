#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include "SynthVoice.h"

namespace Fx
{
    inline float dbToGain (float db) { return std::pow (10.f, db * 0.05f); }
    inline float gainToDb (float g) { return 20.f * std::log10 (std::max (g, 1.0e-6f)); }

    //==========================================================================
    // Runs inside the oversampled voice domain (rate = host rate x oversampling), so the hard types
    // alias far less. Types 0-3 are the original ones (TUBE now also gets the DC blocker).
    class Distortion
    {
    public:
        void reset()
        {
            counter = 0;
            for (auto* a : { held, dcX, dcY, toneLp, hp, post }) a[0] = a[1] = 0.f;
        }

        void process (float* L, float* R, int n, int type, float drive, float mix, float tone, int factor, float rate)
        {
            const float d = std::clamp (drive, 0.f, 1.f);
            const float g = 1.f + d * 20.f;
            const float bits = 16.f - d * 13.f;
            const float q = std::exp2 (bits - 1.f);
            const int hold = (1 + (int) (d * 15.f)) * std::max (1, factor);
            const float comp = 1.f / (1.f + d * 1.2f);

            // new types
            const float gFuzz = 1.f + d * 10.f + d * d * 80.f;
            const float gFold = 1.f + d * 14.f;
            const float gRect = 1.f + d * 12.f;
            const float gDiode = 1.f + d * 30.f;
            const float gScream = 1.f + d * 40.f;
            const float gDestroy = 2.f + d * 30.f + d * d * 300.f;
            const float qDestroy = std::exp2 (7.f - d * 4.5f);
            const int holdDestroy = (1 + (int) (d * 6.f)) * std::max (1, factor);
            const float dcR = 1.f - 6.2831853f * 10.f / rate;
            const float toneHz = 600.f * std::exp2 (std::clamp (tone, 0.f, 1.f) * 5.06f);
            const bool toneOn = tone < 0.995f;
            const float toneA = std::exp (-6.2831853f * std::min (toneHz, rate * 0.45f) / rate);
            const float hpA = std::exp (-6.2831853f * 400.f / rate);
            const float postA = std::exp (-6.2831853f * std::min (7000.f, rate * 0.45f) / rate);

            for (int i = 0; i < n; ++i)
            {
                float* io[2] = { L + i, R + i };
                if (++counter >= (type == DDestroy ? holdDestroy : hold)) counter = 0;
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *io[c];
                    float y;
                    switch (type)
                    {
                        case DTube: y = (std::tanh (x * g + 0.25f) - std::tanh (0.25f)) * 0.9f; break;
                        case DSoft: y = std::tanh (x * g) * 0.8f; break;
                        case DHard: y = std::clamp (x * g, -1.f, 1.f) * 0.75f; break;
                        case DCrush:
                            if (counter == 0) held[c] = std::round (x * q) / q;
                            y = held[c];
                            break;
                        case DFuzz:
                        {
                            // asymmetric, biased: the negative side breaks up harder
                            const float u = x * gFuzz + 0.15f;
                            y = u > 0.f ? 1.f - std::exp (-u) : -(1.f - std::exp (u * 1.8f)) * 0.8f;
                            y *= 0.62f;
                            break;
                        }
                        case DFold:
                        {
                            // triangle wavefolder: louder input folds back into more harmonics
                            float t = (x * gFold + 1.f) * 0.25f;
                            t -= std::floor (t);
                            y = (1.f - 4.f * std::abs (t - 0.5f)) * 0.62f;
                            break;
                        }
                        case DRectify:
                        {
                            // blend towards full-wave rectification: adds the octave above
                            const float sh = std::tanh (x * gRect);
                            y = (sh * (1.f - 0.9f * d) + std::abs (sh) * 1.3f * d) * 0.66f;
                            break;
                        }
                        case DDiode:
                        {
                            const float u = x * gDiode;
                            y = (u >= 0.f ? std::tanh (u) : std::tanh (u * 0.45f) * 1.35f) * 0.6f;
                            break;
                        }
                        case DScream:
                        {
                            // mid push (high-pass at 400 Hz added back), hard clip, rounded top end
                            hp[c] = hp[c] * hpA + x * (1.f - hpA);
                            const float e = x + (x - hp[c]) * (1.f + d * 4.f);
                            const float cl = std::clamp (e * gScream, -1.f, 1.f);
                            post[c] = post[c] * postA + cl * (1.f - postA);
                            y = post[c] * 0.6f;
                            break;
                        }
                        default:
                        {
                            // DESTROY: huge gain, fold, hard clip, few bits, sample-rate crush
                            if (counter == 0)
                            {
                                float t = (x * gDestroy + 1.f) * 0.25f;
                                t -= std::floor (t);
                                const float f = std::clamp ((1.f - 4.f * std::abs (t - 0.5f)) * 1.6f, -1.f, 1.f);
                                held[c] = std::round (f * qDestroy) / qDestroy;
                            }
                            y = held[c] * 0.5f;
                            break;
                        }
                    }
                    if (type <= DHard) y *= comp * 1.6f;
                    if (type >= DFuzz || type == DTube)
                    {
                        // the asymmetric types make DC: block it
                        const float b = y - dcX[c] + dcR * dcY[c];
                        dcX[c] = y; dcY[c] = b;
                        y = b;
                    }
                    if (toneOn)
                    {
                        toneLp[c] = toneLp[c] * toneA + y * (1.f - toneA);
                        y = toneLp[c];
                    }
                    *io[c] = x * (1.f - mix) + y * mix;
                }
            }
        }

    private:
        int counter = 0;
        float held[2] {}, dcX[2] {}, dcY[2] {}, toneLp[2] {}, hp[2] {}, post[2] {};
    };

    //==========================================================================
    // Global filter on the whole mix (after the distortion), smoothed so LFO sweeps don't zipper
    class FxFilter
    {
    public:
        void prepare (double sampleRate) { sr = (float) sampleRate; flt.prepare (sr); smoothed = -1.f; }
        void reset() { flt.reset(); smoothed = -1.f; }

        void process (float* L, float* R, int n, int type, float cutoff, float reso, float drive, float mix)
        {
            static const int map[] = { 3, 5, 7, 8, 1 };   // LP 24, HP 24, BP 24, Notch, Acid ladder
            const int t = map[std::clamp (type, 0, 4)];
            const float target = std::clamp (cutoff, 20.f, sr * 0.45f);
            if (smoothed <= 0.f) smoothed = target;
            const float k = 1.f - std::exp (-1.f / (0.004f * sr));
            for (int i = 0; i < n; ++i)
            {
                smoothed += (target - smoothed) * k;
                float l = L[i], r = R[i];
                flt.process (t, smoothed, reso, drive, l, r);
                L[i] = L[i] * (1.f - mix) + l * mix;
                R[i] = R[i] * (1.f - mix) + r * mix;
            }
        }

    private:
        VFilter flt;
        float sr = 44100.f, smoothed = -1.f;
    };

    //==========================================================================
    // Stereo width with mono bass: sides below MONO are removed (tight psy / techno low end),
    // sides above are scaled by WIDTH (0 = mono, 1 = as is, 2 = extra wide)
    class Width
    {
    public:
        void prepare (double sampleRate) { sr = (float) sampleRate; reset(); }
        void reset() { lp1 = lp2 = 0.f; }

        void process (float* L, float* R, int n, float width, float monoHz)
        {
            const float a = std::exp (-6.2831853f * std::clamp (monoHz, 10.f, 1000.f) / sr);
            for (int i = 0; i < n; ++i)
            {
                const float m = (L[i] + R[i]) * 0.5f, s = (L[i] - R[i]) * 0.5f;
                lp1 = lp1 * a + s * (1.f - a);
                lp2 = lp2 * a + lp1 * (1.f - a);
                const float hs = (s - lp2) * width;
                L[i] = m + hs;
                R[i] = m - hs;
            }
        }

    private:
        float sr = 44100.f, lp1 = 0.f, lp2 = 0.f;
    };

    //==========================================================================
    class ModFx
    {
    public:
        void reset()
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f);
            for (auto& c : ap) for (auto& v : c) v = 0.f;
            fbState[0] = fbState[1] = 0.f;
        }

        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            for (auto& b : buf) b.assign ((size_t) (sr * 0.06f) + 4, 0.f);
            pos = 0; phase = 0.f;
            for (auto& c : ap) for (auto& s : c) s = 0.f;
            fbState[0] = fbState[1] = 0.f;
        }

        void process (float* L, float* R, int n, int type, float rate, float depth, float fb, float mix)
        {
            const int size = (int) buf[0].size();
            const float inc = rate / sr;
            // RING: DEPTH sets the carrier (30 Hz .. 3 kHz), FEEDBACK how far the LFO bends it
            const float ringBase = 30.f * std::exp2 (depth * 6.6f);
            for (int i = 0; i < n; ++i)
            {
                phase += inc; if (phase >= 1.f) phase -= 1.f;
                if (type >= 4)
                {
                    // TREMOLO: DEPTH = how far the level dips. AUTO PAN: DEPTH = how far it swings
                    const float lfo = std::sin (6.2831853f * phase);
                    float gl, gr;
                    if (type == 4) gl = gr = 1.f - depth * (0.5f + 0.5f * lfo);
                    else { const float pan = lfo * depth; gl = 1.f - std::max (0.f, pan); gr = 1.f + std::min (0.f, pan); }
                    L[i] = L[i] * (1.f - mix) + L[i] * gl * mix;
                    R[i] = R[i] * (1.f - mix) + R[i] * gr * mix;
                    continue;
                }
                if (type == 3)
                {
                    const float f = std::min (ringBase * std::exp2 (std::sin (6.2831853f * phase) * fb), sr * 0.45f);
                    ringPhase += f / sr;
                    if (ringPhase >= 1.f) ringPhase -= 1.f;
                    const float carrier = std::sin (6.2831853f * ringPhase);
                    for (float* io : { L + i, R + i })
                        *io = *io * (1.f - mix) + *io * carrier * mix;
                    continue;
                }
                float* io[2] = { L + i, R + i };
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = std::sin (6.2831853f * (phase + (c == 1 ? 0.25f : 0.f)));
                    const float x = *io[c];
                    float wet;
                    if (type == 1)
                    {
                        // Phaser: 6 first-order allpasses swept exponentially
                        const float f = 200.f * std::exp2 ((0.5f + 0.5f * lfo) * (1.f + depth * 4.5f));
                        const float t = std::tan (3.14159265f * std::min (f, sr * 0.45f) / sr);
                        const float a = (t - 1.f) / (t + 1.f);
                        float y = x + fbState[c] * fb * 0.85f;
                        for (int k = 0; k < 6; ++k)
                        {
                            const float out = a * y + ap[c][k];
                            ap[c][k] = y - a * out;
                            y = out;
                        }
                        fbState[c] = y;
                        wet = y;
                    }
                    else
                    {
                        const float base = type == 0 ? 0.015f : 0.0025f;
                        const float range = type == 0 ? 0.008f : 0.002f;
                        const float d = (base + range * depth * (0.5f + 0.5f * lfo)) * sr;
                        float rp = (float) pos - d;
                        while (rp < 0.f) rp += (float) size;
                        const int a = (int) rp, b = (a + 1) % size;
                        const float fr = rp - (float) a;
                        wet = buf[c][(size_t) a] + (buf[c][(size_t) b] - buf[c][(size_t) a]) * fr;
                        buf[c][(size_t) pos] = std::tanh (x + wet * fb * (type == 0 ? 0.5f : 0.9f));
                    }
                    *io[c] = x * (1.f - mix * 0.5f) + wet * mix * (type == 0 ? 0.8f : 0.7f);
                }
                pos = (pos + 1) % size;
            }
        }

    private:
        float sr = 44100.f, phase = 0.f, ringPhase = 0.f;
        std::vector<float> buf[2];
        int pos = 0;
        float ap[2][6] {};
        float fbState[2] {};
    };

    //==========================================================================
    class Delay
    {
    public:
        void reset()
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f);
            damp[0] = damp[1] = 0.f;
        }

        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            for (auto& b : buf) b.assign ((size_t) (sr * 4.f) + 8, 0.f);
            pos = 0; smoothed = 0.f; damp[0] = damp[1] = 0.f;
        }

        void process (float* L, float* R, int n, float timeSec, bool pingPong, float fb, float mix)
        {
            const int size = (int) buf[0].size();
            const float target = std::clamp (timeSec * sr, 1.f, (float) size - 4.f);
            if (smoothed <= 0.f) smoothed = target;
            for (int i = 0; i < n; ++i)
            {
                smoothed += (target - smoothed) * 0.0005f;
                float rp = (float) pos - smoothed;
                while (rp < 0.f) rp += (float) size;
                const int a = (int) rp, b = (a + 1) % size;
                const float fr = rp - (float) a;
                float tap[2];
                for (int c = 0; c < 2; ++c)
                {
                    tap[c] = buf[c][(size_t) a] + (buf[c][(size_t) b] - buf[c][(size_t) a]) * fr;
                    damp[c] += (tap[c] - damp[c]) * 0.6f;
                }
                if (pingPong)
                {
                    buf[0][(size_t) pos] = std::tanh ((L[i] + R[i]) * 0.5f + damp[1] * fb);
                    buf[1][(size_t) pos] = std::tanh (damp[0] * fb);
                }
                else
                {
                    buf[0][(size_t) pos] = std::tanh (L[i] + damp[0] * fb);
                    buf[1][(size_t) pos] = std::tanh (R[i] + damp[1] * fb);
                }
                pos = (pos + 1) % size;
                L[i] += tap[0] * mix;
                R[i] += tap[1] * mix;
            }
        }

    private:
        float sr = 44100.f, smoothed = 0.f, damp[2] {};
        std::vector<float> buf[2];
        int pos = 0;
    };

    //==========================================================================
    // 8-line feedback delay network reverb
    class Reverb
    {
    public:
        void reset()
        {
            for (auto& l : lines) std::fill (l.begin(), l.end(), 0.f);
            std::fill (pre.begin(), pre.end(), 0.f);
            for (auto& d : lp) d = 0.f;
        }

        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            for (auto& l : lines) l.assign ((size_t) (sr * 0.12f) + 8, 0.f);
            for (auto& p : pos) p = 0;
            for (auto& d : lp) d = 0.f;
            pre.assign ((size_t) (sr * 0.05f) + 4, 0.f);
            prePos = 0;
        }

        void process (float* L, float* R, int n, float size, float decay, float dampHz, float mix)
        {
            static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.1f, 67.3f };
            int len[8];
            float g[8];
            const float scale = 0.4f + size * 1.2f;
            for (int k = 0; k < 8; ++k)
            {
                len[k] = std::clamp ((int) (baseMs[k] * 0.001f * scale * sr), 16, (int) lines[k].size() - 2);
                g[k] = std::pow (10.f, -3.f * (float) len[k] / (std::max (decay, 0.1f) * sr));
            }
            const float dampA = std::exp (-6.2831853f * dampHz / sr);
            const int preLen = (int) (0.012f * sr * scale);
            const int preSize = (int) pre.size();

            for (int i = 0; i < n; ++i)
            {
                pre[(size_t) prePos] = (L[i] + R[i]) * 0.5f;
                int rp = prePos - preLen; if (rp < 0) rp += preSize;
                const float in = pre[(size_t) rp] * 0.5f;
                prePos = (prePos + 1) % preSize;

                float out[8], sum = 0.f;
                for (int k = 0; k < 8; ++k)
                {
                    int r = pos[k] - len[k];
                    if (r < 0) r += (int) lines[k].size();
                    float v = lines[k][(size_t) r];
                    lp[k] = v * (1.f - dampA) + lp[k] * dampA;
                    out[k] = lp[k] * g[k];
                    sum += out[k];
                }
                sum *= 2.f / 8.f;
                float wl = 0.f, wr = 0.f;
                for (int k = 0; k < 8; ++k)
                {
                    const float fbk = out[k] - sum + in * ((k & 1) ? -1.f : 1.f);
                    lines[k][(size_t) pos[k]] = fbk;
                    pos[k] = (pos[k] + 1) % (int) lines[k].size();
                    if (k & 1) wr += out[k]; else wl += out[k];
                }
                L[i] = L[i] * (1.f - mix * 0.4f) + wl * 0.6f * mix;
                R[i] = R[i] * (1.f - mix * 0.4f) + wr * 0.6f * mix;
            }
        }

    private:
        float sr = 44100.f;
        std::vector<float> lines[8], pre;
        int pos[8] {}, prePos = 0;
        float lp[8] {};
    };

    //==========================================================================
    class Compressor
    {
    public:
        void reset()
        {
            env = 0.f;
            for (auto& b : bandEnv) b = -100.f;
            for (auto* f : { &lowLP, &lowHP, &midLP, &midHP }) f->reset();
        }

        void prepare (double sampleRate, int blockSize)
        {
            sr = (float) sampleRate;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) std::max (1, blockSize), 2 };
            for (auto* f : { &lowLP, &lowHP, &midLP, &midHP })
            {
                f->prepare (spec);
                f->reset();
            }
            lowLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            lowHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            midLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            midHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            lowLP.setCutoffFrequency (120.f); lowHP.setCutoffFrequency (120.f);
            midLP.setCutoffFrequency (2500.f); midHP.setCutoffFrequency (2500.f);
            env = 0.f;
            for (auto& b : bandEnv) b = -100.f;
        }

        void process (float* L, float* R, int n, int mode, float thrDb, float ratio,
                      float attMs, float relMs, float makeupDb, float depth)
        {
            const float makeup = dbToGain (makeupDb);
            if (mode == 0)
            {
                const float att = std::exp (-1.f / (std::max (attMs, 0.05f) * 0.001f * sr));
                const float rel = std::exp (-1.f / (std::max (relMs, 1.f) * 0.001f * sr));
                for (int i = 0; i < n; ++i)
                {
                    const float lvl = gainToDb (std::max (std::abs (L[i]), std::abs (R[i])));
                    const float over = std::max (0.f, lvl - thrDb);
                    const float gr = over * (1.f - 1.f / std::max (ratio, 1.f));
                    env = gr > env ? att * env + (1.f - att) * gr : rel * env + (1.f - rel) * gr;
                    const float g = dbToGain (-env) * makeup;
                    L[i] *= g; R[i] *= g;
                }
                return;
            }

            // OTT-style 3-band upward + downward compression
            const float att = std::exp (-1.f / (0.003f * sr));
            const float rel = std::exp (-1.f / (0.08f * sr));
            for (int i = 0; i < n; ++i)
            {
                float band[3][2];
                const float in[2] = { L[i], R[i] };
                for (int c = 0; c < 2; ++c)
                {
                    band[0][c] = lowLP.processSample (c, in[c]);
                    const float rest = lowHP.processSample (c, in[c]);
                    band[1][c] = midLP.processSample (c, rest);
                    band[2][c] = midHP.processSample (c, rest);
                }
                float outL = 0.f, outR = 0.f;
                for (int b = 0; b < 3; ++b)
                {
                    const float lvl = gainToDb (std::max (std::abs (band[b][0]), std::abs (band[b][1])));
                    bandEnv[b] = lvl > bandEnv[b] ? att * bandEnv[b] + (1.f - att) * lvl : rel * bandEnv[b] + (1.f - rel) * lvl;
                    const float e = bandEnv[b];
                    float gDb = 0.f;
                    if (e > -18.f) gDb = -(e + 18.f) * 0.75f;                     // downward 4:1
                    else if (e < -42.f && e > -80.f) gDb = std::min (18.f, (-42.f - e) * 0.66f); // upward 3:1
                    const float g = dbToGain (gDb * depth);
                    outL += band[b][0] * g;
                    outR += band[b][1] * g;
                }
                L[i] = outL * makeup;
                R[i] = outR * makeup;
            }
        }

    private:
        float sr = 44100.f, env = 0.f, bandEnv[3] {};
        juce::dsp::LinkwitzRileyFilter<float> lowLP, lowHP, midLP, midHP;
    };

    //==========================================================================
    class Eq
    {
    public:
        void reset() { for (auto& b : bands) for (auto& f : b) f.reset(); }

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& b : bands) for (auto& f : b) f.reset();
            for (auto& c : cache) c = -1.f;
        }

        void process (float* L, float* R, int n, float lf, float lg, float mf, float mq, float mg, float hf, float hg)
        {
            const float now[7] = { lf, lg, mf, mq, mg, hf, hg };
            bool changed = false;
            for (int i = 0; i < 7; ++i) if (std::abs (now[i] - cache[i]) > 1.0e-4f) changed = true;
            if (changed)
            {
                for (int i = 0; i < 7; ++i) cache[i] = now[i];
                using C = juce::dsp::IIR::Coefficients<float>;
                auto low = C::makeLowShelf (sr, lf, 0.707f, dbToGain (lg));
                auto mid = C::makePeakFilter (sr, mf, mq, dbToGain (mg));
                auto high = C::makeHighShelf (sr, std::min (hf, (float) sr * 0.45f), 0.707f, dbToGain (hg));
                for (int c = 0; c < 2; ++c)
                {
                    *bands[0][c].coefficients = *low;
                    *bands[1][c].coefficients = *mid;
                    *bands[2][c].coefficients = *high;
                }
            }
            const bool active[3] = { std::abs (lg) > 0.05f, std::abs (mg) > 0.05f, std::abs (hg) > 0.05f };
            for (int i = 0; i < n; ++i)
                for (int b = 0; b < 3; ++b)
                    if (active[b])
                    {
                        L[i] = bands[b][0].processSample (L[i]);
                        R[i] = bands[b][1].processSample (R[i]);
                    }
        }

    private:
        double sr = 44100.0;
        juce::dsp::IIR::Filter<float> bands[3][2] {
            { juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)), juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)) },
            { juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)), juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)) },
            { juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)), juce::dsp::IIR::Filter<float> (new juce::dsp::IIR::Coefficients<float> (1, 0, 1, 0)) } };
        float cache[7] {};
    };
}
