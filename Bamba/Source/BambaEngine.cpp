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
        { "bars", "Length", P::Choice, 0, 5, 3, "", 1, 1, { "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "24 BARS" } }, // 24 added last so saved lengths keep their index
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
        // Splice sample layer (added in 1.1, after the originals so saved projects keep their mapping)
        { "synth", "Synth Level", P::Float, 0, 100, 100, "%", 0.1f },
        { "smix", "Sample Level", P::Float, 0, 100, 80, "%", 0.1f },
        { "sfit", "Sample Fit", P::Choice, 0, 2, 1, "", 1, 1, { "ORIGINAL", "STRETCH", "VARISPEED" } },
        { "salign", "Sample Align", P::Choice, 0, 1, 1, "", 1, 1, { "START", "END ON DROP" } },
        { "stune", "Sample Tune", P::Float, -24, 24, 0, "st", 1 },
        { "sshape", "Sample Shape", P::Float, 0, 100, 30, "%", 0.1f },
        { "srev", "Sample Reverse", P::Bool, 0, 1, 0 },
        // roll quantize and mixer (1.1)
        { "rrate", "Roll Rate", P::Choice, 0, 8, 0, "", 1, 1, { "FREE", "BUILD", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" } },
        { "rdecay", "Roll Decay", P::Float, 0, 100, 50, "%", 0.1f },
        { "synfx", "Synth FX Send", P::Float, 0, 100, 100, "%", 0.1f },
        { "smpfx", "Sample FX Send", P::Float, 0, 100, 100, "%", 0.1f },
    };
    return defs;
}

int barsFor (int c) { static const int b[] = { 1, 2, 4, 8, 16, 24 }; return b[juce::jlimit (0, 5, c)]; }

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
    s.synth = v ("synth") * 0.01f; s.sampleLevel = v ("smix") * 0.01f; s.sampleTune = v ("stune"); s.sampleShape = v ("sshape") * 0.01f;
    s.sampleFit = (int) v ("sfit"); s.sampleAlign = (int) v ("salign"); s.sampleReverse = v ("srev") > 0.5f;
    s.rollRate = (int) v ("rrate"); s.rollDecay = v ("rdecay") * 0.01f; s.synthFx = v ("synfx") * 0.01f; s.sampleFx = v ("smpfx") * 0.01f;
    return s;
}

SamplePtr makeSample (const juce::AudioBuffer<float>& audio, double rate)
{
    const int n = audio.getNumSamples(), chans = audio.getNumChannels();
    if (n <= 0 || chans <= 0 || rate <= 0)
        return nullptr;
    auto d = std::make_shared<SampleData>();
    d->rate = rate;
    d->l.assign (audio.getReadPointer (0), audio.getReadPointer (0) + n);
    d->r.assign (audio.getReadPointer (chans > 1 ? 1 : 0), audio.getReadPointer (chans > 1 ? 1 : 0) + n);

    // Peak-normalise, so SAMPLE LEVEL means the same for a quiet Splice riser and a loud one.
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (d->l[(size_t) i]), std::abs (d->r[(size_t) i]));
    if (peak > 1.0e-5f)
        for (int i = 0; i < n; ++i) { d->l[(size_t) i] /= peak; d->r[(size_t) i] /= peak; }

    d->overview.assign (512, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        auto& o = d->overview[(size_t) ((juce::int64) i * 512 / n)];
        o = juce::jmax (o, std::abs (d->l[(size_t) i]), std::abs (d->r[(size_t) i]));
    }
    return d;
}

double rollHitsPerBeat (int rate, float progress)
{
    if (rate == RollFree) return 0.0;
    if (rate == RollBuild) return progress < 0.25f ? 1.0 : progress < 0.5f ? 2.0 : progress < 0.75f ? 4.0 : 8.0;
    return 0.5 * std::exp2 (juce::jlimit (0, 6, rate - Roll2)); // 1/2 = half a hit per beat ... 1/128 = 32
}

SamplePlacement placeSample (const Settings& s, double sampleSeconds, double lenSeconds)
{
    // STRETCH and VARISPEED fill the effect exactly; ORIGINAL keeps the sample's own speed,
    // either from the note or timed so it ends right on the drop.
    if (s.sampleFit != FitOriginal) return { 0.0, lenSeconds };
    if (s.sampleAlign == AlignStart) return { 0.0, sampleSeconds };
    return { lenSeconds - sampleSeconds, sampleSeconds };
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
    dryL.assign (8192, 0.0f);
    dryR.assign (8192, 0.0f);
    dlPos = 0;
    reverb.setSampleRate (sr);
    reverb.reset();
    active = releasing = sActive = false;
    heldNote = -1;
}

void Engine::start (int note, const Settings& s, juce::int64 seed)
{
    active = true;
    t = 0;
    type = s.type;
    noteSemis = (float) (note - 60);
    ph[0] = ph[1] = zapPh = rollPh = sirenPh = subPh = 0;
    evT = 0;
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

void Engine::startSample (const Settings& s)
{
    voiceSmp = smp;
    sActive = voiceSmp != nullptr && voiceSmp->frames() > 1 && s.sampleLevel > 0.0f;
    if (! sActive) return;
    sT = 0; sOffset = 0; sDelay = 0;
    sFit = s.sampleFit;
    for (auto& gr : grains) gr.age = -1;
    for (int c = 0; c < 2; ++c) sic1[c] = sic2[c] = 0;

    const double frames = voiceSmp->frames();
    const double pitch = std::exp2 ((noteSemis + s.sampleTune) / 12.0);
    if (sFit == FitVarispeed)
    {
        sStep = frames / len; // the whole file across the effect, like a tape sped up or slowed down
        return;
    }
    sStep = voiceSmp->rate / sr * pitch;
    if (sFit == FitStretch)
        return;
    const auto place = placeSample (s, frames / sStep / sr, len / sr);
    sDelay = place.start * sr;
    if (sDelay < 0) { sOffset = -sDelay * sStep; sDelay = 0; } // longer than the effect: start partway so it still ends on the drop
}

void Engine::samplePlay (const Settings& s, float& l, float& r)
{
    const auto& d = *voiceSmp;
    const int frames = d.frames();
    auto read = [&] (double pos, float& a, float& b)
    {
        if (pos < 0 || pos >= frames - 1) { a = b = 0; return; }
        if (s.sampleReverse) pos = frames - 1 - pos;
        const int i = (int) pos;
        const float f = (float) (pos - i);
        const int j = juce::jmin (frames - 1, i + 1);
        a = d.l[(size_t) i] + (d.l[(size_t) j] - d.l[(size_t) i]) * f;
        b = d.r[(size_t) i] + (d.r[(size_t) j] - d.r[(size_t) i]) * f;
    };

    float a = 0, b = 0;
    if (sFit == FitStretch)
    {
        // Overlapping Hann grains that read at the sample's own pitch while their start points
        // walk through the file at the speed that makes it last exactly the effect's length.
        const int grain = juce::jmax (64, (int) (0.09 * sr)), hop = grain / 2;
        if (((juce::int64) sT) % hop == 0)
        {
            auto& gr = grains[0].age < 0 || (grains[1].age >= 0 && grains[0].age >= grains[1].age) ? grains[0] : grains[1];
            gr.pos = sT / len * frames;
            gr.age = 0;
        }
        for (auto& gr : grains)
            if (gr.age >= 0)
            {
                float ga, gb;
                read (gr.pos + gr.age * sStep, ga, gb);
                const float w = 0.5f - 0.5f * (float) std::cos (twoPi * gr.age / grain);
                a += ga * w; b += gb * w;
                if (++gr.age >= grain) gr.age = -1;
            }
        if (sT >= len) sActive = false;
    }
    else if (sFit == FitVarispeed)
    {
        read (sT * sStep, a, b);
        if (sT >= len) sActive = false;
    }
    else if (sT >= sDelay)
    {
        const double pos = sOffset + (sT - sDelay) * sStep;
        read (pos, a, b);
        if (pos >= frames - 1) sActive = false;
    }
    sT += 1.0;

    // SHAPE: how much of BAMBA's own level and filter movement the sample follows
    const float shape = s.sampleShape;
    float amp = 1.0f;
    if (shape > 0.001f)
    {
        const float sweep = s.type == RandomFx ? rSweep : s.sweep;
        const auto sh = shapeAt (type, (float) juce::jmin (1.0, sT / len), s.curve, (float) (len / sr));
        amp = 1.0f - shape + shape * sh.amp;
        const float open = 1.0f - shape + shape * (sh.filter * sweep + (1.0f - sweep));
        const float cut = juce::jlimit (30.0f, (float) (0.45 * sr), 80.0f * std::exp2 (open * 7.9f));
        const float g = (float) std::tan (juce::MathConstants<double>::pi * cut / sr);
        const float k = 1.0f / (0.6f + s.reso * s.reso * 4.0f);
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float* x[2] = { &a, &b };
        for (int c = 0; c < 2; ++c)
        {
            const float v3 = *x[c] - sic2[c];
            const float v1 = a1 * sic1[c] + a2 * v3;
            const float v2 = sic2[c] + a2 * sic1[c] + a3 * v3;
            sic1[c] = 2.0f * v1 - sic1[c];
            sic2[c] = 2.0f * v2 - sic2[c];
            *x[c] = v2;
        }
    }
    const float level = s.sampleLevel * 0.6f * amp;
    l = a * level;
    r = b * level;
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

    juce::ignoreUnused (rollAmt, rise);

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
        out[c] = y * sh.amp * 0.5f;
    }
    l = out[0];
    r = out[1];
}

void Engine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead)
{
    const int n = out.getNumSamples();
    if ((int) dryL.size() < n) { dryL.assign ((size_t) n, 0.0f); dryR.assign ((size_t) n, 0.0f); } // a host block bigger than promised
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
                startSample (s);
                heldNote = m.getNoteNumber();
                releasing = false;
                fade = 1.0f;
            }
            else if ((m.isNoteOff() && m.getNoteNumber() == heldNote) || m.isAllNotesOff() || m.isAllSoundOff())
            {
                heldNote = -1;
                if ((active || sActive) && s.trigger == Midi) releasing = true;
            }
        }

        // MIXER: synth and sample levels, and how much of each goes on to the delay and reverb
        float l = 0.0f, r = 0.0f, dl0 = 0.0f, dr0 = 0.0f;
        if (active || sActive)
        {
            float yl = 0.0f, yr = 0.0f, sl = 0.0f, sr2 = 0.0f;
            if (active)
            {
                sample (s, yl, yr);
                yl *= s.synth;
                yr *= s.synth;
                t += 1.0;
                if (t >= len) active = false;
            }
            if (sActive)
                samplePlay (s, sl, sr2);

            // ROLL: a gate over synth and sample, FREE (speeds up 1/8 -> 1/32) or quantized to the beat
            const float rollUse = type == SnareRoll ? 1.0f : (s.type == RandomFx ? rRoll : s.roll);
            float gate = 1.0f;
            if (rollUse > 0.0f)
            {
                const float p = (float) juce::jmin (1.0, evT / len);
                const float rise = std::pow (p, std::pow (2.0f, (s.curve - 0.5f) * 4.0f));
                double phase;
                if (s.rollRate == RollFree)
                {
                    rollPh += 2.0 * std::pow (4.0, (double) rise) * bpm / 60.0 / sr;
                    phase = frac (rollPh);
                }
                else
                {
                    const double beat = ht.playing ? ppq + i * bpm / 60.0 / sr : evT * bpm / 60.0 / sr;
                    phase = frac (beat * rollHitsPerBeat (s.rollRate, p));
                }
                const double k = 6.0 * std::pow (5.0, (s.rollDecay - 0.5) * 2.0);
                gate = 1.0f - rollUse * (1.0f - (float) std::exp (-phase * k));
            }
            evT += 1.0;

            l = (yl * s.synthFx + sl * s.sampleFx) * gate;
            r = (yr * s.synthFx + sr2 * s.sampleFx) * gate;
            dl0 = (yl * (1.0f - s.synthFx) + sl * (1.0f - s.sampleFx)) * gate;
            dr0 = (yr * (1.0f - s.synthFx) + sr2 * (1.0f - s.sampleFx)) * gate;
        }
        if (releasing)
        {
            fade -= fadeStep;
            if (fade <= 0.0f) { fade = 0.0f; active = sActive = false; }
            l *= fade;
            r *= fade;
            dl0 *= fade;
            dr0 *= fade;
        }
        if (! active && ! sActive) releasing = false;
        L[i] = l;
        if (R != L) R[i] = r;
        dryL[(size_t) i] = dl0;
        dryR[(size_t) i] = R != L ? dr0 : 0.5f * (dl0 + dr0);
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

    // the un-sent part joins after the FX, at the reverb's dry level so 100% sends sound like 1.0
    for (int i = 0; i < n; ++i)
    {
        L[i] += dryL[(size_t) i] * rp.dryLevel;
        if (R != L) R[i] += dryR[(size_t) i] * rp.dryLevel;
    }

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
