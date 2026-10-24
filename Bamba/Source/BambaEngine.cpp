#include "BambaEngine.h"
#include <cmath>

namespace bamba
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    inline double frac (double x) { return x - std::floor (x); }
    inline double blep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }
}

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = {
        { "type", "Type", P::Choice, 0, 9, 0, "", 1, 1,
          { "UPLIFTER", "DOWNLIFTER", "NOISE SWEEP", "IMPACT", "SNARE ROLL", "REVERSE CRASH", "LASER", "SIREN", "SUB DROP", "RANDOM FX" } },
        { "trigger", "Trigger", P::Choice, 0, 1, 1, "", 1, 1, { "FULL BARS", "HOLD NOTE" } },
        { "bars", "Length", P::Choice, 0, 4, 3, "", 1, 1, { "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS" } },
        { "curve", "Curve", P::Float, 0, 100, 60, "%", 0.1f },
        { "offset", "Offset (bars)", P::Int, 0, 15, 0 },
        { "noise", "Noise", P::Float, 0, 100, 70, "%", 0.1f },
        { "color", "Noise Color", P::Float, 0, 100, 50, "%", 0.1f },
        { "tone", "Tone", P::Float, 0, 100, 40, "%", 0.1f },
        { "pfrom", "Pitch From", P::Float, -24, 24, -12, "st", 1 },
        { "pto", "Pitch To", P::Float, -24, 48, 24, "st", 1 },
        { "sweep", "Filter Sweep", P::Float, 0, 100, 75, "%", 0.1f },
        { "reso", "Resonance", P::Float, 0, 100, 40, "%", 0.1f },
        { "roll", "Roll", P::Float, 0, 100, 0, "%", 0.1f },
        { "width", "Width", P::Float, 0, 100, 70, "%", 0.1f },
        { "phaser", "Phaser", P::Float, 0, 100, 30, "%", 0.1f },
        { "reverb", "Reverb", P::Float, 0, 100, 50, "%", 0.1f },
        { "delay", "Delay", P::Float, 0, 100, 30, "%", 0.1f },
        { "gain", "Gain", P::Float, -24, 6, 0, "dB", 0.1f },
    };
    return defs;
}

int barsFor (int c) { static const int b[] = { 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 4, c)]; }

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.type = (int) v ("type"); s.trigger = (int) v ("trigger"); s.bars = barsFor ((int) v ("bars"));
    s.curve = v ("curve") * 0.01f; s.offset = (int) v ("offset");
    s.noise = v ("noise") * 0.01f; s.color = v ("color") * 0.01f; s.tone = v ("tone") * 0.01f;
    s.pitchFrom = v ("pfrom"); s.pitchTo = v ("pto");
    s.sweep = v ("sweep") * 0.01f; s.reso = v ("reso") * 0.01f; s.roll = v ("roll") * 0.01f;
    s.width = v ("width") * 0.01f; s.phaser = v ("phaser") * 0.01f;
    s.reverb = v ("reverb") * 0.01f; s.delay = v ("delay") * 0.01f; s.gain = juce::Decibels::decibelsToGain (v ("gain"));
    return s;
}

Shape shapeAt (int type, float p, float curve, float lenSec)
{
    p = juce::jlimit (0.0f, 1.0f, p);
    const float e = std::pow (2.0f, (curve - 0.5f) * 4.0f);
    const float rise = std::pow (p, e), fall = std::pow (1.0f - p, e);
    const float tSec = p * lenSec;
    switch (type)
    {
        case Uplifter:     return { 0.12f + 0.88f * rise, rise, rise };
        case Downlifter:   return { fall, fall, fall };
        case NoiseSweep:   { const float b = std::pow ((float) std::sin (juce::MathConstants<double>::pi * p), e); return { b, 0.5f, b }; }
        case Impact:       return { std::exp (-tSec / (0.25f + 0.9f * curve)), std::exp (-tSec / 0.08f), std::exp (-tSec / 0.6f) };
        case SnareRoll:    return { 0.25f + 0.75f * rise, 0.3f * rise, rise };
        case ReverseCrash: return { std::pow (p, 2.5f * e), 0.5f, 0.75f + 0.25f * p };
        case Laser:        return { 0.5f + 0.5f * rise, 0.0f, 0.6f + 0.4f * rise };
        case Siren:        return { 0.2f + 0.8f * rise, rise, 0.5f + 0.5f * rise };
        case SubDrop:      return { std::exp (-tSec / (0.8f + 2.0f * curve)), std::exp (-tSec / (0.3f + 0.7f * curve)), 1.0f };
        default:           return { rise, rise, rise };
    }
}

//==============================================================================
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& d : dl) d.assign ((size_t) (sr * 4.0) + 1, 0.0f);
    dlPos = 0;
    reverb.setSampleRate (sr);
    reverb.reset();
    active = releasing = false;
    heldNote = -1;
}

void Engine::start (int note, const Settings& s, juce::int64 seed)
{
    active = true;
    t = 0;
    type = s.type;
    noteSemis = (float) (note - 60);
    ph[0] = ph[1] = zapPh = rollPh = sirenPh = subPh = 0;
    if (type == RandomFx)
    {
        juce::Random r (seed);
        type = r.nextInt (RandomFx);
        rPitchFrom = (float) -r.nextInt (25);
        rPitchTo = (float) r.nextInt (37);
        rSweep = 0.4f + 0.6f * r.nextFloat();
        rRoll = r.nextFloat() * 0.7f;
    }
    uiType.store (type);
}

void Engine::sample (const Settings& s, float& l, float& r)
{
    const bool rnd = s.type == RandomFx;
    const float from = rnd ? rPitchFrom : s.pitchFrom, to = rnd ? rPitchTo : s.pitchTo;
    const float sweep = rnd ? rSweep : s.sweep, rollAmt = rnd ? rRoll : s.roll;
    const float p = (float) juce::jmin (0.99999, t / len);
    const float lenSec = (float) (len / sr), tSec = (float) (t / sr);
    const auto sh = shapeAt (type, p, s.curve, lenSec);
    const float e = std::pow (2.0f, (s.curve - 0.5f) * 4.0f);
    const float rise = std::pow (p, e);
    const double noteMul = std::exp2 (noteSemis / 12.0);

    // tone frequency per type
    double f = 110.0 * std::exp2 ((noteSemis + from + (to - from) * sh.pitch) / 12.0);
    bool sine = false;
    switch (type)
    {
        case Laser:
            zapPh += bpm / 60.0 * 4.0 / sr; // one zap per 1/16
            f = 2000.0 * noteMul * std::exp (-frac (zapPh) * 6.0) + 80.0;
            break;
        case Siren:
            sirenPh += (2.0 + 10.0 * sh.pitch) / sr;
            f *= std::exp2 (std::sin (twoPi * sirenPh) * 7.0 / 12.0);
            break;
        case SubDrop:
            f = 45.0 * std::exp2 ((noteSemis + juce::jmax (0.0f, to) * sh.pitch) / 12.0);
            sine = true;
            break;
        case Impact:
            f = (45.0 + 110.0 * sh.pitch) * noteMul;
            sine = true;
            break;
        case SnareRoll:
            f = 180.0 * noteMul;
            break;
        default: break;
    }
    f = juce::jlimit (10.0, 0.45 * sr, f);

    // tone: band-limited saws detuned across L/R by WIDTH, or a sine for the booms
    float tone[2];
    const double det = s.width * 15.0;
    for (int c = 0; c < 2; ++c)
    {
        const double fc = sine ? f : f * std::exp2 ((c == 0 ? det : -det) / 1200.0);
        const double dt = fc / sr;
        tone[c] = sine ? (float) std::sin (twoPi * ph[c]) : (float) (2.0 * ph[c] - 1.0 - blep (ph[c], dt));
        ph[c] = frac (ph[c] + dt);
    }

    // noise: L and R decorrelated by WIDTH, colored white -> pink -> brown
    const float wL = rng.nextFloat() * 2.0f - 1.0f;
    const float wR = wL * (1.0f - s.width) + (rng.nextFloat() * 2.0f - 1.0f) * s.width;
    const float k = 1.0f - s.color * 0.97f;
    float nz[2];
    for (int c = 0; c < 2; ++c)
    {
        noiseLp[c] += ((c == 0 ? wL : wR) - noiseLp[c]) * k;
        nz[c] = noiseLp[c] * juce::jmin (4.0f, 0.8f / std::sqrt (k));
    }

    float toneAmt = s.tone, noiseAmt = s.noise;
    if (type == NoiseSweep)   toneAmt *= 0.15f;
    if (type == ReverseCrash) toneAmt *= 0.2f;
    if (type == SubDrop || type == Laser || type == Siren) noiseAmt *= 0.15f;
    if (type == Impact) noiseAmt *= std::exp (-tSec / 0.04f) * 0.9f;
    if (type == SubDrop || type == Impact) toneAmt = juce::jmax (toneAmt, 0.8f) * 0.55f;

    // roll: a gate whose hits speed up from 1/8 to 1/32 as the effect builds
    float gate = 1.0f;
    const float rollUse = type == SnareRoll ? 1.0f : rollAmt;
    if (rollUse > 0.0f)
    {
        rollPh += 2.0 * std::pow (4.0, (double) rise) * bpm / 60.0 / sr;
        gate = 1.0f - rollUse * (1.0f - (float) std::exp (-frac (rollPh) * 6.0));
    }

    // filter sweep (high-pass for the crash), phaser, amp
    const float cut = juce::jlimit (30.0f, (float) (0.45 * sr), 80.0f * std::exp2 ((sh.filter * sweep + (1.0f - sweep)) * 7.8f));
    const float g = (float) std::tan (juce::MathConstants<double>::pi * (type == ReverseCrash ? 1200.0f : cut) / sr);
    const float kk = 1.0f / (0.6f + s.reso * s.reso * 8.0f);
    phaserPh = frac (phaserPh + 0.25 / sr);
    const float apFc = 300.0f + 2700.0f * (0.5f + 0.5f * (float) std::sin (twoPi * phaserPh));
    const float tn = (float) std::tan (juce::MathConstants<double>::pi * apFc / sr);
    const float apA = (tn - 1.0f) / (tn + 1.0f);

    float out[2];
    for (int c = 0; c < 2; ++c)
    {
        const float x = nz[c] * noiseAmt + tone[c] * toneAmt;
        const float a1 = 1.0f / (1.0f + g * (g + kk)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2[c];
        const float v1 = a1 * ic1[c] + a2 * v3;
        const float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
        ic1[c] = 2.0f * v1 - ic1[c];
        ic2[c] = 2.0f * v2 - ic2[c];
        float y = type == ReverseCrash ? x - kk * v1 - v2 : v2;

        if (s.phaser > 0.0f)
        {
            float z = y;
            for (auto& st : ap[c])
            {
                const float o = apA * z + st;
                st = z - apA * o;
                z = o;
            }
            y = (y + z * s.phaser) / (1.0f + s.phaser * 0.5f);
        }
        out[c] = y * sh.amp * gate * 0.5f;
    }
    l = out[0];
    r = out[1];
}

void Engine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead)
{
    const int n = out.getNumSamples();
    const auto ht = skazi::hostTime (playHead);
    bpm = ht.bpm;
    if (ht.playing) ppq = ht.ppq;
    const double barSamples = ht.beatsPerBar * 60.0 / bpm * sr;
    const float fadeStep = (float) (1.0 / (0.04 * sr)); // 40 ms release when the note ends

    out.clear();
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);
    auto it = midi.cbegin();

    // Only notes play BAMBA (piano roll, keyboard). MIDI: lasts while the note is held.
    // SYNC: each note plays one full pass of the BARS length.
    for (int i = 0; i < n; ++i)
    {
        for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                start (m.getNoteNumber(), s, (juce::int64) rng.nextInt());
                len = s.bars * barSamples;
                heldNote = m.getNoteNumber();
                releasing = false;
                fade = 1.0f;
            }
            else if ((m.isNoteOff() && m.getNoteNumber() == heldNote) || m.isAllNotesOff() || m.isAllSoundOff())
            {
                heldNote = -1;
                if (active && s.trigger == Midi) releasing = true;
            }
        }

        float l = 0.0f, r = 0.0f;
        if (active)
        {
            sample (s, l, r);
            if (releasing)
            {
                fade -= fadeStep;
                if (fade <= 0.0f) { fade = 0.0f; active = releasing = false; }
            }
            l *= fade;
            r *= fade;
            t += 1.0;
            if (t >= len) { active = releasing = false; }
        }
        L[i] = l;
        if (R != L) R[i] = r;
    }

    // 3/16 ping-pong delay (always running so its echoes ring out after the effect ends)
    {
        const int size = (int) dl[0].size();
        const int d = juce::jlimit (1, size - 1, (int) (0.75 * 60.0 / bpm * sr));
        for (int i = 0; i < n; ++i)
        {
            const int rd = (dlPos - d + size) % size;
            const float yl = dl[0][(size_t) rd], yr = dl[1][(size_t) rd];
            const float in = 0.5f * (L[i] + (R != L ? R[i] : L[i]));
            dl[0][(size_t) dlPos] = in + yr * 0.45f;
            dl[1][(size_t) dlPos] = yl * 0.45f;
            dlPos = (dlPos + 1) % size;
            L[i] += yl * s.delay;
            if (R != L) R[i] += yr * s.delay;
        }
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.88f; rp.damping = 0.35f; rp.width = 1.0f;
    rp.wetLevel = s.reverb * 0.7f; rp.dryLevel = 1.0f - s.reverb * 0.3f;
    reverb.setParameters (rp);
    if (R != L) reverb.processStereo (L, R, n);
    else        reverb.processMono (L, n);

    const float decay = (float) std::exp (-(double) n / (0.2 * sr));
    for (int i = 0; i < n; ++i)
    {
        L[i] = skazi::safeClip (L[i] * s.gain);
        if (R != L) R[i] = skazi::safeClip (R[i] * s.gain);
        peak = juce::jmax (peak, std::abs (L[i]));
    }
    uiOut.store (peak);
    peak *= decay;
    uiProgress.store (active ? (float) (t / len) : -1.0f);
}
} // namespace bamba
