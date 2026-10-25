#include "SalatEngine.h"
#include <cmath>
#include <complex>

namespace salat
{
namespace
{
    constexpr double kStepBeats[] = { 1.0, 0.5, 0.25, 0.125 };
    constexpr int kRepeats[] = { 2, 3, 4, 6, 8, 12, 16 };
    constexpr double kLfoBeats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    constexpr double kDelayBeats[] = { 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25 };

    template <typename T, size_t N>
    T pickAt (const T (&list)[N], int index) { return list[(size_t) juce::jlimit (0, (int) N - 1, index)]; }

    float approach (float v, float target, float step)
    {
        return v < target ? juce::jmin (target, v + step) : juce::jmax (target, v - step);
    }

    int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }
}

double chopStepBeats (int index) { return pickAt (kStepBeats, index); }
int chopRepeats (int index) { return pickAt (kRepeats, index); }
double lfoBeats (int index) { return pickAt (kLfoBeats, index); }
double delayBeats (int index) { return pickAt (kDelayBeats, index); }
int riseBars (int index) { static const int bars[] = { 2, 4, 8, 16 }; return bars[juce::jlimit (0, 3, index)]; }

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = {
        { "in", "Input", P::Float, -24, 24, 0, "dB", 0.1f },
        { "mix", "Mix", P::Float, 0, 100, 100, "%", 0.1f },
        { "out", "Output", P::Float, -24, 12, 0, "dB", 0.1f },

        { "chopOn", "Chop", P::Bool, 0, 1, 1 },
        { "chopPos", "Chop Position", P::Choice, 0, 2, Middle, "", 1, 1, { "FIRST", "MIDDLE", "LAST" } },
        { "chopStep", "Chop Step", P::Choice, 0, 3, 2, "", 1, 1, { "1/4", "1/8", "1/16", "1/32" } },
        { "chopRep", "Chop Repeats", P::Choice, 0, 6, 2, "", 1, 1, { "2", "3", "4", "6", "8", "12", "16" } },
        { "chance", "Chop Chance", P::Float, 0, 100, 35, "%", 0.1f },
        { "wStut", "Stutter Amount", P::Float, 0, 100, 100, "%", 0.1f },
        { "wRev", "Reverse Amount", P::Float, 0, 100, 50, "%", 0.1f },
        { "wGate", "Gate Amount", P::Float, 0, 100, 40, "%", 0.1f },
        { "wTape", "Tape Stop Amount", P::Float, 0, 100, 25, "%", 0.1f },
        { "hold", "Hold Stutter", P::Bool, 0, 1, 0 },

        { "drvOn", "Drive On", P::Bool, 0, 1, 1 },
        { "drvMode", "Drive Type", P::Choice, 0, 3, Tube, "", 1, 1, { "TUBE", "FUZZ", "FOLD", "RECTIFY" } },
        { "drive", "Drive", P::Float, 0, 36, 9, "dB", 0.1f },

        { "crOn", "Crush On", P::Bool, 0, 1, 0 },
        { "bits", "Bits", P::Float, 2, 16, 8, "bit", 0.1f },
        { "down", "Downsample", P::Float, 1, 50, 4, "x", 1, 0.5f },

        { "fltOn", "Filter On", P::Bool, 0, 1, 1 },
        { "fltType", "Filter Type", P::Choice, 0, 3, LowPass, "", 1, 1, { "LP", "BP", "HP", "NOTCH" } },
        { "cutoff", "Cutoff", P::Float, 20, 20000, 8000, "Hz", 1, 0.25f },
        { "reso", "Resonance", P::Float, 0, 100, 30, "%", 0.1f },
        { "lfoRate", "Filter LFO Rate", P::Choice, 0, 7, 2, "", 1, 1, { "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32" } },
        { "lfoShape", "Filter LFO Shape", P::Choice, 0, 4, Sine, "", 1, 1, { "SINE", "TRI", "SAW", "SQUARE", "S&H" } },
        { "lfoDepth", "Filter LFO Depth", P::Float, 0, 4, 0, "oct", 0.01f },

        { "ringOn", "Ring Mod On", P::Bool, 0, 1, 0 },
        { "ringFreq", "Ring Frequency", P::Float, 20, 5000, 440, "Hz", 1, 0.3f },
        { "ringMix", "Ring Mix", P::Float, 0, 100, 50, "%", 0.1f },

        { "modOn", "Mod On", P::Bool, 0, 1, 0 },
        { "modType", "Mod Type", P::Choice, 0, 1, Flanger, "", 1, 1, { "FLANGER", "CHORUS" } },
        { "modRate", "Mod Rate", P::Float, 0.05f, 5, 0.3f, "Hz", 0.01f, 0.5f },
        { "modDepth", "Mod Depth", P::Float, 0, 100, 60, "%", 0.1f },
        { "modFb", "Mod Feedback", P::Float, 0, 95, 50, "%", 0.1f },
        { "modMix", "Mod Mix", P::Float, 0, 100, 50, "%", 0.1f },

        { "dlyOn", "Delay On", P::Bool, 0, 1, 0 },
        { "dlyTime", "Delay Time", P::Choice, 0, 7, 5, "", 1, 1, { "1/2", "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T", "1/16" } },
        { "dlyFb", "Delay Feedback", P::Float, 0, 95, 40, "%", 0.1f },
        { "dlyTone", "Delay Tone", P::Float, 0, 100, 50, "%", 0.1f },
        { "dlyMix", "Delay Mix", P::Float, 0, 100, 30, "%", 0.1f },

        { "revOn", "Reverb On", P::Bool, 0, 1, 0 },
        { "revSize", "Reverb Size", P::Float, 0, 100, 60, "%", 0.1f },
        { "revDamp", "Reverb Damping", P::Float, 0, 100, 50, "%", 0.1f },
        { "revMix", "Reverb Mix", P::Float, 0, 100, 25, "%", 0.1f },

        // Arrangement tool, kept out of presets / RANDOM / MAKE A SALAT (see keepOnPresetChange)
        { "riseOn", "Rise", P::Bool, 0, 1, 0 },
        { "riseLen", "Rise Length", P::Choice, 0, 3, 2, "", 1, 1, { "2 BARS", "4 BARS", "8 BARS", "16 BARS" } },
        { "riseAmt", "Rise Amount", P::Float, -48, 48, 12, "st", 0.01f }, // free: any amount, glides smoothly; 12/24/48 buttons for exact octaves
    };
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.inGain = juce::Decibels::decibelsToGain (v ("in")); s.mix = v ("mix") * 0.01f; s.outGain = juce::Decibels::decibelsToGain (v ("out"));
    s.chopOn = v ("chopOn") > 0.5f; s.hold = v ("hold") > 0.5f;
    s.chopPos = (int) v ("chopPos"); s.chopStep = (int) v ("chopStep"); s.repeats = chopRepeats ((int) v ("chopRep"));
    s.chance = v ("chance") * 0.01f;
    s.weights = { v ("wStut") * 0.01f, v ("wRev") * 0.01f, v ("wGate") * 0.01f, v ("wTape") * 0.01f };
    s.driveOn = v ("drvOn") > 0.5f; s.driveMode = (int) v ("drvMode"); s.driveDb = v ("drive");
    s.crushOn = v ("crOn") > 0.5f; s.bits = v ("bits"); s.downsample = v ("down");
    s.filterOn = v ("fltOn") > 0.5f; s.filterType = (int) v ("fltType"); s.cutoff = v ("cutoff"); s.reso = v ("reso") * 0.01f;
    s.lfoRate = (int) v ("lfoRate"); s.lfoShape = (int) v ("lfoShape"); s.lfoDepth = v ("lfoDepth");
    s.ringOn = v ("ringOn") > 0.5f; s.ringFreq = v ("ringFreq"); s.ringMix = v ("ringMix") * 0.01f;
    s.modOn = v ("modOn") > 0.5f; s.modType = (int) v ("modType"); s.modRate = v ("modRate");
    s.modDepth = v ("modDepth") * 0.01f; s.modFb = v ("modFb") * 0.01f; s.modMix = v ("modMix") * 0.01f;
    s.delayOn = v ("dlyOn") > 0.5f; s.delayTime = (int) v ("dlyTime");
    s.delayFb = v ("dlyFb") * 0.01f; s.delayTone = v ("dlyTone") * 0.01f; s.delayMix = v ("dlyMix") * 0.01f;
    s.reverbOn = v ("revOn") > 0.5f; s.reverbSize = v ("revSize") * 0.01f; s.reverbDamp = v ("revDamp") * 0.01f; s.reverbMix = v ("revMix") * 0.01f;
    s.riseOn = v ("riseOn") > 0.5f; s.riseLen = (int) v ("riseLen"); s.riseSemis = v ("riseAmt");
    return s;
}

float shape (int mode, float x)
{
    switch (mode)
    {
        case Fuzz:    return std::copysign (1.0f - std::exp (-std::abs (x) * 2.0f), x + 0.1f) - (1.0f - std::exp (-0.2f));
        case Fold:    return std::sin (x);
        case Rectify: return std::tanh (std::abs (x) * 1.5f - 0.5f) * 0.9f + x * 0.1f;
        case Tube:
        default:      return std::tanh (x + 0.15f) - std::tanh (0.15f);
    }
}

// k runs from 2 (no resonance) to 0.05 (Q 20); the same mapping as the engine
static float resoToK (float reso) { return 2.0f - 1.95f * std::pow (juce::jlimit (0.0f, 1.0f, reso), 0.7f); }

float filterMagnitude (int type, float cutoff, float reso, float freq)
{
    const std::complex<float> s (0.0f, freq / cutoff);
    const float k = resoToK (reso);
    const auto d = s * s + k * s + 1.0f;
    switch (type)
    {
        case BandPass: return std::abs (k * s / d);
        case HighPass: return std::abs (s * s / d);
        case Notch:    return std::abs ((s * s + 1.0f) / d);
        case LowPass:
        default:       return std::abs (1.0f / d);
    }
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlock);
    latencySamples = (int) std::round (os->getLatencyInSamples());

    work.setSize (2, maxBlock);
    dry.setSize (2, maxBlock);
    scratch.setSize (3, maxBlock);
    for (auto& d : dryDelay) d.assign ((size_t) juce::jmax (1, latencySamples), 0.0f);
    dryPos = 0;

    chopSize = (int) (sr * 4.0);
    for (auto& b : chopBuf) b.assign ((size_t) chopSize, 0.0f);
    chopWrite = 0;
    voices = {};
    lastStep = std::numeric_limits<juce::int64>::min();
    currentEvent = None;
    for (auto& h : history) h.store (None);

    modSize = nextPow2 ((int) (sr * 0.06));
    for (auto& b : modBuf) b.assign ((size_t) modSize, 0.0f);
    delaySize = nextPow2 ((int) (sr * 6.5));
    for (auto& b : delayBuf) b.assign ((size_t) delaySize, 0.0f);
    modWrite = delayWrite = 0;

    reverb.setSampleRate (sr);
    reverb.reset();

    riseFadeLen = juce::jmax (64, (int) (0.006 * sr));      // 6 ms crossfade at each jump
    riseSize = nextPow2 ((int) (0.7 * sr));                  // room for +48 st (reading 16x faster)
    for (auto& b : riseBuf) b.assign ((size_t) riseSize, 0.0f);
    riseWrite = 0;
    riseDelay = 0.04 * sr;
    riseFading = false;
    riseWasOn = false;
    riseSemisSm = 0.0f;

    for (int c = 0; c < 2; ++c) { crushHold[c] = ic1[c] = ic2[c] = modFbState[c] = delayLp[c] = 0.0f; }
    primed = false;
    limiterGain = 1.0f;
}

float Engine::readChop (int c, double idx) const
{
    const double fl = std::floor (idx);
    const float frac = (float) (idx - fl);
    int i0 = (int) ((juce::int64) fl % chopSize);
    if (i0 < 0) i0 += chopSize;
    const int i1 = (i0 + 1) % chopSize;
    const auto& b = chopBuf[c];
    return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * frac;
}

void Engine::startEvent (int type, const Settings& s, bool hold)
{
    for (auto& v : voices)
        if (v.active && ! v.hold && v.t >= v.len) v.active = false;

    Voice* v = nullptr;
    for (auto& cand : voices)
        if (! cand.active && (v == nullptr || cand.blend < v->blend)) v = &cand;
    if (v == nullptr) v = voices[0].blend < voices[1].blend ? &voices[0] : &voices[1];

    v->type = type;
    v->hold = hold;
    v->active = true;
    v->start = chopWrite;
    v->t = 0;
    v->len = juce::jlimit (64, chopSize / 2, (int) stepSamples);
    v->slice = juce::jmax (32, v->len / juce::jmax (1, s.repeats));
    v->fade = juce::jlimit (8, juce::jmax (8, v->slice / 4), (int) (0.004 * sr));
    v->tapePos = 0.0;
    v->gate = 1.0f;
}

void Engine::processChop (int i, float& l, float& r, const Settings& s)
{
    chopBuf[0][(size_t) chopWrite] = l;
    chopBuf[1][(size_t) chopWrite] = r;

    const double beat = blockBeat + i * beatsPerSample;
    const auto step = (juce::int64) std::floor (beat / chopStepBeats (s.chopStep) + 1.0e-9);
    if (step != lastStep)
    {
        const bool first = lastStep == std::numeric_limits<juce::int64>::min();
        lastStep = step;
        if (! first)
        {
            int ev = None;
            if (s.chopOn && s.hold)
            {
                bool holding = false;
                for (auto& v : voices) holding = holding || (v.active && v.hold);
                if (! holding) startEvent (Stutter, s, true);
                ev = Hold;
            }
            else
            {
                for (auto& v : voices) if (v.hold) v.active = false;
                if (s.chopOn)
                {
                    const int forced = burstSteps.load();
                    if (forced > 0) burstSteps.store (forced - 1);
                    if (forced > 0 || rng.nextFloat() < s.chance)
                    {
                        float total = 0.0f;
                        for (float w : s.weights) total += w;
                        ev = Stutter;
                        if (total > 0.0f)
                        {
                            float pick = rng.nextFloat() * total;
                            for (int k = 0; k < 4; ++k)
                            {
                                pick -= s.weights[(size_t) k];
                                if (pick < 0.0f) { ev = Stutter + k; break; }
                            }
                        }
                        startEvent (ev, s, false);
                    }
                }
            }
            const int count = historyCount.load();
            history[(size_t) (count % kHistory)].store (ev);
            historyCount.store (count + 1);
            currentEvent = ev;
        }
    }

    float evL = 0.0f, evR = 0.0f, sum = 0.0f;
    for (auto& v : voices)
    {
        if (v.type == None) continue;
        if (v.active && ! v.hold && v.t >= v.len) v.active = false;
        v.blend = approach (v.blend, v.active ? 1.0f : 0.0f, 1.0f / (float) v.fade);

        float a = 0.0f, b = 0.0f;
        switch (v.type)
        {
            case Stutter:
            {
                // loop [start, start + slice), crossfading each wrap into the audio just before start
                const int k = v.t % v.slice;
                const double idx = (double) v.start + k;
                a = readChop (0, idx); b = readChop (1, idx);
                const int xf = v.slice - v.fade;
                if (k >= xf)
                {
                    const float g = (float) (k - xf + 1) / (float) v.fade;
                    const double from = (double) v.start + k - v.slice;
                    a += (readChop (0, from) - a) * g;
                    b += (readChop (1, from) - b) * g;
                }
                break;
            }
            case Reverse:
            {
                const double idx = (double) v.start - 1 - v.t;
                a = readChop (0, idx); b = readChop (1, idx);
                break;
            }
            case Gate:
            {
                const bool open = (v.t % v.slice) < v.slice / 2;
                v.gate = approach (v.gate, open ? 1.0f : 0.0f, 1.0f / (float) v.fade);
                const double idx = (double) v.start + v.t;
                a = readChop (0, idx) * v.gate; b = readChop (1, idx) * v.gate;
                break;
            }
            case Tape:
            {
                const float speed = juce::jmax (0.0f, 1.0f - (float) v.t / (float) v.len);
                const float amp = juce::jmin (1.0f, speed * 4.0f);
                const double idx = (double) v.start + v.tapePos;
                a = readChop (0, idx) * amp; b = readChop (1, idx) * amp;
                v.tapePos += speed;
                break;
            }
            default: break;
        }
        evL += a * v.blend; evR += b * v.blend; sum += v.blend;
        ++v.t;
        if (! v.active && v.blend <= 0.0f) v.type = None;
    }

    const float live = juce::jmax (0.0f, 1.0f - sum);
    l = l * live + evL;
    r = r * live + evR;
    chopWrite = (chopWrite + 1) % chopSize;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s, const skazi::HostTime& host)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0 || n == 0 || os == nullptr) return;

    // where each sample sits on the beat grid: the host's when playing, our own clock when stopped
    beatsPerSample = host.bpm / 60.0 / sr;
    beatsPerBar = host.beatsPerBar;
    blockBeat = host.playing ? host.ppq : freeBeat;
    freeBeat = blockBeat + n * beatsPerSample;
    stepSamples = chopStepBeats (s.chopStep) / beatsPerSample;

    work.setSize (2, n, false, false, true);
    dry.setSize (2, n, false, false, true);
    scratch.setSize (3, n, false, false, true);
    for (int c = 0; c < 2; ++c)
    {
        work.copyFrom (c, 0, buffer, juce::jmin (c, numCh - 1), 0, n);
        dry.copyFrom (c, 0, buffer, juce::jmin (c, numCh - 1), 0, n);
    }

    // keep the dry signal in time with the oversampled wet path
    if (latencySamples > 0)
    {
        const int L = (int) dryDelay[0].size();
        int p = dryPos;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float x = dry.getSample (c, i);
                dry.setSample (c, i, dryDelay[c][(size_t) p]);
                dryDelay[c][(size_t) p] = x;
            }
            p = (p + 1) % L;
        }
        dryPos = p;
    }

    const float sm = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));      // 20 ms control smoothing
    const float smSlow = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));  // delay time glide
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);

    const float nyq = (float) juce::jmin (20000.0, sr * 0.45);
    const float cutTarget = std::log2 (juce::jlimit (20.0f, nyq, s.cutoff));
    const float delayTarget = juce::jlimit (16.0f, (float) delaySize - 4.0f, (float) (delayBeats (s.delayTime) / beatsPerSample));
    if (! primed)
    {
        primed = true;
        auto on = [] (bool b) { return b ? 1.0f : 0.0f; };
        inSm = s.inGain; mixSm = s.mix; outSm = s.outGain;
        driveSm = juce::Decibels::decibelsToGain (s.driveDb); driveOnSm = on (s.driveOn);
        crushOnSm = on (s.crushOn); filterOnSm = on (s.filterOn); cutLogSm = cutTarget; resoSm = s.reso;
        ringOnSm = on (s.ringOn); ringMixSm = s.ringMix; ringFreqSm = s.ringFreq;
        modOnSm = on (s.modOn); modMixSm = s.modMix;
        delayOnSm = on (s.delayOn); delayMixSm = s.delayMix; delaySamplesSm = delayTarget;
        reverbOnSm = on (s.reverbOn); reverbMixSm = s.reverbMix;
    }

    // --- input, chop FIRST ---
    for (int i = 0; i < n; ++i)
    {
        inSm += (s.inGain - inSm) * sm;
        L[i] *= inSm; R[i] *= inSm;
        inPk = juce::jmax (inPk, std::abs (L[i]), std::abs (R[i]));
        if (s.chopPos == First) processChop (i, L[i], R[i], s);
    }

    // --- drive, 2x oversampled ---
    {
        juce::dsp::AudioBlock<float> block (work);
        auto up = os->processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        const float driveTarget = juce::Decibels::decibelsToGain (s.driveDb);
        const float smUp = sm * 0.5f;
        float* ul = up.getChannelPointer (0);
        float* ur = up.getChannelPointer (1);
        for (int i = 0; i < un; ++i)
        {
            driveSm += (driveTarget - driveSm) * smUp;
            driveOnSm += ((s.driveOn ? 1.0f : 0.0f) - driveOnSm) * smUp;
            if (driveOnSm < 1.0e-4f) continue;
            const float comp = 0.5f / std::tanh (0.5f * driveSm);
            for (float* d : { ul + i, ur + i })
            {
                const float x = *d;
                *d = x + (shape (s.driveMode, x * driveSm) * comp - x) * driveOnSm;
            }
        }
        os->processSamplesDown (block);
    }

    // --- crush, filter, ring, chop MIDDLE, flanger / chorus, delay ---
    const float q = std::pow (2.0f, s.bits - 1.0f);
    const int holdN = juce::jmax (1, (int) std::round (s.downsample));
    const double lfoB = lfoBeats (s.lfoRate);
    float lastCut = s.cutoff;

    const float modBase = s.modType == Flanger ? 0.3f : 10.0f, modSpan = s.modType == Flanger ? 4.0f : 10.0f;
    const float modFb = s.modType == Flanger ? s.modFb : s.modFb * 0.3f;
    const float toneA = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (800.0 * std::pow (2.0, s.delayTone * 4.5), sr * 0.45) / sr));
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    for (int i = 0; i < n; ++i)
    {
        float x[2] = { L[i], R[i] };

        // crush: hold every Nth sample, then quantise
        crushOnSm += ((s.crushOn ? 1.0f : 0.0f) - crushOnSm) * sm;
        if (crushOnSm > 1.0e-4f)
        {
            const bool take = (crushCount++ % holdN) == 0;
            for (int c = 0; c < 2; ++c)
            {
                if (take) crushHold[c] = x[c];
                const float y = std::round (crushHold[c] * q) / q;
                x[c] += (y - x[c]) * crushOnSm;
            }
        }

        // filter: state variable, cutoff swept by the synced LFO
        filterOnSm += ((s.filterOn ? 1.0f : 0.0f) - filterOnSm) * sm;
        resoSm += (s.reso - resoSm) * sm;
        cutLogSm += (cutTarget - cutLogSm) * sm;
        if (filterOnSm > 1.0e-4f)
        {
            float lfo = 0.0f;
            if (s.lfoDepth > 0.0f)
            {
                const double pos = (blockBeat + i * beatsPerSample) / lfoB;
                const float ph = (float) (pos - std::floor (pos));
                switch (s.lfoShape)
                {
                    case Triangle: lfo = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                    case Saw:      lfo = 1.0f - 2.0f * ph; break;
                    case Square:   lfo = ph < 0.5f ? 1.0f : -1.0f; break;
                    case SampleHold:
                    {
                        const auto st = (juce::int64) std::floor (pos);
                        if (st != lastLfoStep) { lastLfoStep = st; lfoHeld = rng.nextFloat() * 2.0f - 1.0f; }
                        lfo = lfoHeld;
                        break;
                    }
                    case Sine:
                    default:       lfo = std::sin (ph * twoPi); break;
                }
            }
            const float fc = juce::jlimit (20.0f, nyq, std::exp2 (cutLogSm + s.lfoDepth * lfo));
            lastCut = fc;
            const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
            const float k = resoToK (resoSm);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < 2; ++c)
            {
                const float v3 = x[c] - ic2[c];
                const float v1 = a1 * ic1[c] + a2 * v3;
                const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.0f * v1 - ic1[c];
                ic2[c] = 2.0f * v2 - ic2[c];
                if (! std::isfinite (ic1[c]) || ! std::isfinite (ic2[c])) ic1[c] = ic2[c] = 0.0f;
                float y;
                switch (s.filterType)
                {
                    case BandPass: y = k * v1; break;
                    case HighPass: y = x[c] - k * v1 - v2; break;
                    case Notch:    y = x[c] - k * v1; break;
                    case LowPass:
                    default:       y = v2; break;
                }
                x[c] += (y - x[c]) * filterOnSm;
            }
        }

        // ring modulator
        ringOnSm += ((s.ringOn ? 1.0f : 0.0f) - ringOnSm) * sm;
        ringMixSm += (s.ringMix - ringMixSm) * sm;
        ringFreqSm += (s.ringFreq - ringFreqSm) * sm;
        if (ringOnSm > 1.0e-4f)
        {
            const float carrier = std::sin ((float) ringPhase * twoPi);
            ringPhase += ringFreqSm / sr;
            ringPhase -= std::floor (ringPhase);
            for (auto& xc : x) xc += (xc * carrier - xc) * ringMixSm * ringOnSm;
        }

        if (s.chopPos == Middle) processChop (i, x[0], x[1], s);

        // flanger / chorus: modulated delay, the right side a quarter cycle behind
        modOnSm += ((s.modOn ? 1.0f : 0.0f) - modOnSm) * sm;
        modMixSm += (s.modMix - modMixSm) * sm;
        {
            const float mm = modMixSm * modOnSm;
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = std::sin ((float) (modPhase + c * 0.25) * twoPi);
                const float d = (modBase + modSpan * s.modDepth * (0.5f + 0.5f * lfo)) * 0.001f * (float) sr;
                const float rp = (float) modWrite - d;
                const int i0 = (int) std::floor (rp);
                const float fr = rp - (float) i0;
                const auto& b = modBuf[c];
                const float wet = b[(size_t) (i0 & (modSize - 1))] + (b[(size_t) ((i0 + 1) & (modSize - 1))] - b[(size_t) (i0 & (modSize - 1))]) * fr;
                modFbState[c] = wet;
                modBuf[c][(size_t) modWrite] = x[c] + modFb * wet;
                if (mm > 1.0e-4f) x[c] = x[c] * (1.0f - 0.5f * mm) + wet * 0.5f * mm;
            }
            modWrite = (modWrite + 1) & (modSize - 1);
            modPhase += s.modRate / sr;
            modPhase -= std::floor (modPhase);
        }

        // ping-pong delay: the input goes left, each repeat crosses sides; off stops the input, the tail rings out
        delayOnSm += ((s.delayOn ? 1.0f : 0.0f) - delayOnSm) * sm;
        delayMixSm += (s.delayMix - delayMixSm) * sm;
        delaySamplesSm += (delayTarget - delaySamplesSm) * smSlow;
        {
            const float rp = (float) delayWrite - delaySamplesSm;
            const int i0 = (int) std::floor (rp);
            const float fr = rp - (float) i0;
            float echo[2];
            for (int c = 0; c < 2; ++c)
            {
                const auto& b = delayBuf[c];
                const float a = b[(size_t) (i0 & (delaySize - 1))], bb = b[(size_t) ((i0 + 1) & (delaySize - 1))];
                echo[c] = a + (bb - a) * fr;
                delayLp[c] += (echo[c] - delayLp[c]) * toneA;
            }
            const float in = 0.5f * (x[0] + x[1]) * delayOnSm;
            delayBuf[0][(size_t) delayWrite] = in + s.delayFb * delayLp[1];
            delayBuf[1][(size_t) delayWrite] = s.delayFb * delayLp[0];
            delayWrite = (delayWrite + 1) & (delaySize - 1);
            x[0] += echo[0] * delayMixSm;
            x[1] += echo[1] * delayMixSm;
        }

        // reverb input (sent wet only); the mix is applied after the reverb runs
        reverbOnSm += ((s.reverbOn ? 1.0f : 0.0f) - reverbOnSm) * sm;
        reverbMixSm += (s.reverbMix - reverbMixSm) * sm;
        scratch.setSample (0, i, x[0] * reverbOnSm);
        scratch.setSample (1, i, x[1] * reverbOnSm);
        scratch.setSample (2, i, reverbMixSm);
        L[i] = x[0] * (1.0f - reverbMixSm * reverbOnSm);
        R[i] = x[1] * (1.0f - reverbMixSm * reverbOnSm);
    }
    uiCutoff.store (lastCut);

    // --- reverb ---
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + 0.69f * s.reverbSize;
        p.damping = s.reverbDamp;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);
        reverb.processStereo (scratch.getWritePointer (0), scratch.getWritePointer (1), n);
    }

    // --- chop LAST, dry / wet, output, limiter ---
    const float relA = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
    const int scopeEvery = juce::jmax (1, (int) (2.0 * sr / kScope));
    float minGain = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = scratch.getSample (2, i);
        float l = L[i] + scratch.getSample (0, i) * m;
        float r = R[i] + scratch.getSample (1, i) * m;
        if (s.chopPos == Last) processChop (i, l, r, s);

        mixSm += (s.mix - mixSm) * sm;
        outSm += (s.outGain - outSm) * sm;
        l = (dry.getSample (0, i) * (1.0f - mixSm) + l * mixSm) * outSm;
        r = (dry.getSample (1, i) * (1.0f - mixSm) + r * mixSm) * outSm;
        processRise (blockBeat + i * beatsPerSample, l, r, s);
        if (! std::isfinite (l) || ! std::isfinite (r)) l = r = 0.0f;

        // instant-attack peak limiter at -0.5 dB, then the shared soft clip
        constexpr float ceiling = 0.944f;
        const float pk = juce::jmax (std::abs (l), std::abs (r));
        if (pk * limiterGain > ceiling) limiterGain = ceiling / pk;
        else limiterGain += (1.0f - limiterGain) * relA;
        minGain = juce::jmin (minGain, limiterGain);
        l = skazi::safeClip (l * limiterGain);
        r = skazi::safeClip (r * limiterGain);

        L[i] = l; R[i] = r;
        outPk = juce::jmax (outPk, std::abs (l), std::abs (r));
        scopePk = juce::jmax (scopePk, std::abs (l), std::abs (r));
        if (++scopeCount >= scopeEvery)
        {
            const int w = scopeWrite.load();
            scope[(size_t) w].store (scopePk);
            scopeEvent[(size_t) w].store (currentEvent);
            scopeWrite.store ((w + 1) % kScope);
            scopePk = 0.0f; scopeCount = 0;
        }
    }

    for (int c = 0; c < numCh; ++c)
        buffer.copyFrom (c, 0, work, c, 0, n);

    const double stepPos = freeBeat / chopStepBeats (s.chopStep);
    uiStepPhase.store ((float) (stepPos - std::floor (stepPos)));
    const float decay = (float) std::exp (-(double) n / (0.2 * sr));
    uiIn.store (inPk); uiOut.store (outPk); uiReduction.store (minGain);
    inPk *= decay; outPk *= decay;
    uiRiseSemis.store (riseSemisSm);
}

void Engine::processRise (double beat, float& l, float& r, const Settings& s)
{
    // The rise sits on the DAW's bar grid: it starts on a bar line, climbs linearly in semitones
    // over the chosen number of bars, so it ends exactly on a bar line, then holds at the top.
    // Switched on in the first quarter of a bar (automation a little early or late) it counts from
    // that bar's start; later than that it waits for the next bar line.
    const bool jumpedBack = beat < riseLastBeat - 0.5; // the host looped or the user moved the cursor
    if (s.riseOn && (! riseWasOn || jumpedBack))
    {
        const double barStart = std::floor (beat / beatsPerBar + 1.0e-6) * beatsPerBar;
        riseStartBeat = beat - barStart <= 0.25 * beatsPerBar ? barStart : barStart + beatsPerBar;
    }
    riseWasOn = s.riseOn;
    riseLastBeat = beat;

    const double lengthBeats = riseBars (s.riseLen) * beatsPerBar;
    const bool waiting = s.riseOn && beat < riseStartBeat;
    const float progress = s.riseOn ? (float) juce::jlimit (0.0, 1.0, (beat - riseStartBeat) / lengthBeats) : 0.0f;
    uiRiseProgress.store (waiting ? -1.0f : progress);

    const float target = s.riseOn ? s.riseSemis * progress : 0.0f;
    const float tau = s.riseOn ? 0.05f : 0.03f; // knob and button changes glide; the ramp itself is continuous
    riseSemisSm += (target - riseSemisSm) * (float) (1.0 - std::exp (-1.0 / (tau * sr)));

    // Always keep the delay line fed so switching on never starts from silence
    riseBuf[0][(size_t) riseWrite] = l;
    riseBuf[1][(size_t) riseWrite] = r;
    riseWrite = (riseWrite + 1) & (riseSize - 1);

    // Below a quarter semitone, blend towards the untouched signal (the shifted path runs ~35 ms late)
    const float blend = juce::jlimit (0.0f, 1.0f, std::abs (riseSemisSm) * 4.0f);
    if (blend <= 0.0f && ! riseFading)
        return;

    // Reading faster than real time shortens the delay (pitch up), slower lengthens it (pitch down)
    const double speed = std::exp2 (riseSemisSm / 12.0) - 1.0;
    riseDelay -= speed;
    if (riseFading) riseFadeDelay -= speed;

    // The read point needs room to drift: the faster it reads (up to 16x at +48 st), the wider the
    // window, and the further from the write point it must stay, since the old tap keeps moving
    // through a crossfade.
    const double fade = riseFadeLen;
    const double lo = std::max (speed, 0.0) * fade + fade + 8.0;
    const double window = std::min (std::max (0.05 * sr, std::abs (speed) * fade * 3.0), riseSize - lo - 3.0 * fade);
    const double hi = lo + window;
    if (! riseFading && (riseDelay < lo || riseDelay > hi))
    {
        // jump back across the window to where the waveform lines up best, then crossfade
        const double from = riseDelay;
        riseDelay = speed > 0 ? riseBestJump (from, std::max (lo, from + 0.45 * window), std::min (hi, from + window))
                              : riseBestJump (from, std::max (lo, from - window), std::min (hi, from - 0.45 * window));
        riseFadeDelay = from;
        riseFading = true;
        riseFadePos = 0;
    }

    float out[2];
    for (int c = 0; c < 2; ++c)
    {
        out[c] = riseRead (c, riseDelay);
        if (riseFading)
        {
            const float t = (float) riseFadePos / (float) riseFadeLen;
            const float a = std::cos (t * juce::MathConstants<float>::halfPi), b = std::sin (t * juce::MathConstants<float>::halfPi);
            out[c] = riseRead (c, riseFadeDelay) * a + out[c] * b;
        }
    }
    if (riseFading && ++riseFadePos >= riseFadeLen)
        riseFading = false;

    l += (out[0] - l) * blend;
    r += (out[1] - r) * blend;
}

float Engine::riseRead (int c, double delay) const
{
    // the newest sample sits at riseWrite - 1
    const double pos = riseWrite - 1 - delay;
    const double fl = std::floor (pos);
    const float frac = (float) (pos - fl);
    const int i0 = (int) ((juce::int64) fl & (riseSize - 1)), i1 = (i0 + 1) & (riseSize - 1);
    const auto& b = riseBuf[c];
    return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * frac;
}

double Engine::riseBestJump (double from, double lo, double hi) const
{
    // Compare what the current read point is about to play with each candidate (mono, coarse steps;
    // at most ~160 candidates, so wide windows at high pitch don't cost more)
    const int len = riseFadeLen;
    const double step = std::max (3.0, (hi - lo) / 160.0);
    double best = -1.0e30, bestDelay = 0.5 * (lo + hi);
    for (double cand = lo; cand <= hi; cand += step)
    {
        double corr = 0.0, energy = 1.0e-9;
        for (int k = 0; k < len; k += 2)
        {
            // both read points move forward in time over the fade, i.e. towards smaller delays
            const float a = riseRead (0, from - k) + riseRead (1, from - k);
            const float b = riseRead (0, cand - k) + riseRead (1, cand - k);
            corr += a * b;
            energy += b * b;
        }
        const double score = corr / std::sqrt (energy);
        if (score > best) { best = score; bestDelay = cand; }
    }
    return bestDelay;
}
} // namespace salat
