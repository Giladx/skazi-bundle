#include "PumpEngine.h"
#include <cmath>

namespace pump
{
namespace
{
    struct Def { const char* name; double beats; };

    const Def kRates[] = {
        { "4/1", 16.0 }, { "2/1", 8.0 }, { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 },
        { "1/16", 0.25 }, { "1/32", 0.125 },
        { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 },
        { "1/2D", 3.0 }, { "1/4D", 1.5 }, { "1/8D", 0.75 }, { "1/16D", 0.375 },
    };
    constexpr int kNumRates = (int) (sizeof (kRates) / sizeof (kRates[0]));

    const Def kSnaps[] = { { "OFF", 0.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 } };

    constexpr float kAutoTarget = 0.126f; // -18 dBFS RMS
    constexpr float kAutoMax = 15.85f;    // +24 dB

    inline float safeClip (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.95f) return x;
        return std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), x);
    }

    float shapeValue (int shape, float x, const Point* pts, int n)
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        switch (shape)
        {
            case Pump:     return juce::jmin (1.0f, std::pow (x / 0.6f, 1.6f));
            case HardCut:  return x < 0.18f ? 0.0f : juce::jmin (1.0f, (x - 0.18f) / 0.06f);
            case Smooth:   return 0.5f - 0.5f * std::cos (pi * juce::jmin (1.0f, x / 0.85f));
            case Expo:     return 1.0f - std::exp (-x * 6.0f);
            case Log:      return std::log1p (x * 9.0f) / std::log (10.0f);
            case SawDown:  return 1.0f - x;
            case Triangle: return 1.0f - std::abs (2.0f * x - 1.0f);
            case Gate:     return std::fmod (x, 0.5f) < 0.25f ? 0.0f : 1.0f;
            case Bounce:   return juce::jmin (1.0f, std::abs (std::sin (x * pi * 2.5f)) * (0.4f + x));
            case Draw:
                if (n < 2) return 1.0f;
                if (x <= pts[0].x) return pts[0].y;
                for (int i = 1; i < n; ++i)
                    if (x <= pts[i].x)
                    {
                        const float w = pts[i].x - pts[i - 1].x;
                        const float t = w > 1.0e-6f ? (x - pts[i - 1].x) / w : 1.0f;
                        return pts[i - 1].y + (pts[i].y - pts[i - 1].y) * t;
                    }
                return pts[n - 1].y;
            default: return 1.0f;
        }
    }
}

const char* shapeName (int s)
{
    static const char* n[] = { "PUMP", "HARD CUT", "SMOOTH", "EXPO", "LOG", "SAW DOWN", "TRIANGLE", "GATE", "BOUNCE", "DRAW" };
    return n[juce::jlimit (0, NumShapes - 1, s)];
}

juce::StringArray rateNames()
{
    juce::StringArray s;
    for (auto& r : kRates) s.add (r.name);
    return s;
}

double rateBeats (int i) { return kRates[juce::jlimit (0, kNumRates - 1, i)].beats; }

juce::StringArray snapNames()
{
    juce::StringArray s;
    for (auto& r : kSnaps) s.add (r.name);
    return s;
}

double snapBeats (int i) { return kSnaps[juce::jlimit (0, 4, i)].beats; }

std::vector<Point> defaultPoints()
{
    return { { 0.0f, 0.0f }, { 0.08f, 0.15f }, { 0.3f, 0.55f }, { 0.45f, 0.4f }, { 0.7f, 0.95f }, { 1.0f, 1.0f } };
}

float gainAt (const Params& p, float x, float tMs, const Point* pts, int n)
{
    float g = 1.0f;
    const float xs = x / juce::jmax (0.1f, p.release);
    if (xs < 1.0f)
    {
        g = juce::jlimit (0.0f, 1.0f, shapeValue (p.shape, xs, pts, n));
        // CURVE bends the shape: below 50% ducks longer, above 50% recovers sooner
        g = std::pow (g, std::pow (2.0f, (0.5f - p.curve) * 4.0f));
    }
    if (p.attackMs > 0.0f && tMs < p.attackMs)
        g = 1.0f - (tMs / p.attackMs) * (1.0f - g);
    return 1.0f - p.depth * (1.0f - g);
}

//==============================================================================
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    gainSm = 1.0f; ms = 0.0f; autoGain = 1.0f; env = 0.0f; armed = true;
    sinceTrig = 1.0e12;
    peakSlot = -1;
    if (numPoints.load() == 0)
        setPoints (defaultPoints());
    for (auto& pk : uiPeaks) pk.store (0.0f);
}

void Engine::setPoints (const std::vector<Point>& pts)
{
    const int n = juce::jmin (kMaxPoints, (int) pts.size());
    for (int i = 0; i < n; ++i)
    {
        pointData[(size_t) i * 2].store (pts[(size_t) i].x);
        pointData[(size_t) i * 2 + 1].store (pts[(size_t) i].y);
    }
    numPoints.store (n);
}

std::vector<Point> Engine::getPoints() const
{
    std::vector<Point> v;
    const int n = numPoints.load();
    for (int i = 0; i < n; ++i)
        v.push_back ({ pointData[(size_t) i * 2].load(), pointData[(size_t) i * 2 + 1].load() });
    return v;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain,
                      const std::vector<int>& noteOns, const Params& p, juce::AudioPlayHead* playHead)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0) return;

    double bpm = 120.0, hostPpq = 0.0;
    bool playing = false;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { hostPpq = *q; playing = pos->getIsPlaying(); }
        }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    uiBpm.store (bpm);
    if (playing) ppq = hostPpq;

    Point pts[kMaxPoints];
    const int nPts = numPoints.load();
    for (int i = 0; i < nPts; ++i)
        pts[i] = { pointData[(size_t) i * 2].load(), pointData[(size_t) i * 2 + 1].load() };

    const double cycleBeats = rateBeats (p.rate);
    const double beatsPerSample = bpm / (60.0 * sr);
    const double cycleSamples = cycleBeats * 60.0 / bpm * sr;
    const float smoothCoef = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.3, (double) p.smoothMs) * 0.001 * sr)));
    const float msCoef = (float) (1.0 - std::exp (-1.0 / (0.4 * sr)));
    const float autoCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * sr)));
    const float envAtt = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));
    const float envRel = (float) (1.0 - std::exp (-1.0 / (0.05 * sr)));
    const float thresh = juce::Decibels::decibelsToGain (p.threshDb);
    const double minRetrig = 0.03 * sr;
    const float meterDecay = (float) std::exp (-1.0 / (0.15 * sr));

    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };
    const bool scValid = sidechain != nullptr && sidechain->getNumChannels() > 0 && sidechain->getNumSamples() >= numSamples;
    size_t nextNote = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        // where are we in the cycle?
        juce::int64 cycleIdx = 0;
        float x = 1.0f, tMs = 1.0e9f;
        if (p.trigger == Sync)
        {
            const double pos = ppq / cycleBeats + p.phase / 360.0;
            const auto pair = (juce::int64) std::floor (pos * 0.5);
            const double f = pos - 2.0 * (double) pair;
            const double a = 2.0 * p.swing; // first cycle of each pair gets this share of two cycles
            const bool second = f >= a;
            const double len = second ? 2.0 - a : a;
            cycleIdx = pair * 2 + (second ? 1 : 0);
            x = (float) ((second ? f - a : f) / len);
            tMs = (float) (x * len * cycleSamples * 1000.0 / sr);
            ppq += beatsPerSample;
        }
        else
        {
            bool hit = false;
            if (p.trigger == Midi)
            {
                while (nextNote < noteOns.size() && noteOns[nextNote] <= i) { hit = true; ++nextNote; }
            }
            else
            {
                float s = 0.0f;
                if (scValid)
                    for (int c = 0; c < sidechain->getNumChannels(); ++c)
                        s = juce::jmax (s, std::abs (sidechain->getSample (c, i)));
                else
                    s = juce::jmax (std::abs (ch[0][i]), std::abs (ch[1][i]));
                env += (s - env) * (s > env ? envAtt : envRel);
                if (env > thresh && armed && sinceTrig > minRetrig) { hit = true; armed = false; }
                if (env < thresh * 0.5f) armed = true;
            }
            if (hit) { ++trigCycle; sinceTrig = 0.0; }
            cycleIdx = trigCycle;
            x = (float) juce::jmin (1.0, sinceTrig / cycleSamples);
            tMs = (float) (sinceTrig * 1000.0 / sr);
            sinceTrig += 1.0;
            ppq += beatsPerSample;
        }

        const float target = gainAt (p, juce::jmin (x, 0.99999f), tMs, pts, nPts);
        gainSm += (target - gainSm) * smoothCoef;

        // pre-amp, with the optional auto boost toward -18 dBFS RMS
        float in[2] = { ch[0][i] * p.preGain, ch[1][i] * p.preGain };
        const float sq = 0.5f * (in[0] * in[0] + in[1] * in[1]);
        ms += (sq - ms) * msCoef;
        if (p.autoGain)
        {
            if (ms > 1.0e-6f) // below -60 dB: hold, so silence never gets pumped up to full boost
            {
                const float want = juce::jlimit (1.0f, kAutoMax, kAutoTarget / std::sqrt (ms));
                autoGain += (want - autoGain) * autoCoef;
            }
        }
        else
        {
            autoGain += (1.0f - autoGain) * autoCoef;
        }
        in[0] *= autoGain;
        in[1] *= autoGain;

        float out[2];
        for (int c = 0; c < 2; ++c)
            out[c] = safeClip ((in[c] * (1.0f - p.mix) + in[c] * gainSm * p.mix) * p.outGain);

        if (numCh > 1) { ch[0][i] = out[0]; ch[1][i] = out[1]; }
        else           { ch[0][i] = 0.5f * (out[0] + out[1]); }

        inPk = juce::jmax (inPk * meterDecay, std::abs (in[0]), std::abs (in[1]));
        outPk = juce::jmax (outPk * meterDecay, std::abs (out[0]), std::abs (out[1]));

        const auto cyc = (int) (((cycleIdx % kCyclesShown) + kCyclesShown) % kCyclesShown);
        const int slot = juce::jmin (kDisplaySlots - 1, (int) ((cyc + juce::jmin (x, 0.999f)) / kCyclesShown * kDisplaySlots));
        if (slot != peakSlot)
        {
            if (peakSlot >= 0) uiPeaks[(size_t) peakSlot].store (peakAcc);
            peakSlot = slot;
            peakAcc = 0.0f;
        }
        peakAcc = juce::jmax (peakAcc, std::abs (out[0]), std::abs (out[1]));

        if (i == numSamples - 1)
            uiPos.store ((float) ((cyc + juce::jmin (x, 0.999f)) / kCyclesShown));
    }

    uiGain.store (gainSm);
    uiIn.store (inPk);
    uiOut.store (outPk);
    uiAutoDb.store (juce::Decibels::gainToDecibels (autoGain));
}
} // namespace pump
