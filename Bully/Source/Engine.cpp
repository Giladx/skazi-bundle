#include "Engine.h"
#include <cmath>

namespace bully
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;
    inline float clamp01 (float x) { return juce::jlimit (0.0f, 1.0f, x); }
    // cheap tanh (Pade), used inside the ladder and the voice saturation
    inline float ftanh (float x) { x = juce::jlimit (-3.0f, 3.0f, x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
    inline float frac (float x) { return x - std::floor (x); }

    // A warp, worked out once per slice: `aux` holds the type's precomputed number
    struct WarpSetup { int type = WNone; float a = 0.0f, aux = 0.0f; };

    WarpSetup makeWarp (int type, float a)
    {
        WarpSetup w { type, a, 0.0f };
        switch (type)
        {
            case WSync:  w.aux = 1.0f + 7.0f * a; break;
            case WBendP:
            case WBendM: w.aux = 8.0f * a; break;
            case WPwm:   w.aux = 1.0f - 0.95f * a; break;
            case WAsym:  w.aux = 0.5f - 0.45f * a; break;
            case WQuant: w.aux = std::exp2 (10.0f - 9.0f * a); break;
            case WFm:    w.aux = 1.5f * a; break;
            default: break;
        }
        return w;
    }

    // how much brighter a warp makes the wave (picks a darker mip so it does not alias)
    float warpBrightness (const WarpSetup& w)
    {
        switch (w.type)
        {
            case WSync:  return w.aux;
            case WBendP: case WBendM: return 1.0f + 1.5f * w.a;
            case WPwm:   return 1.0f / w.aux;
            case WAsym:  return 1.0f + 2.0f * w.a;
            case WMirror: return 1.0f + w.a;
            case WFm:    return 1.0f + 3.0f * w.a;
            default:     return 1.0f;
        }
    }

    // phase in 0..1 -> warped phase; `gate` drops to 0 where PWM leaves silence
    inline float warpPhase (const WarpSetup& w, float p, float other, float& gate)
    {
        switch (w.type)
        {
            case WSync:  return frac (p * w.aux);
            case WBendP: return p * (1.0f + w.aux) / (1.0f + w.aux * p);
            case WBendM: return p / (1.0f + w.aux * (1.0f - p));
            case WPwm:   if (p < w.aux) return p / w.aux; gate = 0.0f; return 0.0f;
            case WAsym:  return p < w.aux ? 0.5f * p / w.aux : 0.5f + 0.5f * (p - w.aux) / (1.0f - w.aux);
            case WMirror: { const float m = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p; return juce::jmin (0.99999f, p + (m - p) * w.a); }
            case WQuant: return std::floor (p * w.aux) / w.aux;
            case WFm:    return frac (p + w.aux * other);
            default:     return p;
        }
    }

    inline float readCycle (const float* c, int size, float p)
    {
        const float idx = p * (float) size;
        int i = (int) idx;
        if (i >= size) i = size - 1;
        const float f = idx - (float) i;
        return c[i] + (c[i + 1] - c[i]) * f;
    }

    float lfoShape (int shape, float p, float shPrev, float shNext)
    {
        switch (shape)
        {
            case LSine:    return 0.5f - 0.5f * std::cos (twoPi * p);
            case LTri:     return p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
            case LSawUp:   return p;
            case LSawDown: return 1.0f - p;
            case LSquare:  return p < 0.5f ? 1.0f : 0.0f;
            case LPulse:   return p < 0.2f ? 1.0f : 0.0f;
            case LSH:      return shNext;
            case LSmooth:  return shPrev + (shNext - shPrev) * (0.5f - 0.5f * std::cos (pi * p));
            case LStairs:  return std::floor (p * 4.0f) / 3.0f;
            default:       { const float s = std::sin (pi * p); return s * s; } // HUMP
        }
    }

    const float vowels[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
}

//==============================================================================
void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    bank = &WaveBank::get();
    fx.prepare (sr);
    bufL.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    bufR.assign ((size_t) juce::jmax (512, maxBlock), 0.0f);
    slots.reserve (kSlots);
    held.reserve (128);
    reset();
}

void Engine::reset()
{
    for (auto& vc : voices)
    {
        vc.active = false;
        for (auto& e : vc.env) e.kill();
        for (auto& f : vc.filt) for (auto& c : f) c.reset();
    }
    held.clear();
    lastPitch = -1.0f;
    for (auto& l : lfo) l = {};
    for (auto& c : chaos) c = {};
    fx.reset();
}

//==============================================================================
void Engine::startVoice (Voice& vc, int note, float vel, bool retrigger, const float* v)
{
    const auto& ix = Ix::get();
    const bool wasIdle = ! vc.active;
    const float glide = v[ix.glide];
    vc.pitch = (glide > 0.0f && lastPitch >= 0.0f) ? (wasIdle ? lastPitch : vc.pitch) : (float) note;
    vc.pitchTarget = (float) note;
    lastPitch = (float) note;
    vc.note = note;
    vc.velocity = vel;
    vc.active = true;
    vc.released = false;
    vc.killing = false;
    vc.age = ++ageCounter;
    if (! retrigger) return;

    vc.random = rng.nextFloat();
    for (int e = 0; e < kEnv; ++e)
    {
        vc.env[e].set (v[ix.env[e].a], v[ix.env[e].d], v[ix.env[e].s] * 0.01f, v[ix.env[e].r], sr);
        vc.env[e].gate (true);
    }
    for (int o = 0; o < kOsc; ++o)
    {
        const float phs = v[ix.osc[o].phs] * 0.01f, rnd = v[ix.osc[o].rnd] * 0.01f;
        for (auto& p : vc.ph[(size_t) o]) p = frac (phs + rnd * rng.nextFloat());
    }
    if (wasIdle)
    {
        for (auto& f : vc.filt) for (auto& c : f) c.reset();
        for (auto& g : vc.gainL) for (auto& x : g) x = 0.0f;
        for (auto& g : vc.gainR) for (auto& x : g) x = 0.0f;
        for (auto& o : vc.oscLast) o = 0.0f;
        vc.seed = (juce::uint32) rng.nextInt() | 1u;
        // each voice starts slightly off and somewhere else in the stereo field, like a real analog poly
        const float driftAmt = v[ix.drift] * 0.01f;
        for (int o = 0; o < kOsc; ++o) vc.drift[o] = vc.driftTarget[o] = (rng.nextFloat() * 2.0f - 1.0f) * driftAmt * 0.12f;
        vc.pan = rng.nextFloat() * 2.0f - 1.0f;
        vc.boostLp[0] = vc.boostLp[1] = 0.0f;
    }
    noteStarted = true;
}

void Engine::noteOn (int note, float vel, const float* v)
{
    const auto& ix = Ix::get();
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    const int mode = (int) v[ix.vmode];

    if (mode != VPoly)
    {
        auto& vc = voices[0];
        const bool legato = mode == VLegato && vc.active && ! vc.released && held.size() > 1;
        if (legato)
        {
            vc.pitchTarget = (float) note;
            if (v[ix.glide] <= 0.0f) vc.pitch = (float) note;
            vc.note = note;
            lastPitch = (float) note;
        }
        else
            startVoice (vc, note, vel, true, v);
        for (int i = 1; i < kMaxVoices; ++i) // leaving poly mode: let the others go
            if (voices[(size_t) i].active) for (auto& e : voices[(size_t) i].env) e.gate (false);
        return;
    }

    const int poly = juce::jlimit (1, 16, (int) v[ix.poly]);
    int sounding = 0;
    for (auto& vc : voices) if (vc.active && ! vc.released && ! vc.killing) ++sounding;
    if (sounding >= poly)
    {
        Voice* oldest = nullptr;
        for (auto& vc : voices)
            if (vc.active && ! vc.released && ! vc.killing && (oldest == nullptr || vc.age < oldest->age)) oldest = &vc;
        if (oldest != nullptr) { oldest->killing = true; oldest->killFade = 1.0f; }
    }
    Voice* target = nullptr;
    for (auto& vc : voices) if (! vc.active) { target = &vc; break; }
    if (target == nullptr) // every slot busy: reuse the quietest released one, else the oldest
        for (auto& vc : voices)
            if (target == nullptr || (vc.released && ! target->released) || (vc.released == target->released && vc.age < target->age)) target = &vc;
    startVoice (*target, note, vel, true, v);
}

void Engine::noteOff (int note, const float* v)
{
    const auto& ix = Ix::get();
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    const int mode = (int) v[ix.vmode];
    if (mode != VPoly)
    {
        auto& vc = voices[0];
        if (! vc.active || vc.note != note) return;
        if (! held.empty())
        {
            const int back = held.back();
            if (mode == VLegato)
            {
                vc.pitchTarget = (float) back;
                if (v[ix.glide] <= 0.0f) vc.pitch = (float) back;
                vc.note = back;
                lastPitch = (float) back;
            }
            else
                startVoice (vc, back, vc.velocity, true, v);
            return;
        }
        vc.released = true;
        for (auto& e : vc.env) e.gate (false);
        return;
    }
    for (auto& vc : voices)
        if (vc.active && ! vc.released && vc.note == note)
        {
            vc.released = true;
            for (auto& e : vc.env) e.gate (false);
        }
}

//==============================================================================
void Engine::updateGlobals (const float* v, int len, const skazi::HostTime& ht)
{
    const auto& ix = Ix::get();
    for (int l = 0; l < kLfo; ++l)
    {
        auto& s = lfo[(size_t) l];
        const auto& p = ix.lfo[l];
        const int shape = (int) v[p.shp], mode = (int) v[p.mode];
        const bool sync = v[p.sync] > 0.5f;
        const double beats = skazi::noteBeats ((int) v[p.rate]);
        const double hz = sync ? ht.bpm / 60.0 / beats : (double) v[p.hz];
        const float start = v[p.phs] * 0.01f;
        bool wrapped = false;
        if (noteStarted && mode != LFree)
        {
            s.phase = start;
            s.done = false;
            wrapped = true;
        }
        else if (mode == LFree && sync && ht.playing)
        {
            const double ph = std::fmod (ht.ppq / beats + start, 1.0);
            wrapped = ph < s.phase;
            s.phase = ph;
        }
        else if (! s.done)
        {
            s.phase += hz * len / sr;
            if (s.phase >= 1.0)
            {
                if (mode == LEnv) { s.phase = 1.0; s.done = true; }
                else { s.phase -= std::floor (s.phase); wrapped = true; }
            }
        }
        if (wrapped) { s.shPrev = s.shNext; s.shNext = rng.nextFloat(); }
        const float val = lfoShape (shape, (float) juce::jmin (0.99999, s.phase), s.shPrev, s.shNext);
        const float smo = v[p.smo] * 0.01f;
        if (smo > 0.0f) s.smooth += (val - s.smooth) * (1.0f - std::exp (-(float) len / (smo * 0.25f * (float) sr)));
        else            s.smooth = val;
        src[SLfo1 + l] = s.smooth;
        uiLfo[(size_t) l].store (s.smooth);
        uiLfoPhase[(size_t) l].store ((float) s.phase);
    }
    noteStarted = false;

    for (int c = 0; c < kChaos; ++c)
    {
        auto& s = chaos[(size_t) c];
        const int type = (int) v[ix.chaos[c].type];
        const float rate = v[ix.chaos[c].rate] * 0.01f, smo = v[ix.chaos[c].smo] * 0.01f;
        float x = 0.0f, y = 0.0f;
        if (type == 2) // HENON map, stepped
        {
            s.acc += (0.5 + rate * rate * 200.0) * len / sr;
            while (s.acc >= 1.0)
            {
                s.acc -= 1.0;
                const double nx = 1.0 - 1.4 * s.x * s.x + s.y;
                s.y = 0.3 * s.x;
                s.x = nx;
                if (! std::isfinite (s.x) || std::abs (s.x) > 10.0) { s.x = 0.1; s.y = 0.0; }
            }
            x = (float) (s.x / 1.3); y = (float) (s.y / 0.4);
        }
        else
        {
            const double speed = type == 0 ? 0.05 + rate * rate * 25.0 : 0.2 + rate * rate * 60.0;
            const double t = speed * len / sr;
            const int steps = juce::jlimit (1, 64, (int) std::ceil (t / 0.004));
            const double dt = t / steps;
            for (int k = 0; k < steps; ++k)
            {
                double dx, dy, dz;
                if (type == 0) { dx = 10.0 * (s.y - s.x); dy = s.x * (28.0 - s.z) - s.y; dz = s.x * s.y - 8.0 / 3.0 * s.z; }
                else           { dx = -s.y - s.z; dy = s.x + 0.2 * s.y; dz = 0.2 + s.z * (s.x - 5.7); }
                s.x += dx * dt; s.y += dy * dt; s.z += dz * dt;
            }
            if (! std::isfinite (s.x) || std::abs (s.x) > 1000.0) { s.x = 0.1; s.y = s.z = 0.0; }
            const double sc = type == 0 ? 20.0 : 12.0;
            x = (float) (s.x / sc); y = (float) (s.y / sc);
        }
        x = juce::jlimit (-1.0f, 1.0f, x);
        s.sm = smo > 0.0f ? s.sm + (x - s.sm) * (1.0f - std::exp (-(float) len / (smo * 0.5f * (float) sr))) : x;
        src[SChaos1 + c] = s.sm;
        uiChaosX[(size_t) c].store (s.sm);
        uiChaosY[(size_t) c].store (juce::jlimit (-1.0f, 1.0f, y));
    }

    for (int m = 0; m < kMacro; ++m) src[SMacro1 + m] = v[ix.macro[m]] * 0.01f;
    src[SModWheel] = modWheel;
    src[SBend] = bend;
    src[SAfter] = after;
}

void Engine::computeMod (const Voice* vc, float* mod)
{
    std::fill (mod, mod + NumDests, 0.0f);
    if (slots.empty()) return;
    float s[NumSrcs];
    std::copy (src, src + NumSrcs, s);
    for (int e = 0; e < kEnv; ++e) s[SEnv1 + e] = vc != nullptr ? vc->env[e].level : 0.0f;
    s[SVel] = vc != nullptr ? vc->velocity : 0.0f;
    s[SNote] = vc != nullptr ? (float) vc->note / 127.0f : 0.0f;
    s[SRand] = vc != nullptr ? vc->random : 0.0f;
    for (auto& sl : slots) mod[sl.dst] += s[sl.src] * sl.amt;
}

//==============================================================================
void Engine::setupFilter (FilterSetup& fs, int f, const float* v, const float* mod, float noteHz)
{
    const auto& p = Ix::get().filt[f];
    const int base = f == 0 ? DF1Cut : DF2Cut;
    fs.on = v[p.on] > 0.5f;
    if (! fs.on) return;
    fs.type = (int) v[p.type];
    const float cutN = clamp01 (toNorm (p.cut, v[p.cut]) + mod[base]);
    float cut = fromNorm (p.cut, cutN) * std::pow (noteHz / 261.63f, v[p.key] * 0.01f);
    const float nyq = 0.45f * (float) sr;
    cut = juce::jlimit (20.0f, nyq, cut);
    fs.res = clamp01 (v[p.res] * 0.01f + mod[base + 1]);
    fs.drive = clamp01 (v[p.drv] * 0.01f + mod[base + 2]);
    fs.var = clamp01 (v[p.var] * 0.01f + mod[base + 3]);
    fs.mix = clamp01 (v[p.mix] * 0.01f + mod[base + 4]);
    fs.k = 2.0f - 1.96f * fs.res;

    for (int c = 0; c < 2; ++c)
    {
        const float spread = fs.type == FCombP || fs.type == FCombM || fs.type == FFormant ? 0.0f : fs.var * (c == 0 ? -0.5f : 0.5f);
        const float fc = juce::jlimit (20.0f, nyq, cut * std::exp2 (spread));
        fs.g[c] = std::tan (pi * fc / (float) sr);
        fs.ladG[c] = 1.0f - std::exp (-twoPi * juce::jmin (fc, 0.3f * (float) sr) / (float) sr);
        fs.sharpG[c] = fs.g[c] / (1.0f + fs.g[c]);
        switch (fs.type)
        {
            case FCombP: case FCombM:
                fs.combDelay[c] = juce::jlimit (2.0f, 4090.0f, (float) sr / juce::jmax (30.0f, cut));
                break;
            case FPhaser:
                for (int i = 0; i < 4; ++i)
                {
                    const float fi = juce::jlimit (20.0f, nyq, fc * std::exp2 (((float) i - 1.5f) * (0.2f + fs.var * 1.2f)));
                    const float t = std::tan (pi * fi / (float) sr);
                    fs.apA[c][i] = (t - 1.0f) / (t + 1.0f);
                }
                break;
            case FRing:
                fs.ringInc[c] = fc / (float) sr;
                break;
            case FFormant:
            {
                const float pos = cutN * 4.0f;
                const int a = juce::jmin (3, (int) pos);
                const float u = pos - (float) a, shift = std::exp2 (fs.var - 0.3f);
                for (int i = 0; i < 3; ++i)
                {
                    const float ff = juce::jlimit (60.0f, nyq, (vowels[a][i] + (vowels[a + 1][i] - vowels[a][i]) * u) * shift);
                    fs.formG[c][i] = std::tan (pi * ff / (float) sr);
                }
                fs.formK = 0.06f + 0.4f * (1.0f - fs.res);
                break;
            }
            default: break;
        }
    }
    fs.fbk = fs.res * 0.97f;

    // SHARP 24: self-oscillation starts at k = 4 at every cutoff, i.e. at ~95% RESONANCE.
    // The ladder loses 1/(1+k) of its passband to resonance; give most of it back so the bass stays.
    fs.sharpK = 4.2f * fs.res;
    fs.sharpComp = std::pow (1.0f + fs.sharpK, 0.75f);
}

float Engine::runFilter (const FilterSetup& fs, FilterChannel& ch, int c, float x)
{
    if (fs.drive > 0.001f) x = std::tanh (x * (1.0f + 5.0f * fs.drive)) / (1.0f + 2.0f * fs.drive);
    const float g = fs.g[c];
    float lp, bp, hp, y = x;
    switch (fs.type)
    {
        case FLp12: ch.s[0].tick (x, g, fs.k, lp, bp, hp); y = lp; break;
        case FLp24: // saturated 4-pole ladder: fat, squelchy, screams near full resonance
        {
            auto& s = ch.lad;
            const float gl = fs.ladG[c], k = fs.res * 4.1f;
            const float u = ftanh (x * (1.0f + 0.5f * k) - k * s[3]);
            s[0] += gl * (u - s[0]);
            s[1] += gl * (s[0] - s[1]);
            s[2] += gl * (s[1] - s[2]);
            s[3] += gl * (s[2] - s[3]);
            y = s[3];
            break;
        }
        case FHp12: ch.s[0].tick (x, g, fs.k, lp, bp, hp); y = hp; break;
        case FHp24: ch.s[0].tick (x, g, 1.85f, lp, bp, hp); ch.s[1].tick (hp, g, fs.k, lp, bp, hp); y = hp; break;
        case FBp12: ch.s[0].tick (x, g, fs.k, lp, bp, hp); y = bp * (0.4f + 0.6f * fs.k); break;
        case FNotch: ch.s[0].tick (x, g, fs.k, lp, bp, hp); y = lp + hp; break;
        case FPeak: ch.s[0].tick (x, g, 0.6f, lp, bp, hp); y = x + bp * 0.6f * fs.res * 4.0f; break;
        case FCombP: case FCombM:
        {
            const float sign = fs.type == FCombP ? 1.0f : -1.0f;
            const float rp = (float) ch.combW - fs.combDelay[c];
            const int i0 = (int) std::floor (rp);
            const float fr = rp - (float) i0;
            const float a = ch.comb[(size_t) (i0 & 4095)], b = ch.comb[(size_t) ((i0 + 1) & 4095)];
            const float d = a + (b - a) * fr;
            ch.combLp += (d - ch.combLp) * (1.0f - fs.var * 0.9f);
            const float w = x + sign * fs.fbk * ch.combLp;
            ch.comb[(size_t) ch.combW] = juce::jlimit (-4.0f, 4.0f, w);
            ch.combW = (ch.combW + 1) & 4095;
            y = w * (1.0f - fs.fbk * 0.6f);
            break;
        }
        case FFormant:
        {
            float sum = 0.0f;
            const float lv[3] = { 1.0f, 0.7f, 0.4f };
            for (int i = 0; i < 3; ++i) { ch.s[i].tick (x, fs.formG[c][i], fs.formK, lp, bp, hp); sum += bp * fs.formK * lv[i]; }
            y = sum * 1.8f;
            break;
        }
        case FPhaser:
        {
            float z = x + ch.phFb * fs.res * 0.85f;
            for (int i = 0; i < 4; ++i)
            {
                const float o = fs.apA[c][i] * z + ch.ap[i];
                ch.ap[i] = z - fs.apA[c][i] * o;
                z = o;
            }
            ch.phFb = z;
            y = 0.5f * (x + z);
            break;
        }
        case FRing:
            ch.ringPh += fs.ringInc[c];
            if (ch.ringPh >= 1.0) ch.ringPh -= 1.0;
            y = x * std::sin (twoPi * (float) ch.ringPh);
            break;
        case FSharp24: // zero-delay-feedback 4-pole ladder: cutoff where the knob says, strong, even resonance
        {
            // Each stage is a trapezoidal one-pole, y = G x + (1 - G) s. The feedback node is solved
            // within the sample (no unit delay), so the peak stays on the cutoff and the resonance
            // behaves the same across the whole range; tanh at the node keeps self-oscillation in check.
            auto& s = ch.lad;
            const float G = fs.sharpG[c], h = 1.0f - G, k = fs.sharpK;
            const float S = ((G * s[0] + s[1]) * G + s[2]) * G * h + s[3] * h; // the four states seen at the output
            const float G4 = G * G * G * G;
            const float u = ftanh ((x - k * S) / (1.0f + k * G4));
            float v = u;
            for (auto& st : s)
            {
                const float d = (v - st) * G;
                const float yy = d + st;
                st = yy + d;
                v = yy;
            }
            y = v * fs.sharpComp;
            break;
        }
        case FDirty: // overdriven ladder, every stage clipping
        {
            auto& s = ch.lad;
            const float gl = fs.ladG[c], k = fs.res * 4.3f;
            const float u = ftanh (x * 2.5f - k * s[3]);
            s[0] += gl * (u - s[0]);
            s[1] += gl * (ftanh (s[0] * 1.4f) - s[1]);
            s[2] += gl * (s[1] - s[2]);
            s[3] += gl * (ftanh (s[2] * 1.2f) - s[3]);
            y = s[3] * 0.85f;
            break;
        }
        default: break;
    }
    return x + (y - x) * fs.mix;
}

float Engine::noise (Voice& vc, int type, float tone)
{
    vc.seed ^= vc.seed << 13; vc.seed ^= vc.seed >> 17; vc.seed ^= vc.seed << 5;
    const float w = (float) (juce::int32) vc.seed * 4.656612e-10f;
    switch (type)
    {
        case NPink:
        {
            auto& b = vc.pink;
            b[0] = 0.99886f * b[0] + w * 0.0555179f; b[1] = 0.99332f * b[1] + w * 0.0750759f;
            b[2] = 0.96900f * b[2] + w * 0.1538520f; b[3] = 0.86650f * b[3] + w * 0.3104856f;
            b[4] = 0.55000f * b[4] + w * 0.5329522f; b[5] = -0.7616f * b[5] - w * 0.0168980f;
            const float out = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f) * 0.11f;
            b[6] = w * 0.115926f;
            return out;
        }
        case NBrown:   vc.brown = (vc.brown + 0.02f * w) / 1.02f; return vc.brown * 3.5f;
        case NCrackle: return std::abs (w) > 1.0f - (0.0004f + tone * tone * 0.01f) ? w * 3.0f : 0.0f;
        case NVinyl:   return (std::abs (w) > 0.9994f ? w * 2.5f : 0.0f) + w * 0.03f;
        case NDigital:
            vc.digPh += (80.0 + tone * tone * 16000.0) / sr;
            if (vc.digPh >= 1.0) { vc.digPh -= std::floor (vc.digPh); vc.digHold = w; }
            return vc.digHold;
        default: return w;
    }
}

//==============================================================================
void Engine::renderVoice (Voice& vc, const float* v, float* L, float* R, int len)
{
    const auto& ix = Ix::get();
    for (int e = 0; e < kEnv; ++e)
        vc.env[e].set (v[ix.env[e].a], v[ix.env[e].d], v[ix.env[e].s] * 0.01f, v[ix.env[e].r], sr);
    float mod[NumDests];
    computeMod (&vc, mod);

    const float glideMs = v[ix.glide];
    if (glideMs > 0.0f) vc.pitch += (vc.pitchTarget - vc.pitch) * (1.0f - std::exp (-(float) len / (glideMs * 0.001f * (float) sr)));
    else                vc.pitch = vc.pitchTarget;
    const float notePitch = vc.pitch + bend * v[ix.bend];

    // analog drift: each oscillator wanders a few cents on its own
    const float driftAmt = v[ix.drift] * 0.01f;
    const float driftCoef = 1.0f - std::exp (-(float) len / (0.5f * (float) sr));
    for (int o = 0; o < kOsc; ++o)
    {
        if (rng.nextFloat() < (float) len / (float) sr * 0.8f)
            vc.driftTarget[o] = (rng.nextFloat() * 2.0f - 1.0f) * driftAmt * 0.12f;
        vc.drift[o] += (vc.driftTarget[o] * (driftAmt > 0.0f ? 1.0f : 0.0f) - vc.drift[o]) * driftCoef;
    }
    const float noteHz = 440.0f * std::exp2 ((notePitch - 69.0f) / 12.0f);
    const bool parallel = v[ix.froute] > 0.5f;

    // oscillators
    struct OscRun
    {
        bool on = false; const float* c0 = nullptr; const float* c1 = nullptr; int size = 64; float u = 0.0f;
        WarpSetup w1, w2; int uni = 1; float inc[8] {}, gl0[8] {}, gr0[8] {}, dgl[8] {}, dgr[8] {}; int bus = 0;
    } osc[kOsc];
    // bus: bit 0 = filter 1, bit 1 = filter 2
    auto busFor = [&] (bool f1, bool f2) { return (f1 ? 1 : 0) | (f2 ? 2 : 0); };
    for (int o = 0; o < kOsc; ++o)
    {
        const auto& p = ix.osc[o];
        auto& r = osc[o];
        r.on = v[p.on] > 0.5f;
        if (! r.on) continue;
        const float* m = mod + DOsc0 + o * kOscDests;
        const int tab = juce::jlimit (0, kTables - 1, (int) v[p.tab]);
        const float pos = clamp01 (v[p.pos] * 0.01f + m[OPos]);
        r.w1 = makeWarp ((int) v[p.w1t], clamp01 (v[p.w1] * 0.01f + m[OW1]));
        r.w2 = makeWarp ((int) v[p.w2t], clamp01 (v[p.w2] * 0.01f + m[OW2]));
        const float semis = v[p.oct] * 12.0f + v[p.semi] + v[p.fine] * 0.01f + m[OPitch] * kPitchModRange + vc.drift[o];
        const float det = clamp01 (v[p.det] * 0.01f + m[ODet]), lvl = clamp01 (v[p.lvl] * 0.01f + m[OLvl]);
        const float pan = juce::jlimit (-1.0f, 1.0f, v[p.pan] * 0.01f + m[OPan] * 2.0f);
        r.uni = juce::jlimit (1, 8, (int) v[p.uni]);
        const float bld = v[p.bld] * 0.01f, spread = 0.6f * det * std::sqrt (det);
        const float hz = juce::jlimit (1.0f, 0.49f * (float) sr, noteHz * std::exp2 (semis / 12.0f));
        const int mip = WaveBank::mipFor (hz * std::exp2 (spread / 12.0f) * warpBrightness (r.w1) * warpBrightness (r.w2), sr);
        const float fp = pos * (WaveBank::kFrames - 1);
        const int f0 = (int) fp, f1 = juce::jmin (WaveBank::kFrames - 1, f0 + 1);
        r.u = fp - (float) f0;
        r.c0 = bank->cycle (tab, mip, f0);
        r.c1 = bank->cycle (tab, mip, f1);
        r.size = WaveBank::size (mip);
        const float norm = lvl / std::sqrt ((float) r.uni);
        for (int k = 0; k < r.uni; ++k)
        {
            const float t = r.uni > 1 ? (float) k / (float) (r.uni - 1) * 2.0f - 1.0f : 0.0f;
            r.inc[k] = hz * std::exp2 (t * spread / 12.0f) / (float) sr;
            const float pk = juce::jlimit (-1.0f, 1.0f, pan + t * bld);
            const float gl = std::cos ((pk + 1.0f) * pi * 0.25f) * norm, gr = std::sin ((pk + 1.0f) * pi * 0.25f) * norm;
            r.gl0[k] = vc.gainL[o][k]; r.gr0[k] = vc.gainR[o][k];
            r.dgl[k] = (gl - r.gl0[k]) / (float) len; r.dgr[k] = (gr - r.gr0[k]) / (float) len;
            vc.gainL[o][k] = gl; vc.gainR[o][k] = gr;
        }
        r.bus = busFor (v[p.f1] > 0.5f, v[p.f2] > 0.5f);
    }

    // noise
    const bool noiseOn = v[ix.non] > 0.5f;
    const int noiseType = (int) v[ix.ntype];
    const float nTone = clamp01 (v[ix.ntone] * 0.01f + mod[DNTone]), nLvl = clamp01 (v[ix.nlvl] * 0.01f + mod[DNLvl]);
    const float nPan = v[ix.npan] * 0.01f;
    const float nGL = std::cos ((nPan + 1.0f) * pi * 0.25f) * nLvl * 0.7f, nGR = std::sin ((nPan + 1.0f) * pi * 0.25f) * nLvl * 0.7f;
    const float nA = 1.0f - std::exp (-twoPi * juce::jmin (0.45f * (float) sr, 200.0f * std::exp2 (nTone * 6.6f)) / (float) sr);
    const int nBus = busFor (v[ix.nf1] > 0.5f, v[ix.nf2] > 0.5f);

    FilterSetup fs[2];
    setupFilter (fs[0], 0, v, mod, noteHz);
    setupFilter (fs[1], 1, v, mod, noteHz);

    const float velGain = 1.0f - v[ix.velo] * 0.01f * (1.0f - vc.velocity);
    // voice character: saturation, analog low-end boost, stereo spread
    const float sat = v[ix.sat] * 0.01f, satDrive = 1.0f + 4.0f * sat, satNorm = 1.0f / std::sqrt (satDrive);
    const float boost = v[ix.boost] * 0.01f * 1.2f;
    const float boostA = 1.0f - std::exp (-twoPi * 160.0f / (float) sr);
    const float sp = juce::jlimit (-1.0f, 1.0f, vc.pan * v[ix.spread] * 0.01f);
    const float spL = 1.0f - juce::jmax (0.0f, sp) * 0.7f, spR = 1.0f - juce::jmax (0.0f, -sp) * 0.7f;
    const float fadeStep = 1.0f / (0.003f * (float) sr);

    for (int i = 0; i < len; ++i)
    {
        const float amp = vc.env[0].next();
        for (int e = 1; e < kEnv; ++e) vc.env[e].next();

        // buses: [0] direct, [1] filter 1, [2] filter 2 (L and R)
        float bus[3][2] {};
        auto add = [&] (int b, float l, float r)
        {
            if (b == 0) { bus[0][0] += l; bus[0][1] += r; return; }
            if (b & 1) { bus[1][0] += l; bus[1][1] += r; }
            if ((b & 2) && (parallel || ! (b & 1))) { bus[2][0] += l; bus[2][1] += r; }
        };

        float newLast[kOsc] {};
        for (int o = 0; o < kOsc; ++o)
        {
            auto& r = osc[o];
            if (! r.on) continue;
            const float other = vc.oscLast[(o + 1) % kOsc];
            float sl = 0.0f, sr2 = 0.0f;
            auto& ph = vc.ph[(size_t) o];
            for (int k = 0; k < r.uni; ++k)
            {
                float gate = 1.0f;
                float p = (float) ph[(size_t) k];
                p = warpPhase (r.w1, p, other, gate);
                p = warpPhase (r.w2, p, other, gate);
                const float a = readCycle (r.c0, r.size, p), b = readCycle (r.c1, r.size, p);
                float s = (a + (b - a) * r.u) * gate;
                if (r.w1.type == WRing) s *= 1.0f + r.w1.a * (other - 1.0f);
                if (r.w2.type == WRing) s *= 1.0f + r.w2.a * (other - 1.0f);
                if (k == 0) newLast[o] = s;
                const float fi = (float) i;
                sl += s * (r.gl0[k] + r.dgl[k] * fi);
                sr2 += s * (r.gr0[k] + r.dgr[k] * fi);
                double np = ph[(size_t) k] + r.inc[k];
                if (np >= 1.0) np -= 1.0;
                ph[(size_t) k] = np;
            }
            add (r.bus, sl, sr2);
        }
        for (int o = 0; o < kOsc; ++o) vc.oscLast[o] = newLast[o];

        if (noiseOn)
        {
            float nz = noise (vc, noiseType, nTone);
            if (noiseType != NDigital && noiseType != NCrackle) { vc.noiseLp += (nz - vc.noiseLp) * nA; nz = vc.noiseLp; }
            add (nBus, nz * nGL, nz * nGR);
        }

        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            float y1 = fs[0].on ? runFilter (fs[0], vc.filt[0][c], c, bus[1][c]) : bus[1][c];
            float y;
            if (parallel) y = y1 + (fs[1].on ? runFilter (fs[1], vc.filt[1][c], c, bus[2][c]) : bus[2][c]);
            else
            {
                const float in2 = y1 + bus[2][c];
                y = fs[1].on ? runFilter (fs[1], vc.filt[1][c], c, in2) : in2;
            }
            y += bus[0][c];
            if (sat > 0.001f) y = ftanh (y * satDrive) * satNorm;
            vc.boostLp[c] += (y - vc.boostLp[c]) * boostA;
            out[c] = (y + vc.boostLp[c] * boost) * (c == 0 ? spL : spR);
        }

        float g = amp * velGain * 0.8f;
        if (vc.killing)
        {
            vc.killFade -= fadeStep;
            if (vc.killFade <= 0.0f) { vc.killFade = 0.0f; vc.active = false; }
            g *= vc.killFade;
        }
        L[i] += out[0] * g;
        R[i] += out[1] * g;
        if (! vc.active) break;
    }
    if (vc.env[0].stage == Adsr::Idle) vc.active = false;
    if (! vc.active) vc.killing = false;
}

//==============================================================================
void Engine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* v, const skazi::HostTime& ht)
{
    const auto& ix = Ix::get();
    const int n = out.getNumSamples();
    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); }

    if (panicFlag)
    {
        panicFlag = false;
        for (auto& vc : voices) { vc.active = false; for (auto& e : vc.env) e.kill(); }
        held.clear();
        fx.reset();
    }

    slots.clear();
    for (int s = 0; s < kSlots; ++s)
    {
        const int a = (int) v[ix.slot[s].src], d = (int) v[ix.slot[s].dst];
        const float amt = v[ix.slot[s].amt] * 0.01f;
        if (a > 0 && a < NumSrcs && d > 0 && d < NumDests && amt != 0.0f) slots.push_back ({ a, d, amt });
    }

    auto it = midi.cbegin();
    for (int pos = 0; pos < n; pos += kSlice)
    {
        const int len = juce::jmin (kSlice, n - pos);
        for (; it != midi.cend() && (*it).samplePosition < pos + len; ++it)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), v);
            else if (m.isNoteOff()) noteOff (m.getNoteNumber(), v);
            else if (m.isPitchWheel()) bend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8191.0f);
            else if (m.isController() && m.getControllerNumber() == 1) modWheel = (float) m.getControllerValue() / 127.0f;
            else if (m.isChannelPressure()) after = (float) m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch()) after = (float) m.getAfterTouchValue() / 127.0f;
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                for (auto& vc : voices) if (vc.active) { vc.released = true; for (auto& e : vc.env) e.gate (false); }
        }

        auto sliceHt = ht;
        sliceHt.ppq = ht.ppq + pos * ht.bpm / 60.0 / sr;
        updateGlobals (v, len, sliceHt);

        float* L = bufL.data() + pos;
        float* R = bufR.data() + pos;
        std::fill (L, L + len, 0.0f);
        std::fill (R, R + len, 0.0f);
        Voice* newest = nullptr;
        int count = 0;
        for (auto& vc : voices)
            if (vc.active)
            {
                renderVoice (vc, v, L, R, len);
                ++count;
                if (vc.active && (newest == nullptr || vc.age > newest->age)) newest = &vc;
            }

        // FX, with modulation from the global sources and the newest voice
        float gm[NumDests];
        computeMod (newest, gm);
        auto clampAdd = [&] (int idx, int dest) { return clamp01 (v[idx] * 0.01f + gm[dest]); };
        FxValues f;
        f.hyOn = v[ix.hyon] > 0.5f; f.hyVoices = (int) v[ix.hyuni]; f.hyDetune = v[ix.hydet] * 0.01f; f.hyRate = v[ix.hyrate] * 0.01f;
        f.hyMix = clampAdd (ix.hymix, DHyMix);
        f.dsOn = v[ix.dson] > 0.5f; f.dsType = (int) v[ix.dstype]; f.dsDrive = clampAdd (ix.dsdrv, DDsDrv); f.dsTone = v[ix.dstone];
        f.dsMix = clampAdd (ix.dsmix, DDsMix);
        f.moOn = v[ix.moon] > 0.5f; f.moType = (int) v[ix.motype];
        f.moRate = fromNorm (ix.morate, toNorm (ix.morate, v[ix.morate]) + gm[DMoRate]);
        f.moDepth = v[ix.modep] * 0.01f; f.moFb = v[ix.mofb] * 0.01f; f.moMix = clampAdd (ix.momix, DMoMix);
        f.fsOn = v[ix.fson] > 0.5f; f.fsFreq = fromNorm (ix.fsfrq, toNorm (ix.fsfrq, v[ix.fsfrq]) + gm[DFsFrq]);
        f.fsMix = clampAdd (ix.fsmix, DFsMix);
        f.otOn = v[ix.oton] > 0.5f; f.otDepth = clampAdd (ix.otdep, DOtDep); f.otTime = v[ix.ottime] * 0.01f;
        f.otUp = v[ix.otup] * 0.01f; f.otDown = v[ix.otdn] * 0.01f;
        f.eqOn = v[ix.eqon] > 0.5f; f.eqLow = v[ix.eqlo]; f.eqMidF = v[ix.eqmidf]; f.eqMid = v[ix.eqmid]; f.eqHigh = v[ix.eqhi];
        f.dlOn = v[ix.dlon] > 0.5f;
        f.dlSeconds = juce::jmin (4.0f, v[ix.dlsync] > 0.5f ? (float) (skazi::noteBeats ((int) v[ix.dlnote]) * 60.0 / ht.bpm) : v[ix.dlms] * 0.001f);
        f.dlFb = juce::jmin (0.95f, clampAdd (ix.dlfb, DDlFb)); f.dlPingPong = v[ix.dlpp] > 0.5f; f.dlTone = v[ix.dltone] * 0.01f;
        f.dlMix = clampAdd (ix.dlmix, DDlMix);
        f.rvOn = v[ix.rvon] > 0.5f; f.rvSize = clampAdd (ix.rvsize, DRvSize); f.rvDecay = v[ix.rvdec] * 0.01f; f.rvDamp = v[ix.rvdamp] * 0.01f;
        f.rvPre = v[ix.rvpre]; f.rvWidth = v[ix.rvwid] * 0.01f; f.rvMix = clampAdd (ix.rvmix, DRvMix);
        f.spOn = v[ix.spon] > 0.5f; f.spLow = v[ix.splo]; f.spHigh = v[ix.sphi];
        fx.process (L, R, len, f);

        if (pos + len >= n)
        {
            for (int d = 0; d < NumDests; ++d) uiDestMod[(size_t) d].store (gm[d]);
            for (int e = 0; e < kEnv; ++e) uiEnv[(size_t) e].store (newest != nullptr ? newest->env[e].level : 0.0f);
            uiVoices.store (count);
        }
    }

    const float gain = juce::Decibels::decibelsToGain (v[ix.vol]);
    float* oL = out.getWritePointer (0);
    float* oR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float l = bufL[(size_t) i] * gain, r = bufR[(size_t) i] * gain;
        if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.0f; fx.reset(); }
        l = skazi::safeClip (l);
        r = skazi::safeClip (r);
        if (oR != nullptr) { oL[i] = l; oR[i] = r; }
        else oL[i] = 0.5f * (l + r);
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    for (int c = 2; c < out.getNumChannels(); ++c) out.clear (c, 0, n);
    uiOut.store (peak);
    peak *= (float) std::exp (-(double) n / (0.25 * sr));
    uiBend.store (bend);
    uiMod.store (modWheel);
}
} // namespace bully
