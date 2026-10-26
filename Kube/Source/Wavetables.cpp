#include "Wavetables.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace kube
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi, pi = juce::MathConstants<double>::pi;

    double frac (double x) { return x - std::floor (x); }
    double bell (double k, double centre, double width) { return std::exp (-0.5 * std::pow ((k - centre) / width, 2.0)); }

    // Repeatable 0..1 noise for SPECTRAL NOISE
    double hash01 (int a, int b)
    {
        juce::uint32 h = (juce::uint32) a * 2654435761u ^ (juce::uint32) b * 2246822519u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (h & 0xffffff) / (double) 0xffffff;
    }

    bool isPrime (int k)
    {
        if (k < 2) return false;
        for (int d = 2; d * d <= k; ++d) if (k % d == 0) return false;
        return true;
    }

    // Vowel formants as harmonic numbers (a voice around 110 Hz): a > e > i > o > u
    double vowel (double k, double t, double widthScale)
    {
        static const double v[5][3] = { { 7.3, 10.5, 26.0 }, { 3.6, 14.5, 24.5 }, { 3.2, 20.0, 27.0 }, { 4.1, 7.5, 22.0 }, { 2.9, 6.2, 21.0 } };
        const double p = t * 4.0;
        const int i = juce::jmin (3, (int) p);
        const double f = p - i, w[3] = { 1.0, 0.6, 0.25 };
        double a = 0.0;
        for (int n = 0; n < 3; ++n)
        {
            const double c = v[i][n] + f * (v[i + 1][n] - v[i][n]);
            a += w[n] * bell (k, c, (0.6 + 0.07 * c) * widthScale);
        }
        return a;
    }

    enum Bank { Growl, Yoy, Robot, Talkbox, Choir, MetalCube, Glass, SpectralShift, ReeseComb, HollowComb, PsySquelch, AcidSquare,
                LaserChirp, PsyRes, PhaseDist, FoldSine, DigitalGrit, Stairs, WarmBrass, OrganDraw, SawStack, SpectralNoise, FmCube, BellCluster };

    bool isAdditive (int table)
    {
        switch (table)
        {
            case LaserChirp: case PsyRes: case PhaseDist: case FoldSine: case DigitalGrit: case Stairs: case FmCube: return false;
            default: return true;
        }
    }

    // Harmonic k's amplitude at morph position t, for the tables built from spectra
    double harmonic (int table, double t, int k, int frame)
    {
        const double kd = k;
        switch (table)
        {
            case Growl:   { const double c = 3.0 + t * 37.0; return (0.35 + 2.5 * bell (kd, c, 1.0 + 0.15 * c)) / kd; }
            case Yoy:     return (bell (kd, 4.0 + t * 10.0, 1.5) + 0.7 * bell (kd, 28.0 - t * 16.0, 2.5)) / std::sqrt (kd) + 0.02 / kd;
            case Robot:   { const double c = 5.0 + t * 30.0; return k % 2 == 1 ? (0.3 + 2.0 * bell (kd, c, 2.0)) / kd : 0.0; }
            case Talkbox: return vowel (kd, t, 0.7) / std::sqrt (kd) + 0.01 / kd;
            case Choir:   return (vowel (kd * 1.03, t, 1.6) + vowel (kd, t, 1.6) + vowel (kd * 0.97, t, 1.6)) / std::pow (kd, 0.8);
            case MetalCube:
            {
                const double p1 = (k % 3 == 0 ? 1.0 : 0.0) + (k % 5 == 0 ? 1.0 : 0.0);
                const double p2 = (k % 7 == 0 ? 1.0 : 0.0) + (k % 4 == 1 && k > 8 ? 0.7 : 0.0);
                return (k == 1 ? 1.0 : 0.0) + ((1.0 - t) * p1 + t * p2 + 0.03) / std::pow (kd, 0.7);
            }
            case Glass:   { const double c = 5.0 + t * 25.0; return (k == 1 ? 1.0 : 0.0) + 0.6 * bell (kd, c, 1.2) + 0.3 * bell (kd, 2.0 * c, 2.0); }
            case SpectralShift: { const double d = kd - t * 24.0; return d <= 0.0 ? 0.0 : juce::jmin (1.0, d) / juce::jmax (1.0, d); }
            case ReeseComb:  return std::abs (std::cos (pi * kd * (0.02 + t * 0.3))) / kd;
            case HollowComb: { const double s = std::sin (pi * kd / (2.0 + t * 6.0)); return s * s / kd; }
            case PsySquelch:
            case AcidSquare:
            {
                if (table == AcidSquare && k % 2 == 0) return 0.0;
                const double c = table == AcidSquare ? 3.0 + t * 40.0 : 2.0 + t * 46.0;
                return (kd <= c ? 1.0 : std::exp (-(kd - c) * 0.35)) * (1.0 + 6.0 * bell (kd, c, 0.8)) / kd;
            }
            case WarmBrass: { const double c = 2.0 + t * 10.0; return bell (kd, c, 2.0 + 0.4 * c) / kd + 0.15 / kd; }
            case OrganDraw:
            {
                static const int harms[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                static const double regs[4][9] = { { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 6, 8, 0, 4, 0, 0, 2 },
                                                   { 8, 0, 8, 0, 8, 0, 8, 0, 8 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
                const double p = t * 3.0;
                const int r = juce::jmin (2, (int) p);
                const double f = p - r;
                for (int n = 0; n < 9; ++n)
                    if (harms[n] == k) return (regs[r][n] * (1.0 - f) + regs[r + 1][n] * f) / 8.0;
                return 0.0;
            }
            case SawStack:
                return 1.0 / kd + (k % 2 == 0 ? t * 2.0 / kd : 0.0) + (k % 3 == 0 ? juce::jmax (0.0, t - 0.4) * 2.0 / kd : 0.0)
                     + (k % 4 == 0 ? juce::jmax (0.0, t - 0.7) * 3.0 / kd : 0.0);
            case SpectralNoise:
            {
                // four random spectra, morphed one into the next
                const double p = t * 3.0;
                const int r = juce::jmin (2, (int) p);
                const double f = p - r;
                juce::ignoreUnused (frame);
                return ((1.0 - f) * hash01 (r, k) + f * hash01 (r + 1, k)) / std::pow (kd, 0.6);
            }
            case BellCluster:
                return (k == 1 ? 1.0 : 0.0) + (isPrime (k) && k > 2 ? std::exp (-kd / (2.0 + t * 20.0)) : 0.0);
            default: return 0.0;
        }
    }

    // One cycle sample for the tables drawn in time
    double shape (int table, double t, double x)
    {
        switch (table)
        {
            case LaserChirp: return std::sin (twoPi * (x + t * 14.0 * x * x)) * std::sqrt (std::sin (pi * x));
            case PsyRes:     return std::sin (twoPi * x * (1.0 + t * 14.0)) * (1.0 - x);
            case PhaseDist: // Casio CZ style: a cosine bent into a saw
            {
                const double d = 0.5 - t * 0.48;
                const double p = x < d ? x / (2.0 * d) : 0.5 + (x - d) / (2.0 * (1.0 - d));
                return -std::cos (twoPi * p);
            }
            case FoldSine:    return std::sin (pi * 0.5 * (1.0 + t * 7.0) * std::sin (twoPi * x));
            case DigitalGrit: { const double n = juce::jmax (4.0, std::round (64.0 * std::pow (1.0 / 16.0, t))); return std::sin (twoPi * std::floor (x * n) / n); }
            case Stairs:      { const double n = juce::jmax (3.0, std::round (32.0 * std::pow (3.0 / 32.0, t))); return std::round ((1.0 - 2.0 * x) * n) / n; }
            case FmCube:      { const double i = t * 4.0; return std::sin (twoPi * x + i * std::sin (twoPi * 3.0 * x) + i * 0.5 * std::sin (twoPi * 5.0 * x)); }
            default:          return 0.0;
        }
    }

    std::vector<std::unique_ptr<Wavetable>> buildBank()
    {
        std::vector<std::unique_ptr<Wavetable>> bank;
        juce::dsp::FFT fft (11);
        std::vector<float> cycles ((size_t) kBankFrames * kTableSize), spec ((size_t) kTableSize * 2);
        for (int t = 0; t < kBankSize; ++t)
        {
            for (int f = 0; f < kBankFrames; ++f)
            {
                const double morph = f / (double) (kBankFrames - 1);
                float* out = cycles.data() + (size_t) f * kTableSize;
                if (isAdditive (t))
                {
                    std::fill (spec.begin(), spec.end(), 0.0f);
                    for (int k = 1; k < kTableSize / 2; ++k)
                        spec[(size_t) k * 2 + 1] = (float) -harmonic (t, morph, k, f); // sine phase
                    fft.performRealOnlyInverseTransform (spec.data());
                    std::copy (spec.begin(), spec.begin() + kTableSize, out);
                }
                else
                    for (int i = 0; i < kTableSize; ++i)
                        out[i] = (float) shape (t, morph, i / (double) kTableSize);
            }
            bank.push_back (std::make_unique<Wavetable> (cycles, kBankFrames, tableNames()[t], true));
        }
        return bank;
    }

    // Serum writes "<!>2048 ..." into a "clm " chunk: the samples per frame
    int serumFrameSize (const juce::File& file)
    {
        juce::MemoryBlock head;
        if (auto in = file.createInputStream()) in->readIntoMemoryBlock (head, 65536);
        const auto* d = static_cast<const char*> (head.getData());
        for (size_t i = 0; i + 8 < head.getSize(); ++i)
            if (std::memcmp (d + i, "clm ", 4) == 0)
            {
                const juce::String text (d + i + 8, juce::jmin ((size_t) 32, head.getSize() - i - 8));
                if (text.startsWith ("<!>"))
                    return juce::jlimit (16, 65536, text.substring (3).getIntValue());
            }
        return kTableSize;
    }
}

const juce::StringArray& tableNames()
{
    static const juce::StringArray n { "GROWL", "YOY", "ROBOT", "TALKBOX", "CHOIR", "METAL CUBE", "GLASS", "SPECTRAL SHIFT",
                                       "REESE COMB", "HOLLOW COMB", "PSY SQUELCH", "ACID SQUARE", "LASER CHIRP", "PSY RES",
                                       "PHASE DIST", "FOLD SINE", "DIGITAL GRIT", "STAIRS", "WARM BRASS", "ORGAN DRAW",
                                       "SAW STACK", "SPECTRAL NOISE", "FM CUBE", "BELL CLUSTER", "USER" };
    return n;
}

Wavetable::Wavetable (const std::vector<float>& source, int numFrames, juce::String tableName, bool normalisePerFrame)
    : frames (juce::jmax (1, numFrames)), name (std::move (tableName)), cycles (source)
{
    cycles.resize ((size_t) frames * kTableSize, 0.0f);
    data.resize ((size_t) frames * kLevels * (kTableSize + 1));
    juce::dsp::FFT fft (11);
    std::vector<float> spec ((size_t) kTableSize * 2), work ((size_t) kTableSize * 2);
    std::vector<float> peaks ((size_t) frames, 1.0e-9f);

    for (int f = 0; f < frames; ++f)
    {
        std::fill (spec.begin(), spec.end(), 0.0f);
        std::copy (cycles.begin() + (std::ptrdiff_t) f * kTableSize, cycles.begin() + (std::ptrdiff_t) (f + 1) * kTableSize, spec.begin());
        fft.performRealOnlyForwardTransform (spec.data(), true);
        spec[0] = spec[1] = 0.0f; // no DC
        for (int l = 0; l < kLevels; ++l)
        {
            const int maxH = 1024 >> l;
            work = spec;
            for (int k = maxH + 1; k <= kTableSize / 2; ++k)
                work[(size_t) k * 2] = work[(size_t) k * 2 + 1] = 0.0f;
            fft.performRealOnlyInverseTransform (work.data());
            auto* dst = const_cast<float*> (frame (f, l));
            std::copy (work.begin(), work.begin() + kTableSize, dst);
            if (l == 0)
                for (int i = 0; i < kTableSize; ++i) peaks[(size_t) f] = juce::jmax (peaks[(size_t) f], std::abs (dst[i]));
        }
    }

    // the bank plays every frame equally loud; a loaded table keeps its own frame-to-frame levels
    const float tablePeak = *std::max_element (peaks.begin(), peaks.end());
    for (int f = 0; f < frames; ++f)
    {
        const float scale = 0.9f / (normalisePerFrame ? peaks[(size_t) f] : tablePeak);
        for (int l = 0; l < kLevels; ++l)
        {
            auto* dst = const_cast<float*> (frame (f, l));
            for (int i = 0; i < kTableSize; ++i) dst[i] *= scale;
            dst[kTableSize] = dst[0];
        }
    }
}

const Wavetable& bankTable (int index)
{
    static const std::vector<std::unique_ptr<Wavetable>> bank = buildBank(); // thread-safe one-time build
    return *bank[(size_t) juce::jlimit (0, kBankSize - 1, index)];
}

std::unique_ptr<Wavetable> wavetableFromAudio (const juce::AudioBuffer<float>& audio, int frameSize, const juce::String& name, juce::String& error)
{
    const int n = audio.getNumSamples();
    if (n < 8 || audio.getNumChannels() == 0) { error = "the file is empty"; return {}; }

    // mono, then split into frames (a short file is one cycle)
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int c = 0; c < audio.getNumChannels(); ++c)
        for (int i = 0; i < n; ++i) mono[(size_t) i] += audio.getSample (c, i) / (float) audio.getNumChannels();
    if (n < frameSize) frameSize = n;
    const int available = n / frameSize;
    if (available > 1024) { error = "that's not a wavetable (more than 1024 frames)"; return {}; }
    const int frames = juce::jmin (available, kMaxUserFrames);

    std::vector<float> cycles ((size_t) frames * kTableSize);
    for (int f = 0; f < frames; ++f)
    {
        // long tables are thinned evenly, keeping the first and last frames
        const int src = frames == 1 ? 0 : juce::roundToInt (f * (available - 1) / (double) (frames - 1));
        const float* in = mono.data() + (size_t) src * frameSize;
        for (int i = 0; i < kTableSize; ++i) // resample the cycle to 2048
        {
            const double p = i * (double) frameSize / kTableSize;
            const int i0 = (int) p, i1 = (i0 + 1) % frameSize;
            const float fr = (float) (p - i0);
            cycles[(size_t) f * kTableSize + (size_t) i] = in[i0] + fr * (in[i1] - in[i0]);
        }
    }
    float peak = 0.0f;
    for (float x : cycles) peak = juce::jmax (peak, std::abs (x));
    if (peak < 1.0e-5f) { error = "the file is silent"; return {}; }
    return std::make_unique<Wavetable> (cycles, frames, name, false);
}

std::unique_ptr<Wavetable> loadWavetable (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "can't read " + file.getFileName(); return {}; }
    if (reader->lengthInSamples > (juce::int64) kTableSize * 1024) { error = "that file is too long for a wavetable"; return {}; }

    juce::AudioBuffer<float> audio ((int) juce::jmin (2u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
    return wavetableFromAudio (audio, serumFrameSize (file), file.getFileNameWithoutExtension().toUpperCase(), error);
}
} // namespace kube
