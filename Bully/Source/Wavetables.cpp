#include "Wavetables.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace bully
{
namespace
{
    constexpr int N = 2048;              // generation size
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    using Cx = juce::dsp::Complex<float>;

    float saw (float x) { return 1.0f - 2.0f * x; }

    // A frame is described either in time (one cycle, x 0..1) or as harmonic amplitudes/phases
    struct Frame
    {
        std::vector<float> time;          // N samples, or empty
        std::vector<float> amp, phase;    // harmonic k at index k (k >= 1)
    };

    Frame timeFrame (const std::function<float (float)>& f)
    {
        Frame fr;
        fr.time.resize (N);
        for (int i = 0; i < N; ++i) fr.time[(size_t) i] = f ((float) i / N);
        return fr;
    }

    Frame specFrame (int maxH, const std::function<float (int)>& amp, const std::function<float (int)>& phase = {})
    {
        Frame fr;
        fr.amp.assign ((size_t) maxH + 1, 0.0f);
        fr.phase.assign ((size_t) maxH + 1, 0.0f);
        for (int k = 1; k <= maxH; ++k)
        {
            fr.amp[(size_t) k] = amp (k);
            fr.phase[(size_t) k] = phase ? phase (k) : 0.0f;
        }
        return fr;
    }

    // frame t (0..1) of table `table`
    Frame makeFrame (int table, float t, int frameIndex)
    {
        switch (table)
        {
            case 0: // BASIC SHAPES: sine -> triangle -> saw -> square
                return timeFrame ([t] (float x)
                {
                    const float s = std::sin (twoPi * x), tr = 1.0f - 4.0f * std::abs (x - 0.5f);
                    const float tri = -tr, sw = saw (x), sq = x < 0.5f ? 1.0f : -1.0f;
                    const float p = t * 3.0f;
                    if (p < 1.0f) return s + (tri - s) * p;
                    if (p < 2.0f) return tri + (sw - tri) * (p - 1.0f);
                    return sw + (sq - sw) * juce::jmin (1.0f, p - 2.0f);
                });
            case 1: // BULLY SAW: dark to full-bright saw
            {
                const float k = 0.7f * std::pow (1.0f - t, 2.5f);
                return specFrame (511, [k] (int h) { return std::exp (-(h - 1) * k) / h; });
            }
            case 2: // GROWL FM
                return timeFrame ([t] (float x)
                {
                    const float i1 = 6.0f * t, i2 = 2.0f * t * t;
                    return std::sin (twoPi * x + i1 * std::sin (twoPi * 2.0f * x) + i2 * std::sin (twoPi * 3.0f * x));
                });
            case 3: // SUB SINE: sine that drives harder
                return timeFrame ([t] (float x)
                {
                    const float g = 1.0f + 7.0f * t;
                    return std::tanh (g * std::sin (twoPi * x)) / std::tanh (g) + 0.3f * t * std::sin (2.0f * twoPi * x);
                });
            case 4: // SQUARE PWM
            {
                const float w = 0.5f - 0.45f * t;
                return timeFrame ([w] (float x) { return (x < w ? 1.0f : -1.0f) - (2.0f * w - 1.0f); });
            }
            case 5: // VOCAL: formants A E I O U
            {
                static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                const float p = t * 4.0f;
                const int a = juce::jmin (3, (int) p);
                const float u = p - a;
                float f[3];
                for (int i = 0; i < 3; ++i) f[i] = F[a][i] + (F[a + 1][i] - F[a][i]) * u;
                return specFrame (200, [f] (int h)
                {
                    const float hz = h * 110.0f;
                    float g = 0.0f;
                    const float bw[3] = { 90.0f, 130.0f, 170.0f }, lv[3] = { 1.0f, 0.6f, 0.3f };
                    for (int i = 0; i < 3; ++i) g += lv[i] * std::exp (-std::pow ((hz - f[i]) / bw[i], 2.0f));
                    return g + 0.04f / h;
                }, [] (int h) { return 0.5f * h * h; });
            }
            case 6: // METALLIC: stretched partials
            {
                Frame fr;
                fr.amp.assign (512, 0.0f);
                fr.phase.assign (512, 0.0f);
                for (int k = 1; k <= 40; ++k)
                {
                    const int h = (int) std::round (std::pow ((float) k, 1.0f + 0.8f * t));
                    if (h < 512) { fr.amp[(size_t) h] += 1.0f / k; fr.phase[(size_t) h] = (float) k; }
                }
                return fr;
            }
            case 7: // DIGITAL SYNC: hard-synced saw sweeping up
            {
                const float r = 1.0f + 7.0f * t;
                return timeFrame ([r] (float x) { const float p = x * r; return saw (p - std::floor (p)); });
            }
            case 8: // ACID SWEEP: saw with a resonant peak moving up
            {
                const float hc = 2.0f + t * 40.0f, w = 1.0f + hc * 0.15f;
                return specFrame (511, [hc, w] (int h) { return (1.0f + 6.0f * std::exp (-std::pow ((h - hc) / w, 2.0f))) / h; });
            }
            case 9: // HOLLOW ORGAN: drawbars pulled out one by one
            {
                static const int bars[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                Frame fr;
                fr.amp.assign (17, 0.0f);
                fr.phase.assign (17, 0.0f);
                for (int i = 0; i < 9; ++i)
                    fr.amp[(size_t) bars[i]] = juce::jlimit (0.0f, 1.0f, t * 9.0f - i + 1.0f) / std::sqrt ((float) i + 1.0f);
                return fr;
            }
            case 10: // SPECTRAL NOISE: random partials, more of them each frame
            {
                juce::Random r (1234 + frameIndex);
                const int maxH = 16 + (int) (t * 480.0f);
                return specFrame (511, [&r, maxH] (int h) { const float v = r.nextFloat(); return h <= maxH ? v * v / std::sqrt ((float) h) : 0.0f; },
                                  [&r] (int) { return r.nextFloat() * twoPi; });
            }
            default: // FOLD TEAR: wavefolded sine
                return timeFrame ([t] (float x)
                {
                    const float g = 1.0f + 9.0f * t;
                    return std::sin (g * std::sin (twoPi * x) + 0.2f * t);
                });
        }
    }
}

WaveBank::WaveBank()
{
    juce::dsp::FFT fft (11);
    std::vector<Cx> in ((size_t) N), spec ((size_t) N), work ((size_t) N), out ((size_t) N);

    size_t total = 0;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < kMips; ++m)
            total += (size_t) kFrames * (size_t) (size (m) + 1);
    data.resize (total);
    offsets.resize ((size_t) (12 * kMips * kFrames));

    size_t at = 0;
    for (int table = 0; table < 12; ++table)
    {
        std::vector<std::vector<float>> mips ((size_t) kMips);
        for (int fi = 0; fi < kFrames; ++fi)
        {
            const auto fr = makeFrame (table, (float) fi / (kFrames - 1), fi);
            if (! fr.time.empty())
            {
                for (int i = 0; i < N; ++i) in[(size_t) i] = { fr.time[(size_t) i], 0.0f };
                fft.perform (in.data(), spec.data(), false);
            }
            else
            {
                std::fill (spec.begin(), spec.end(), Cx { 0.0f, 0.0f });
                for (size_t k = 1; k < fr.amp.size() && k < (size_t) N / 2; ++k)
                {
                    const float a = fr.amp[k] * 0.5f, ph = fr.phase[k];
                    spec[k] = { a * std::sin (ph), -a * std::cos (ph) };
                    spec[(size_t) N - k] = { a * std::sin (ph), a * std::cos (ph) };
                }
            }

            float scale = 0.0f;
            for (int m = 0; m < kMips; ++m)
            {
                const int H = harmonics (m), sz = size (m), stride = N / sz;
                std::fill (work.begin(), work.end(), Cx { 0.0f, 0.0f });
                for (int k = 1; k <= H && k < N / 2; ++k)
                {
                    work[(size_t) k] = spec[(size_t) k];
                    work[(size_t) (N - k)] = spec[(size_t) (N - k)];
                }
                fft.perform (work.data(), out.data(), true);
                if (m == 0)
                {
                    float peak = 1.0e-9f;
                    for (auto& c : out) peak = juce::jmax (peak, std::abs (c.real()));
                    scale = 1.0f / peak;
                }
                offsets[(size_t) ((table * kMips + m) * kFrames + fi)] = at;
                for (int i = 0; i < sz; ++i) data[at + (size_t) i] = out[(size_t) (i * stride)].real() * scale;
                data[at + (size_t) sz] = data[at]; // guard sample for interpolation
                at += (size_t) sz + 1;
            }
        }
    }
}

const WaveBank& WaveBank::get()
{
    static const WaveBank bank;
    return bank;
}

float WaveBank::preview (int table, float pos, float x) const
{
    const float fp = juce::jlimit (0.0f, 1.0f, pos) * (kFrames - 1);
    const int f0 = (int) fp, f1 = juce::jmin (kFrames - 1, f0 + 1);
    const float u = fp - f0;
    const int sz = size (2);
    const float idx = (x - std::floor (x)) * sz;
    const int i = (int) idx;
    const float w = idx - i;
    auto rd = [&] (int f) { const float* c = cycle (table, 2, f); return c[i] + (c[i + 1] - c[i]) * w; };
    return rd (f0) + (rd (f1) - rd (f0)) * u;
}
} // namespace bully
