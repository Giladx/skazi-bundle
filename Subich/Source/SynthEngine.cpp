#include "SynthEngine.h"
#include "../../Common/SkaziCore.h"
#include <cmath>

namespace subich
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // polyBLEP residual for band-limited steps
    inline double blep (double t, double dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    inline double frac (double x) { return x - std::floor (x); }
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.o1wave = (int) v ("o1wave"); s.o1oct = (int) v ("o1oct"); s.o1semi = (int) v ("o1semi");
    s.o1fine = v ("o1fine"); s.o1shape = v ("o1shape") * 0.01f; s.o1level = v ("o1level") * 0.01f;
    s.o2wave = (int) v ("o2wave"); s.o2oct = (int) v ("o2oct"); s.o2semi = (int) v ("o2semi");
    s.o2detune = v ("o2detune"); s.o2shape = v ("o2shape") * 0.01f; s.o2level = v ("o2level") * 0.01f;
    s.o2sync = v ("o2sync") > 0.5f; s.o2fm = v ("o2fm") > 0.5f;
    s.subwave = (int) v ("subwave"); s.suboct = (int) v ("suboct"); s.sublevel = v ("sublevel") * 0.01f; s.noise = v ("noise") * 0.01f;
    s.f1type = (int) v ("f1type"); s.f1cut = v ("f1cut"); s.f1res = v ("f1res") * 0.01f; s.f1env = v ("f1env") * 0.01f;
    s.f1drive = v ("f1drive") * 0.01f; s.f1key = v ("f1key") * 0.01f;
    s.f2type = (int) v ("f2type"); s.f2cut = v ("f2cut"); s.f2res = v ("f2res") * 0.01f; s.f2env = v ("f2env") * 0.01f;
    s.f2drive = v ("f2drive") * 0.01f; s.f2key = v ("f2key") * 0.01f;
    s.parallel = v ("routing") > 0.5f;
    s.aa = v ("aa"); s.ad = v ("ad"); s.as = v ("as") * 0.01f; s.ar = v ("ar");
    s.fa = v ("fa"); s.fd = v ("fd"); s.fs = v ("fs") * 0.01f; s.fr = v ("fr");
    s.l1shape = (int) v ("l1shape"); s.l1sync = v ("l1sync") > 0.5f; s.l1rate = (int) v ("l1rate"); s.l1hz = v ("l1hz");
    s.l1depth = v ("l1depth") * 0.01f; s.l1dest = (int) v ("l1dest");
    s.l2shape = (int) v ("l2shape"); s.l2sync = v ("l2sync") > 0.5f; s.l2rate = (int) v ("l2rate"); s.l2hz = v ("l2hz");
    s.l2depth = v ("l2depth") * 0.01f; s.l2dest = (int) v ("l2dest");
    s.mode = (int) v ("mode"); s.glideMs = v ("glide"); s.velo = v ("velo") * 0.01f; s.bend = (int) v ("bend");
    s.drive = v ("drive") * 0.01f; s.volume = juce::Decibels::decibelsToGain (v ("vol"));
    return s;
}

//==============================================================================
float Engine::Env::next (float a, float d, float s, float r, double sr)
{
    auto coef = [sr] (float ms) { return (float) (1.0 - std::exp (-4.6 / (juce::jmax (1.0f, ms) * 0.001 * sr))); };
    switch (stage)
    {
        case Attack:
            if (a <= 0.5f) level = 1.0f;
            else level += (float) (1.0 / (a * 0.001 * sr));
            if (level >= 1.0f) { level = 1.0f; stage = Decay; }
            break;
        case Decay:
            level += (s - level) * coef (d);
            if (std::abs (level - s) < 1.0e-4f) { level = s; stage = Sustain; }
            break;
        case Sustain:
            level = s;
            break;
        case Release:
            level -= level * coef (r);
            if (level < 1.0e-4f) { level = 0.0f; stage = Idle; }
            break;
        case Idle:
        default:
            level = 0.0f;
            break;
    }
    return level;
}

void Engine::Filter::setCoefs (float cutHz, float res, int type, double sr)
{
    cutHz = juce::jlimit (20.0f, (float) juce::jmin (18000.0, 0.45 * sr), cutHz);
    g = (float) std::tan (juce::MathConstants<double>::pi * cutHz / sr);
    const float q = type == Acid ? 0.6f + res * res * 10.0f : 0.5f + res * res * 12.0f;
    k = 1.0f / q;
    kb = type == Acid ? 1.0f / (0.6f + res * 2.0f) : 1.6f;
}

float Engine::Filter::process (float x, int type, float drive)
{
    if (drive > 0.0f)
    {
        const float gain = 1.0f + drive * 4.0f;
        x = std::tanh (x * gain) / std::pow (gain, 0.65f);
    }
    float lp, bp, hp, lp2, bp2, hp2;
    a.tick (x, g, k, lp, bp, hp);
    switch (type)
    {
        case LP24: b.tick (lp, g, kb, lp2, bp2, hp2); return lp2;
        case LP12: return lp;
        case HP12: return hp;
        case BP:   return bp * k;
        case Notch: return x - k * bp;
        case Acid:
        {
            b.tick (std::tanh (lp * 1.5f) / 1.5f, g, kb, lp2, bp2, hp2);
            return std::tanh (lp2 * 1.2f) / 1.2f;
        }
        default: return lp;
    }
}

//==============================================================================
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice {};
    held.clear();
    l1phase = l2phase = l1last = l2last = 0;
    outPeak = 0;
}

void Engine::panic()
{
    for (auto& v : voices) v = Voice {};
    held.clear();
}

float Engine::lfoValue (int shape, double phase, float& held, double& lastPhase, juce::Random& rng)
{
    if (phase < lastPhase) held = rng.nextFloat() * 2.0f - 1.0f; // new step each cycle
    lastPhase = phase;
    switch (shape)
    {
        case LSine:   return (float) std::sin (twoPi * phase);
        case LTri:    return (float) (1.0 - 4.0 * std::abs (phase - 0.5));
        case LSaw:    return (float) (1.0 - 2.0 * phase);
        case LSquare: return phase < 0.5 ? 1.0f : -1.0f;
        case LSH:     return held;
        default:      return 0.0f;
    }
}

float Engine::osc (int wave, double& phase, double dt, float shape, float pmod, bool& wrapped)
{
    const double p = frac (phase + pmod);
    double y = 0.0;
    switch (wave)
    {
        case Sine: y = std::sin (twoPi * p + shape * 1.5 * std::sin (twoPi * p)); break;
        case Tri:
        {
            const double t = 1.0 - 4.0 * std::abs (p - 0.5);
            y = shape > 0.0f ? std::sin (t * (1.0 + shape * 3.0) * juce::MathConstants<double>::halfPi) : t;
            break;
        }
        case Saw:
        {
            y = 2.0 * p - 1.0 - blep (p, dt);
            if (shape > 0.0f)
            {
                const double p2 = frac (2.0 * p);
                y = y * (1.0 - 0.5 * shape) + (2.0 * p2 - 1.0 - blep (p2, 2.0 * dt)) * 0.5 * shape;
            }
            break;
        }
        case Square:
        case Pulse:
        {
            const double w = wave == Square ? 0.5 - shape * 0.4 : 0.25 - shape * 0.2;
            y = (p < w ? 1.0 : -1.0) + blep (p, dt) - blep (frac (p + 1.0 - w), dt);
            break;
        }
        case FM:    y = std::sin (twoPi * p + shape * 5.0 * std::sin (2.0 * twoPi * p)); break;
        case Noise: y = rng.nextFloat() * 2.0f - 1.0f; break;
        default: break;
    }
    phase += dt;
    wrapped = phase >= 1.0;
    if (wrapped) phase -= 1.0;
    return (float) y;
}

//==============================================================================
void Engine::startVoice (Voice& v, int note, float velocity, bool retrigger, bool glide, const Settings& s)
{
    const bool wasActive = v.isActive();
    v.note = note;
    v.velocity = velocity;
    v.gated = true;
    v.logTarget = std::log2 (juce::MidiMessage::getMidiNoteInHertz (note));
    if (! glide || s.glideMs <= 0.0f || ! wasActive)
        v.logFreq = v.logTarget;
    if (retrigger)
    {
        if (! wasActive || v.amp.level < 0.05f)
        {
            v.ph1 = v.ph2 = v.phSub = 0.0; // same punch on every note
            if (! wasActive) { v.f1.reset(); v.f2.reset(); }
        }
        v.amp.stage = Env::Attack;
        v.filt.stage = Env::Attack;
    }
    v.coefCounter = 0;
    v.age = ++ageCounter;
}

void Engine::noteOn (int note, float velocity, const Settings& s)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);

    if (s.mode == Poly)
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.gated && v.note == note) target = &v;
        if (target == nullptr) for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age < target->age) target = &v;
        }
        startVoice (*target, note, velocity, true, false, s);
        return;
    }

    auto& v = voices[0];
    const bool wasGated = v.gated && v.isActive();
    if (s.mode == Legato && wasGated) startVoice (v, note, velocity, false, true, s);
    else                              startVoice (v, note, velocity, true, true, s);
}

void Engine::noteOff (int note, const Settings& s)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());

    if (s.mode == Poly)
    {
        for (auto& v : voices)
            if (v.gated && v.note == note) { v.gated = false; v.amp.gate (false); v.filt.gate (false); }
        return;
    }

    auto& v = voices[0];
    if (v.note != note || ! v.gated) return;
    if (! held.empty())
        startVoice (v, held.back(), v.velocity, s.mode == Mono, true, s);
    else
    {
        v.gated = false;
        v.amp.gate (false);
        v.filt.gate (false);
    }
}

float Engine::renderVoice (Voice& v, const Settings& s, float lfo1, float lfo2)
{
    // modulation sums per target
    auto mod = [&] (int dest) { return (s.l1dest == dest ? lfo1 * s.l1depth : 0.0f) + (s.l2dest == dest ? lfo2 * s.l2depth : 0.0f); };
    const float mCut1 = mod (DCut1) + mod (DCutBoth), mCut2 = mod (DCut2) + mod (DCutBoth);
    const float mPitch = mod (DPitch), mShape = mod (DShape), mVol = mod (DVolume), mRes = mod (DReso1);

    if (s.glideMs > 0.0f && s.mode != Poly)
        v.logFreq += (v.logTarget - v.logFreq) * (1.0 - std::exp (-3.0 / (s.glideMs * 0.001 * sr)));
    else
        v.logFreq = v.logTarget;

    const double noteHz = std::exp2 (v.logFreq + (bendSemis + mPitch * 12.0f) / 12.0);
    const double f1 = noteHz * std::exp2 (s.o1oct + s.o1semi / 12.0 + s.o1fine / 1200.0);
    const double f2 = noteHz * std::exp2 (s.o2oct + s.o2semi / 12.0 + s.o2detune / 1200.0);
    const double fs = noteHz * (s.suboct == 0 ? 0.5 : 0.25);
    const double nyq = 0.48 * sr;

    const float a = v.amp.next (s.aa, s.ad, s.as, s.ar, sr);
    const float fe = v.filt.next (s.fa, s.fd, s.fs, s.fr, sr);

    bool w1 = false, w2 = false, ws = false;
    const float sh1 = juce::jlimit (0.0f, 1.0f, s.o1shape + mShape * 0.5f);
    const float sh2 = juce::jlimit (0.0f, 1.0f, s.o2shape + mShape * 0.5f);
    const float y2 = osc (s.o2wave, v.ph2, juce::jmin (f2, nyq) / sr, sh2, 0.0f, w2);
    const float y1 = osc (s.o1wave, v.ph1, juce::jmin (f1, nyq) / sr, sh1, s.o2fm ? y2 * s.o2level * 0.5f : 0.0f, w1);
    if (s.o2sync && w1) v.ph2 = v.ph1 * (f2 / juce::jmax (1.0, f1));
    const float sub = s.subwave == 0 ? (float) std::sin (twoPi * v.phSub) : (float) (1.0 - 4.0 * std::abs (v.phSub - 0.5));
    v.phSub += fs / sr;
    ws = v.phSub >= 1.0;
    if (ws) v.phSub -= 1.0;
    const float nz = s.noise > 0.0f ? (rng.nextFloat() * 2.0f - 1.0f) * s.noise * 0.5f : 0.0f;

    float x = (y1 * s.o1level + (s.o2fm ? 0.0f : y2 * s.o2level) + sub * s.sublevel + nz) * 0.65f;

    if (v.coefCounter-- <= 0)
    {
        v.coefCounter = 7;
        const float velScale = 1.0f - s.velo * (1.0f - v.velocity) * 0.5f;
        const float key = (v.note - 48) / 12.0f;
        const float c1 = s.f1cut * std::exp2 (s.f1env * velScale * fe * 7.0f + s.f1key * key + mCut1 * 4.0f);
        const float c2 = s.f2cut * std::exp2 (s.f2env * velScale * fe * 7.0f + s.f2key * key + mCut2 * 4.0f);
        v.f1.setCoefs (c1, juce::jlimit (0.0f, 1.0f, s.f1res + mRes * 0.5f), s.f1type, sr);
        v.f2.setCoefs (c2, s.f2res, s.f2type, sr);
        if (v.age == ageCounter) { uiCut1.store (c1); uiCut2.store (c2); uiFreq.store ((float) noteHz); }
    }

    const float o1 = v.f1.process (x, s.f1type, s.f1drive);
    x = s.parallel ? 0.5f * (o1 + v.f2.process (x, s.f2type, s.f2drive)) : v.f2.process (o1, s.f2type, s.f2drive);

    const float trem = 1.0f - std::abs (mVol) * 0.5f - mVol * 0.5f;
    const float velGain = 1.0f - s.velo * (1.0f - v.velocity);
    return x * a * velGain * juce::jlimit (0.0f, 1.0f, trem);
}

void Engine::render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead)
{
    const int n = out.getNumSamples();
    const auto ht = skazi::hostTime (playHead);
    if (ht.playing) ppq = ht.ppq;
    const double beatsPerSample = ht.bpm / (60.0 * sr);
    const double l1beats = skazi::noteBeats (s.l1rate), l2beats = skazi::noteBeats (s.l2rate);
    const float driveGain = 1.0f + s.drive * 6.0f, driveNorm = std::pow (driveGain, 0.65f);
    const float peakDecay = (float) std::exp (-(double) n / (0.2 * sr));

    out.clear();
    auto it = midi.cbegin();
    int sw = scopeWrite.load();

    // the MOD wheel adds LFO 1 depth on top of its knob
    Settings eff = s;
    eff.l1depth = s.l1depth + modWheel * (1.0f - s.l1depth);
    bendSemis = bendRaw * s.bend;

    for (int i = 0; i < n; ++i)
    {
        for (; it != midi.cend() && (*it).samplePosition <= i; ++it)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())             noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
            else if (m.isNoteOff())       noteOff (m.getNoteNumber(), s);
            else if (m.isPitchWheel())
            {
                bendRaw = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
                bendSemis = bendRaw * s.bend;
            }
            else if (m.isControllerOfType (1))
            {
                modWheel = m.getControllerValue() / 127.0f;
                eff.l1depth = s.l1depth + modWheel * (1.0f - s.l1depth);
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                held.clear();
                for (auto& v : voices) { v.gated = false; v.amp.gate (false); v.filt.gate (false); }
            }
        }

        if (s.l1sync) l1phase = frac (ppq / l1beats); else l1phase = frac (l1phase + s.l1hz / sr);
        if (s.l2sync) l2phase = frac (ppq / l2beats); else l2phase = frac (l2phase + s.l2hz / sr);
        ppq += beatsPerSample;
        const float lfo1 = lfoValue (s.l1shape, l1phase, l1held, l1last, rng);
        const float lfo2 = lfoValue (s.l2shape, l2phase, l2held, l2last, rng);

        float sum = 0.0f;
        const int count = s.mode == Poly ? (int) voices.size() : 1;
        for (int k = 0; k < count; ++k)
            if (voices[(size_t) k].isActive())
                sum += renderVoice (voices[(size_t) k], eff, lfo1, lfo2);

        const float y = skazi::safeClip (std::tanh (sum * driveGain) / driveNorm * s.volume);
        for (int c = 0; c < out.getNumChannels(); ++c)
            out.setSample (c, i, y);
        outPeak = juce::jmax (outPeak, std::abs (y));
        scope[(size_t) sw].store (y);
        sw = (sw + 1) % kScopeSize;
    }

    scopeWrite.store (sw);
    uiBend.store (bendRaw);
    uiMod.store (modWheel);
    uiOut.store (outPeak);
    outPeak *= peakDecay;
    int newest = -1; juce::uint32 best = 0;
    for (int k = 0; k < (int) voices.size(); ++k)
        if (voices[(size_t) k].isActive() && voices[(size_t) k].gated && voices[(size_t) k].age >= best) { best = voices[(size_t) k].age; newest = voices[(size_t) k].note; }
    uiNote.store (newest);
}
} // namespace subich
