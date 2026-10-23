#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace Fx
{
    inline float dbToGain (float db) { return std::pow (10.f, db * 0.05f); }
    inline float gainToDb (float g) { return 20.f * std::log10 (std::max (g, 1.0e-6f)); }

    //==========================================================================
    class Distortion
    {
    public:
        void reset() { counter = 0; held[0] = held[1] = 0.f; }

        void process (float* L, float* R, int n, int type, float drive, float mix)
        {
            const float g = 1.f + drive * 20.f;
            const float bits = 16.f - drive * 13.f;
            const float q = std::exp2 (bits - 1.f);
            const int hold = 1 + (int) (drive * 15.f);
            const float comp = 1.f / (1.f + drive * 1.2f);
            for (int i = 0; i < n; ++i)
            {
                float* io[2] = { L + i, R + i };
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *io[c];
                    float y;
                    switch (type)
                    {
                        case 0: y = (std::tanh (x * g + 0.25f) - std::tanh (0.25f)) * 0.9f; break;
                        case 1: y = std::tanh (x * g) * 0.8f; break;
                        case 2: y = std::clamp (x * g, -1.f, 1.f) * 0.75f; break;
                        default:
                            if (c == 0 && ++counter >= hold) counter = 0;
                            if (counter == 0) held[c] = std::round (x * q) / q;
                            y = held[c];
                            break;
                    }
                    if (type != 3) y *= comp * 1.6f;
                    *io[c] = x * (1.f - mix) + y * mix;
                }
            }
        }

    private:
        int counter = 0;
        float held[2] {};
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
            for (int i = 0; i < n; ++i)
            {
                phase += inc; if (phase >= 1.f) phase -= 1.f;
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
        float sr = 44100.f, phase = 0.f;
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
