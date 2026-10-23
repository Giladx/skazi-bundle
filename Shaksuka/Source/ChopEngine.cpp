#include "ChopEngine.h"
#include <cmath>
#include <cstring>

namespace chop
{
namespace
{
    struct GridDef { const char* name; double beats; };

    const GridDef kGrids[] = {
        { "1/1", 4.0 },  { "1/2", 2.0 },  { "1/4", 1.0 },  { "1/8", 0.5 },
        { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/64", 0.0625 },
        { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 },
        { "1/16T", 1.0 / 6.0 }, { "1/32T", 1.0 / 12.0 },
        { "1/2D", 3.0 }, { "1/4D", 1.5 }, { "1/8D", 0.75 }, { "1/16D", 0.375 },
    };
    constexpr int kNumGrids = (int) (sizeof (kGrids) / sizeof (kGrids[0]));

    constexpr float kNoSourceWeight = 0.35f;

    inline float softClip (float x)
    {
        if (x > 1.5f)  return 1.0f;
        if (x < -1.5f) return -1.0f;
        return x - (4.0f / 27.0f) * x * x * x;
    }

    inline uint32_t hash32 (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }
}

const char* fxName (int fx)
{
    static const char* n[] = { "REPEAT", "STUTTER", "REVERSE", "SCRAMBLE", "TAPE STOP", "PITCH",
                               "CRUSH", "FILTER", "GATE", "RING", "PAN" };
    return n[fx];
}

const char* fxShortName (int fx)
{
    static const char* n[] = { "RPT", "STT", "REV", "SCR", "TAP", "PIT", "CRU", "FLT", "GAT", "RNG", "PAN" };
    return n[fx];
}

juce::StringArray gridNames()
{
    juce::StringArray s;
    for (auto& g : kGrids)
        s.add (g.name);
    return s;
}

double gridBeats (int gridIndex)
{
    return kGrids[juce::jlimit (0, kNumGrids - 1, gridIndex)].beats;
}

uint32_t packStep (const Step& s)
{
    return (s.active ? 1u : 0u)
         | (s.continuation ? 2u : 0u)
         | ((uint32_t) (s.source + 1) << 2)
         | ((s.procMask & 31u) << 6)
         | ((uint32_t) juce::jlimit (1, 31, s.len) << 11);
}

//==============================================================================
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    histSize = (juce::int64) juce::nextPowerOfTwo ((int) (sr * 20.0));
    histMask = histSize - 1;
    for (auto& h : hist)
        h.assign ((size_t) histSize, 0.0f);
    written = histSize; // pretend the buffer starts full of silence so reads never go negative

    fadeLen = juce::jmax (32, (int) (0.004 * sr));
    gateCoef = (float) (1.0 - std::exp (-1.0 / (0.0015 * sr)));
    cur = {};
    old = {};
    fadeRemaining = remaining = 0;
    pendingIdx = -1;
    lastIdx = std::numeric_limits<juce::int64>::min();
    lastHash = 0;
    for (auto& p : uiPeaks)
        p.store (0.0f);
}

uint64_t Engine::hashParams (const Params& p, int n)
{
    uint64_t h = 1469598103934665603ULL;
    auto mixIn = [&h] (const void* data, size_t size)
    {
        auto* b = static_cast<const unsigned char*> (data);
        for (size_t i = 0; i < size; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
    };
    mixIn (&p.seed, sizeof (p.seed));
    mixIn (&p.chaos, sizeof (p.chaos));
    mixIn (&p.stack, sizeof (p.stack));
    mixIn (&p.lengthVar, sizeof (p.lengthVar));
    mixIn (p.weights, sizeof (p.weights));
    mixIn (&n, sizeof (n));
    return h;
}

Step Engine::makeStep (juce::Random& g, const Params& p, int maxLen, int forcedLen) const
{
    Step s;
    if (forcedLen > 0)
    {
        s.len = forcedLen;
    }
    else if (g.nextFloat() < p.lengthVar)
    {
        const float r = g.nextFloat();
        s.len = r < 0.5f ? 2 : (r < 0.85f ? 4 : 8);
    }
    s.len = juce::jlimit (1, juce::jmax (1, maxLen), s.len);

    for (auto& r : s.r)
        r = g.nextFloat();

    if (g.nextFloat() >= p.chaos)
    {
        if (forcedLen <= 0) s.len = 1;
        return s;
    }

    // Pick the source effect (or none, so some steps are processing only)
    float total = kNoSourceWeight;
    for (int i = 0; i < kNumSources; ++i)
        total += p.weights[i];
    const float x = g.nextFloat() * total;
    float acc = kNoSourceWeight;
    if (x >= acc)
        for (int i = 0; i < kNumSources; ++i)
        {
            acc += p.weights[i];
            if (x < acc) { s.source = i; break; }
        }

    // Layer up to `stack` processors on top
    float pw[NumFx - kNumSources];
    for (int i = 0; i < NumFx - kNumSources; ++i)
        pw[i] = p.weights[kNumSources + i];

    int count = g.nextInt (p.stack + 1);
    if (s.source == kNoSource)
        count = juce::jmax (count, 1);

    for (int c = 0; c < count; ++c)
    {
        float t = 0;
        for (float w : pw) t += w;
        if (t <= 0.0f) break;
        float y = g.nextFloat() * t;
        for (int i = 0; i < NumFx - kNumSources; ++i)
        {
            if (pw[i] <= 0.0f) continue;
            y -= pw[i];
            if (y < 0.0f || i == NumFx - kNumSources - 1)
            {
                s.procMask |= 1u << i;
                pw[i] = 0.0f;
                break;
            }
        }
    }

    s.active = s.source != kNoSource || s.procMask != 0;
    if (! s.active && forcedLen <= 0)
        s.len = 1;
    return s;
}

void Engine::generate (const Params& p)
{
    juce::Random g ((juce::int64) p.seed * 7919 + 1234567);
    int i = 0;
    while (i < numSteps)
    {
        Step s = makeStep (g, p, numSteps - i, 0);
        pattern[i] = s;
        for (int k = 1; k < s.len; ++k)
        {
            Step c;
            c.continuation = true;
            pattern[i + k] = c;
        }
        i += s.len;
    }
    for (int k = 0; k < numSteps; ++k)
        publish (k);
    uiNumSteps.store (numSteps);
    remaining = 0;
}

void Engine::publish (int idx)
{
    uiPattern[(size_t) idx].store (packStep (pattern[idx]));
}

juce::int64 Engine::grooveIndex (double pos, double unit, const Params& p) const
{
    if (p.groove == Straight)
        return (juce::int64) std::floor (pos / unit);

    const double pairLen = 2.0 * unit;
    const auto pair = (juce::int64) std::floor (pos / pairLen);
    const double f = pos - (double) pair * pairLen;

    double a = 0.5;
    switch (p.groove)
    {
        case Swing:   a = p.swing; break;
        case Shuffle: a = 2.0 / 3.0; break;
        case Push:    a = 1.0 - p.swing; break;
        case Drunk:
        {
            const auto h = hash32 ((uint32_t) pair * 2654435761u ^ (uint32_t) p.seed);
            const double r = (h & 0xffff) / 65535.0 - 0.5;
            a = juce::jlimit (0.25, 0.8, 0.5 + r * 3.0 * (p.swing - 0.5) + r * 0.1);
            break;
        }
        default: break;
    }
    return pair * 2 + (f >= a * pairLen ? 1 : 0);
}

void Engine::onNewStep (juce::int64 stepIndex, const Params& p)
{
    const int idx = (int) (((stepIndex % numSteps) + numSteps) % numSteps);
    if (pendingIdx >= 0)
    {
        trigger (pendingIdx, p);
        pendingIdx = -1;
    }

    curPatternIdx = idx;
    samplesInStep = 0;
    uiStep.store (idx);

    const double delay = p.humanize > 0.0f ? rng.nextDouble() * p.humanize * 0.25 * stepSamples : 0.0;
    if (delay < 1.0)
        trigger (idx, p);
    else
    {
        pendingIdx = idx;
        pendingCountdown = (int) delay;
    }
}

void Engine::trigger (int idx, const Params& p)
{
    if (remaining > 0)
    {
        --remaining;
        return;
    }

    Step& s = pattern[idx];
    if (! s.continuation && p.evolve > 0.0f && rng.nextFloat() < p.evolve * 0.5f)
    {
        s = makeStep (rng, p, s.len, s.len);
        publish (idx);
    }

    const Step d = s.continuation ? Step {} : s;
    remaining = d.len - 1;

    if (cur.active || d.active)
    {
        old = cur;
        fadeRemaining = fadeLen;
    }
    setupVoice (cur, d);
}

void Engine::setupVoice (Voice& v, const Step& d) const
{
    v = Voice {};
    v.d = d;
    v.active = d.active;
    v.start = (double) (written - 1);
    v.step = stepSamples;
    v.len = stepSamples * d.len;
    const float* r = d.r;

    switch (d.source)
    {
        case Repeat:   v.loopLen = v.step * (r[0] < 0.6f ? 1.0 : 0.5); break;
        case Stutter:
        {
            static const int div[] = { 2, 4, 8, 16 };
            v.loopLen = v.step / div[juce::jmin (3, (int) (r[0] * 4))];
            break;
        }
        case Reverse:  v.loopLen = v.len; break;
        case Scramble: v.back = v.step * (1 + (int) (r[0] * 8)); break;
        case Tape:     v.stopLen = v.len * (0.4 + 0.6 * r[0]); break;
        case Pitch:
        {
            static const double sp[] = { 0.5, 0.75, 1.5, 2.0 };
            v.speed = sp[juce::jmin (3, (int) (r[0] * 4))];
            v.loopLen = v.step;
            break;
        }
        default: break;
    }
    v.loopLen = juce::jmax (32.0, v.loopLen);

    v.levels = std::pow (2.0f, 2.0f + r[1] * 6.0f);
    v.crushFactor = 1 + (int) (r[2] * 15);

    const double nyq = 0.45 * sr;
    v.filterMode = juce::jmin (2, (int) (r[3] * 3));
    v.fa = juce::jmin (nyq, 60.0 * std::pow (300.0, (double) r[4]));
    v.fb = juce::jmin (nyq, 60.0 * std::pow (300.0, (double) r[5]));
    v.k = 1.0f / (0.7f + r[6] * 7.0f);

    static const int gd[] = { 4, 8, 16 };
    v.gateDiv = gd[juce::jmin (2, (int) (r[7] * 3))];
    v.gateBits = ((uint32_t) (r[8] * 65535.0f)) | 1u;

    v.ringInc = juce::MathConstants<double>::twoPi * 40.0 * std::pow (30.0, (double) r[9]) / sr;
    v.ringMix = 0.6f + 0.4f * r[11];
    v.panCycles = 1 + (int) (r[10] * 4);
}

float Engine::read (int ch, double pos) const
{
    const double maxPos = (double) (written - 1);
    const double minPos = (double) (written - histSize + 4);
    pos = juce::jlimit (minPos, maxPos, pos);
    const auto i0 = (juce::int64) std::floor (pos);
    const auto fr = (float) (pos - (double) i0);
    const auto& b = hist[ch];
    const float s0 = b[(size_t) (i0 & histMask)];
    const float s1 = i0 + 1 <= written - 1 ? b[(size_t) ((i0 + 1) & histMask)] : s0;
    return s0 + (s1 - s0) * fr;
}

void Engine::renderVoice (Voice& v, float* out)
{
    const double now = (double) (written - 1);
    const double t = v.t;
    double pos = now;
    float amp = 1.0f;

    auto edge = [this] (double ph, double L)
    {
        const double n = juce::jmin (L * 0.25, 0.002 * sr);
        if (ph < n)     return (float) (ph / n);
        if (ph > L - n) return (float) ((L - ph) / n);
        return 1.0f;
    };

    if (v.active)
    {
        switch (v.d.source)
        {
            case Repeat:
            case Stutter:
            {
                const double L = v.loopLen, ph = std::fmod (t, L);
                pos = v.start + ph;
                // the first pass is the live audio, so only fade its tail
                amp = t < L ? (ph > L - juce::jmin (L * 0.25, 0.002 * sr) ? edge (ph, L) : 1.0f) : edge (ph, L);
                break;
            }
            case Reverse:
            {
                const double L = v.loopLen, ph = std::fmod (t, L);
                pos = v.start - 1.0 - ph;
                amp = edge (ph, L);
                break;
            }
            case Scramble: pos = now - v.back; break;
            case Tape:
                if (t < v.stopLen)
                {
                    pos = v.start + t - t * t / (2.0 * v.stopLen);
                    const double n = 0.01 * sr;
                    amp = t > v.stopLen - n ? (float) ((v.stopLen - t) / n) : 1.0f;
                }
                else
                {
                    amp = 0.0f;
                }
                break;
            case Pitch:
                if (v.speed < 1.0)
                {
                    pos = v.start + v.speed * t;
                }
                else
                {
                    const double L = v.loopLen, ph = std::fmod (v.speed * t, L);
                    pos = v.start - L + ph;
                    amp = edge (ph, L);
                }
                break;
            default: break;
        }
    }

    float x[2] = { read (0, pos) * amp, read (1, pos) * amp };

    if (v.active && v.d.procMask != 0)
    {
        const auto m = v.d.procMask;
        const double prog = juce::jmin (1.0, t / v.len);

        if (m & (1u << (Crush - kNumSources)))
        {
            if (v.crushCount == 0)
                for (int c = 0; c < 2; ++c)
                    v.held[c] = std::round (x[c] * v.levels) / v.levels;
            v.crushCount = (v.crushCount + 1) % v.crushFactor;
            x[0] = v.held[0];
            x[1] = v.held[1];
        }

        if (m & (1u << (Ring - kNumSources)))
        {
            const float s = (float) std::sin (v.ringPhase);
            for (auto& c : x)
                c = c * (1.0f - v.ringMix) + c * s * v.ringMix;
            v.ringPhase += v.ringInc;
            if (v.ringPhase > juce::MathConstants<double>::twoPi)
                v.ringPhase -= juce::MathConstants<double>::twoPi;
        }

        if (m & (1u << (Filter - kNumSources)))
        {
            if (((int) t & 7) == 0)
            {
                const double fc = v.fa * std::pow (v.fb / v.fa, prog);
                const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
                v.a1 = 1.0f / (1.0f + g * (g + v.k));
                v.a2 = g * v.a1;
                v.a3 = g * v.a2;
            }
            for (int c = 0; c < 2; ++c)
            {
                const float v3 = x[c] - v.ic2[c];
                const float v1 = v.a1 * v.ic1[c] + v.a2 * v3;
                const float v2 = v.ic2[c] + v.a2 * v.ic1[c] + v.a3 * v3;
                v.ic1[c] = 2.0f * v1 - v.ic1[c];
                v.ic2[c] = 2.0f * v2 - v.ic2[c];
                x[c] = v.filterMode == 0 ? v2
                     : v.filterMode == 1 ? x[c] - v.k * v1 - v2
                                         : v.k * v1;
            }
        }

        if (m & (1u << (Gate - kNumSources)))
        {
            const int ci = (int) (t / (v.len / v.gateDiv)) % v.gateDiv;
            const float target = (float) ((v.gateBits >> ci) & 1u);
            v.gateEnv += (target - v.gateEnv) * gateCoef;
            x[0] *= v.gateEnv;
            x[1] *= v.gateEnv;
        }

        if (m & (1u << (Pan - kNumSources)))
        {
            const double lfo = std::sin (juce::MathConstants<double>::twoPi * v.panCycles * prog);
            const double pp = (0.5 + 0.45 * lfo) * juce::MathConstants<double>::halfPi;
            x[0] *= (float) (std::cos (pp) * juce::MathConstants<double>::sqrt2);
            x[1] *= (float) (std::sin (pp) * juce::MathConstants<double>::sqrt2);
        }

        if (m & ((1u << (Filter - kNumSources)) | (1u << (Ring - kNumSources))))
        {
            x[0] = softClip (x[0]);
            x[1] = softClip (x[1]);
        }
    }

    out[0] = x[0];
    out[1] = x[1];
    v.t += 1.0;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Params& p, juce::AudioPlayHead* playHead)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || histSize == 0)
        return;

    double bpm = 120.0, beatsPerBar = 4.0, hostPpq = 0.0;
    bool playing = false;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0) beatsPerBar = ts->numerator * 4.0 / ts->denominator;
            if (auto q = pos->getPpqPosition()) { hostPpq = *q; playing = pos->getIsPlaying(); }
        }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    uiBpm.store (bpm);
    if (playing)
        ppq = hostPpq;

    const double stepBeats = gridBeats (p.grid);
    const double samplesPerBeat = sr * 60.0 / bpm;
    const double freeSamples = juce::jmax (16.0, p.freeMs * 0.001 * sr);
    stepSamples = p.freeMode ? freeSamples : stepBeats * samplesPerBeat;

    int n = p.freeMode ? p.bars * 16 : (int) std::lround (p.bars * beatsPerBar / stepBeats);
    n = juce::jlimit (1, kMaxSteps, n);
    if (const auto h = hashParams (p, n); h != lastHash)
    {
        lastHash = h;
        numSteps = n;
        generate (p);
    }

    const double beatsPerSample = 1.0 / samplesPerBeat;
    const double unit = p.freeMode ? freeSamples : stepBeats;
    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

    for (int i = 0; i < numSamples; ++i)
    {
        const double pos = p.freeMode ? freePos : ppq;
        const auto idx = grooveIndex (pos, unit, p);
        if (p.freeMode) freePos += 1.0; else ppq += beatsPerSample;

        const float dry[2] = { ch[0][i], ch[1][i] };
        hist[0][(size_t) (written & histMask)] = dry[0];
        hist[1][(size_t) (written & histMask)] = dry[1];
        ++written;

        if (idx != lastIdx)
        {
            lastIdx = idx;
            onNewStep (idx, p);
        }
        if (pendingIdx >= 0 && --pendingCountdown <= 0)
        {
            trigger (pendingIdx, p);
            pendingIdx = -1;
        }

        float wet[2];
        renderVoice (cur, wet);
        if (fadeRemaining > 0)
        {
            float o[2];
            renderVoice (old, o);
            const float g = 1.0f - (float) fadeRemaining / (float) fadeLen;
            wet[0] = wet[0] * g + o[0] * (1.0f - g);
            wet[1] = wet[1] * g + o[1] * (1.0f - g);
            --fadeRemaining;
        }

        const float l = (dry[0] * (1.0f - p.mix) + wet[0] * p.mix) * p.gain;
        const float r = (dry[1] * (1.0f - p.mix) + wet[1] * p.mix) * p.gain;
        if (numCh > 1) { ch[0][i] = l; ch[1][i] = r; }
        else           { ch[0][i] = 0.5f * (l + r); }

        // waveform display: peak per slot across one pattern loop
        const double frac = juce::jlimit (0.0, 0.999, samplesInStep / stepSamples);
        samplesInStep += 1.0;
        const int slot = (int) ((curPatternIdx + frac) / numSteps * kDisplaySlots);
        if (slot != peakSlot)
        {
            if (peakSlot >= 0)
                uiPeaks[(size_t) peakSlot].store (peakAcc);
            peakSlot = slot;
            peakAcc = 0.0f;
        }
        peakAcc = juce::jmax (peakAcc, std::abs (l), std::abs (r));
    }

    uiPhase.store ((float) ((curPatternIdx + juce::jlimit (0.0, 1.0, samplesInStep / stepSamples)) / numSteps));
    uiSource.store (cur.active ? cur.d.source : kNoSource);
    uiMask.store (cur.active ? cur.d.procMask : 0u);
}
} // namespace chop
