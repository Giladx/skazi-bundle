#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Four built-in morphing wavetables (16 frames each), stored as band-limited
// mip-maps so high notes don't alias. Built once with an inverse FFT.
class Wavetables
{
public:
    static constexpr int N = 2048, kFrames = 16, kMips = 11, kTables = 4, kMaxHarm = 1024;

    static const Wavetables& get()
    {
        static Wavetables w;
        return w;
    }

    // pos 0..1 morphs through the frames; dt = phase increment (freq / sr)
    float sample (int t, float pos, float ph, float dt) const
    {
        const float h = 0.5f / std::max (dt, 1.0e-7f);
        int m = 0;
        while (m < kMips - 1 && (float) (kMaxHarm >> m) > h) ++m;

        const float fpos = std::clamp (pos, 0.f, 1.f) * (kFrames - 1);
        const int f0 = (int) fpos;
        const int f1 = std::min (f0 + 1, kFrames - 1);
        const float ff = fpos - (float) f0;

        const float idx = ph * (float) N;
        const int i0 = std::clamp ((int) idx, 0, N - 1);
        const float fr = idx - (float) i0;
        const float* a = table (t, f0, m);
        const float* b = table (t, f1, m);
        const float va = a[i0] + (a[i0 + 1] - a[i0]) * fr;
        const float vb = b[i0] + (b[i0 + 1] - b[i0]) * fr;
        return va + (vb - va) * ff;
    }

    // Mip level and frame pair resolved once per oscillator per sample; read() is then cheap
    struct Reader
    {
        const float* a = nullptr;
        const float* b = nullptr;
        float ff = 0.f;

        float read (float ph) const noexcept
        {
            const float idx = ph * (float) N;
            const int i0 = std::clamp ((int) idx, 0, N - 1);
            const float fr = idx - (float) i0;
            const float va = a[i0] + (a[i0 + 1] - a[i0]) * fr;
            const float vb = b[i0] + (b[i0 + 1] - b[i0]) * fr;
            return va + (vb - va) * ff;
        }
    };

    Reader reader (int t, float pos, float maxDt) const
    {
        const float h = 0.5f / std::max (maxDt, 1.0e-7f);
        int m = 0;
        while (m < kMips - 1 && (float) (kMaxHarm >> m) > h) ++m;
        const float fpos = std::clamp (pos, 0.f, 1.f) * (kFrames - 1);
        const int f0 = (int) fpos;
        return { table (t, f0, m), table (t, std::min (f0 + 1, kFrames - 1), m), fpos - (float) f0 };
    }

    // One frame of a table at full bandwidth, for drawing
    const float* frame (int t, float pos) const
    {
        return table (t, (int) std::round (std::clamp (pos, 0.f, 1.f) * (kFrames - 1)), 0);
    }

private:
    static constexpr int stride = N + 1;

    const float* table (int t, int f, int m) const
    {
        return data.data() + (((size_t) t * kFrames + (size_t) f) * kMips + (size_t) m) * stride;
    }

public:
    // Builds the band-limited mip levels of one frame from its harmonics (sin / cos per harmonic)
    static void buildMips (const std::vector<float>& sinA, const std::vector<float>& cosA, float* frameBase)
    {
        juce::dsp::FFT fft (11);
        std::vector<float> buf ((size_t) N * 2);
        float scale = 1.f;
        for (int m = 0; m < kMips; ++m)
        {
            std::fill (buf.begin(), buf.end(), 0.f);
            const int maxH = kMaxHarm >> m;
            for (int n = 1; n <= maxH && n < N / 2; ++n)
            {
                buf[(size_t) (2 * n)]           =  cosA[(size_t) n] * N * 0.5f;
                buf[(size_t) (2 * n + 1)]       = -sinA[(size_t) n] * N * 0.5f;
                buf[(size_t) (2 * (N - n))]     =  cosA[(size_t) n] * N * 0.5f;
                buf[(size_t) (2 * (N - n) + 1)] =  sinA[(size_t) n] * N * 0.5f;
            }
            fft.performRealOnlyInverseTransform (buf.data());
            if (m == 0)
            {
                float peak = 0.f;
                for (int i = 0; i < N; ++i) peak = std::max (peak, std::abs (buf[(size_t) i]));
                scale = peak > 1.0e-6f ? 1.f / peak : 1.f;
            }
            float* dst = frameBase + (size_t) m * stride;
            for (int i = 0; i < N; ++i) dst[i] = buf[(size_t) i] * scale;
            dst[N] = dst[0];
        }
    }

    // A wavetable loaded from the user's WAV file: 16 frames of 2048 samples, same layout as the built-ins
    class UserTable
    {
    public:
        // frames: kFrames * N samples (one cycle each)
        void build (const std::vector<float>& frames)
        {
            data.assign ((size_t) kFrames * kMips * stride, 0.f);
            juce::dsp::FFT fft (11);
            std::vector<float> buf ((size_t) N * 2);
            std::vector<float> sinA (kMaxHarm + 1), cosA (kMaxHarm + 1);
            for (int f = 0; f < kFrames; ++f)
            {
                std::fill (buf.begin(), buf.end(), 0.f);
                for (int i = 0; i < N; ++i) buf[(size_t) i] = frames[(size_t) (f * N + i)];
                fft.performRealOnlyForwardTransform (buf.data());
                std::fill (sinA.begin(), sinA.end(), 0.f);
                std::fill (cosA.begin(), cosA.end(), 0.f);
                for (int n = 1; n <= kMaxHarm && n < N / 2; ++n)
                {
                    cosA[(size_t) n] = buf[(size_t) (2 * n)] * 2.f / N;
                    sinA[(size_t) n] = -buf[(size_t) (2 * n + 1)] * 2.f / N;
                }
                buildMips (sinA, cosA, data.data() + (size_t) f * kMips * stride);
            }
        }

        Reader reader (float pos, float maxDt) const
        {
            const float h = 0.5f / std::max (maxDt, 1.0e-7f);
            int m = 0;
            while (m < kMips - 1 && (float) (kMaxHarm >> m) > h) ++m;
            const float fpos = std::clamp (pos, 0.f, 1.f) * (kFrames - 1);
            const int f0 = (int) fpos;
            return { table (f0, m), table (std::min (f0 + 1, kFrames - 1), m), fpos - (float) f0 };
        }
        const float* frame (float pos) const { return table ((int) std::round (std::clamp (pos, 0.f, 1.f) * (kFrames - 1)), 0); }

    private:
        const float* table (int f, int m) const { return data.data() + ((size_t) f * kMips + (size_t) m) * stride; }
        std::vector<float> data;
    };

private:
    Wavetables()
    {
        data.assign ((size_t) kTables * kFrames * kMips * stride, 0.f);
        juce::dsp::FFT fft (11);
        std::vector<float> buf ((size_t) N * 2);
        std::vector<float> sinA (kMaxHarm + 1), cosA (kMaxHarm + 1);

        for (int t = 0; t < kTables; ++t)
            for (int f = 0; f < kFrames; ++f)
            {
                spectrum (t, f / (float) (kFrames - 1), sinA, cosA);
                float scale = 1.f;
                for (int m = 0; m < kMips; ++m)
                {
                    std::fill (buf.begin(), buf.end(), 0.f);
                    const int maxH = kMaxHarm >> m;
                    for (int n = 1; n <= maxH && n < N / 2; ++n)
                    {
                        // x(t) = c cos + s sin  ->  X[n] = (c - i s) * N/2, mirrored conjugate
                        buf[(size_t) (2 * n)]           =  cosA[(size_t) n] * N * 0.5f;
                        buf[(size_t) (2 * n + 1)]       = -sinA[(size_t) n] * N * 0.5f;
                        buf[(size_t) (2 * (N - n))]     =  cosA[(size_t) n] * N * 0.5f;
                        buf[(size_t) (2 * (N - n) + 1)] =  sinA[(size_t) n] * N * 0.5f;
                    }
                    fft.performRealOnlyInverseTransform (buf.data());

                    if (m == 0)
                    {
                        float peak = 0.f;
                        for (int i = 0; i < N; ++i) peak = std::max (peak, std::abs (buf[(size_t) i]));
                        scale = peak > 0.f ? 1.f / peak : 1.f;
                    }
                    float* dst = data.data() + (((size_t) t * kFrames + (size_t) f) * kMips + (size_t) m) * stride;
                    for (int i = 0; i < N; ++i) dst[i] = buf[(size_t) i] * scale;
                    dst[N] = dst[0];
                }
            }
    }

    static void spectrum (int t, float x, std::vector<float>& s, std::vector<float>& c)
    {
        std::fill (s.begin(), s.end(), 0.f);
        std::fill (c.begin(), c.end(), 0.f);
        const float pi = juce::MathConstants<float>::pi;

        auto basic = [pi] (int shape, int n) -> float
        {
            switch (shape)
            {
                case 0: return n == 1 ? 1.f : 0.f;                                                    // sine
                case 1: return (n & 1) ? 8.f / (pi * pi) * ((((n - 1) / 2) & 1) ? -1.f : 1.f) / (float) (n * n) : 0.f; // triangle
                case 2: return 2.f / pi * ((n & 1) ? 1.f : -1.f) / (float) n;                        // saw
                default: return (n & 1) ? 4.f / (pi * (float) n) : 0.f;                             // square
            }
        };

        if (t == 0)
        {
            // Sine -> Triangle -> Saw -> Square
            const float seg = x * 3.f;
            const int a = std::min (2, (int) seg);
            const float fr = seg - (float) a;
            for (int n = 1; n <= kMaxHarm; ++n)
                s[(size_t) n] = basic (a, n) * (1.f - fr) + basic (a + 1, n) * fr;
        }
        else if (t == 1)
        {
            // Pulse width sweep 50% -> 4%
            const float w = 0.5f - x * 0.46f;
            for (int n = 1; n <= kMaxHarm; ++n)
                c[(size_t) n] = 2.f / ((float) n * pi) * std::sin ((float) n * pi * w);
        }
        else if (t == 2)
        {
            // Vowel formants a-e-i-o-u (positions in harmonic numbers)
            const float f1s[] = { 7.f, 3.6f, 2.5f, 4.f, 3.f };
            const float f2s[] = { 10.5f, 15.5f, 21.f, 7.3f, 6.4f };
            const float v = x * 4.f;
            const int a = std::min (3, (int) v);
            const float fr = v - (float) a;
            const float F1 = f1s[a] + (f1s[a + 1] - f1s[a]) * fr;
            const float F2 = f2s[a] + (f2s[a + 1] - f2s[a]) * fr;
            for (int n = 1; n <= kMaxHarm; ++n)
            {
                const float fn = (float) n;
                const float g1 = std::exp (-std::pow ((fn - F1) / (1.2f + F1 * 0.15f), 2.f));
                const float g2 = std::exp (-std::pow ((fn - F2) / (1.5f + F2 * 0.15f), 2.f));
                s[(size_t) n] = (g1 + 0.7f * g2 + 0.02f) / std::pow (fn, 0.7f);
            }
        }
        else
        {
            // Digital: harmonic count and brightness grow across the table
            const int count = 2 + (int) (x * 80.f);
            for (int n = 1; n <= count; ++n)
            {
                float amp = 1.f / std::pow ((float) n, 1.2f - 0.6f * x);
                if (n % 3 == 0) amp *= 1.8f;
                if (n % 7 == 0) amp *= 0.3f;
                s[(size_t) n] = amp * (((n * 7) % 5) < 2 ? -1.f : 1.f);
            }
        }
    }

    std::vector<float> data;
};
