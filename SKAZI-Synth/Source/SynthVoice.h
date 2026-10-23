#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "Params.h"
#include "Wavetables.h"

constexpr int kMaxUnison = 16;

// Rational tanh approximation (Pade), accurate to ~0.1% in the audible range and much cheaper than std::tanh
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.f, 3.f);
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

//==============================================================================
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0.f, sr = 44100.f;

    void noteOn() { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    bool isActive() const { return stage != Idle; }
    void reset() { stage = Idle; value = 0.f; }

    float next (float a, float d, float s, float r)
    {
        switch (stage)
        {
            case Attack:
                value += 1.f / (std::max (a, 0.0005f) * sr);
                if (value >= 1.f) { value = 1.f; stage = Decay; }
                break;
            case Decay:
                value = s + (value - s) * std::exp (-1.f / (std::max (d, 0.001f) * sr * 0.3f));
                if (std::abs (value - s) < 0.0005f) { value = s; stage = Sustain; }
                break;
            case Sustain: value = s; break;
            case Release:
                value *= std::exp (-1.f / (std::max (r, 0.001f) * sr * 0.3f));
                if (value < 0.00005f) { value = 0.f; stage = Idle; }
                break;
            default: break;
        }
        return value;
    }
};

//==============================================================================
inline float lfoShapeValue (int shape, float ph, float sh, const float* custom)
{
    switch (shape)
    {
        case 0: return std::sin (6.2831853f * ph);
        case 1: return 1.f - 4.f * std::abs (ph - 0.5f);
        case 2: return 2.f * ph - 1.f;
        case 3: return 1.f - 2.f * ph;
        case 4: return ph < 0.5f ? 1.f : -1.f;
        case 5: return sh;
        default: return custom[std::clamp ((int) (ph * kLfoSteps), 0, kLfoSteps - 1)];
    }
}

struct LfoState
{
    float phase = 0.f, sh = 0.f;
    uint32_t rng = 987654321u;

    void advance (float inc)
    {
        phase += inc;
        if (phase >= 1.f)
        {
            phase -= std::floor (phase);
            rng = rng * 1664525u + 1013904223u;
            sh = (float) (rng >> 8) * (2.f / 16777216.f) - 1.f;
        }
    }
};

//==============================================================================
struct OscSettings
{
    bool on = true;
    int wave = 1;
    float wtPos = 0.f, semis = 0.f, level = 0.8f;
    int uni = 1;
    float detune = 0.2f, blend = 0.5f, phase = 0.f;
    bool randPhase = true;
    bool phaseLock = false;   // reset to exactly 0 degrees on every note-on
    int warp = 0;
    float warpAmt = 0.f;
};

struct FilterSettings { int type = 1; float cutoff = 1000.f, reso = 0.3f, drive = 0.f, key = 0.3f, env = 0.4f; };
struct EnvSettings { float a = 0.003f, d = 0.5f, s = 0.8f, r = 0.2f; };
struct LfoSettings { int shape = 0; float rateHz = 1.f; bool retrig = false; float phase = 0.f, fade = 0.f; float custom[kLfoSteps] {}; };
struct ModSlot { int src = 0, dst = 0; float amt = 0.f; };

struct VoiceParams
{
    OscSettings osc[2];
    int subWave = 0, subOct = 0;
    float subLevel = 0.f;
    int noiseType = 0;
    float noiseLevel = 0.f, noisePitch = 1.f;
    FilterSettings flt[2];
    int routing = 0;
    EnvSettings env[kNumEnvs];
    LfoSettings lfo[kNumLfos];
    ModSlot mods[kNumModSlots];
    float accent = 0.5f, velSens = 0.5f;
    float warmth = 0.3f;     // pre- and post-filter analog saturation
    uint32_t lfoUsed = 0xff;  // bit per LFO that any mod slot reads (unused LFOs are skipped)
    int numActiveMods = kNumModSlots;
};

struct GlobalMod
{
    float lfo[kNumLfos] {};
    float modWheel = 0.f, aftertouch = 0.f, bend = 0.f;
};

//==============================================================================
// Stereo multimode filter: ladder, 12/24 dB SVF modes, notch and comb.
class VFilter
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& c : comb) c.assign ((size_t) (sr / 15.f) + 4, 0.f);
        reset();
    }

    void setRate (float sampleRate) { sr = sampleRate; reset(); }

    void reset()
    {
        for (auto& c : st) for (auto& v : c) v = 0.f;
        for (auto& c : comb) std::fill (c.begin(), c.end(), 0.f);
        cpos = 0;
    }

    void process (int type, float fc, float res, float drive, float& L, float& R)
    {
        if (type == 0) return;
        if (drive > 0.001f)
        {
            const float g = 1.f + drive * 6.f, n = 1.f / fastTanh (g);
            L = fastTanh (L * g) * n;
            R = fastTanh (R * g) * n;
        }
        if (type == 9)
        {
            const int size = (int) comb[0].size();
            const float d = std::clamp (sr / fc, 2.f, (float) size - 3.f);
            const float fb = res * 0.96f;
            float rp = (float) cpos - d;
            if (rp < 0.f) rp += (float) size;
            const int a = (int) rp, b = (a + 1) % size;
            const float fr = rp - (float) a;
            float* io[2] = { &L, &R };
            for (int c = 0; c < 2; ++c)
            {
                const float delayed = comb[c][(size_t) a] + (comb[c][(size_t) b] - comb[c][(size_t) a]) * fr;
                const float y = *io[c] + fb * delayed;
                comb[c][(size_t) cpos] = fastTanh (y);
                *io[c] = y * (1.f - fb * 0.6f);
            }
            cpos = (cpos + 1) % size;
            return;
        }
        const float g = std::tan (3.14159265f * fc / sr);
        L = stage (type, L, g, res, st[0]);
        R = stage (type, R, g, res, st[1]);
    }

private:
    static float svf (float x, float g, float k, float* ic, int mode)
    {
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic[1];
        const float v1 = a1 * ic[0] + a2 * v3;
        const float v2 = ic[1] + a2 * ic[0] + a3 * v3;
        ic[0] = 2.f * v1 - ic[0];
        ic[1] = 2.f * v2 - ic[1];
        switch (mode)
        {
            case 0: return v2;
            case 1: return v1 * k;
            case 2: return x - k * v1 - v2;
            default: return x - k * v1;
        }
    }

    static float ladder (float x, float g, float res, float* s)
    {
        const float G = g / (1.f + g);
        const float inv = 1.f / (1.f + g);
        const float k = res * 3.95f;
        const float S = (G * G * G * s[0] + G * G * s[1] + G * s[2] + s[3]) * inv;
        const float G4 = G * G * G * G;
        const float y4est = (G4 * x + S) / (1.f + k * G4);
        float u = fastTanh ((x - k * y4est) * 1.1f);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u * (1.f + k * 0.3f);
    }

    static float stage (int type, float x, float g, float res, float* s)
    {
        const float k = 2.f - 1.96f * res;
        switch (type)
        {
            case 1: return ladder (x, g, res, s);
            case 2: return svf (x, g, k, s, 0);
            case 3: return svf (svf (x, g, 1.85f, s, 0), g, k, s + 2, 0);
            case 4: return svf (x, g, k, s, 2);
            case 5: return svf (svf (x, g, 1.85f, s, 2), g, k, s + 2, 2);
            case 6: return svf (x, g, k, s, 1);
            case 7: return svf (svf (x, g, std::max (k, 0.5f), s, 1), g, k, s + 2, 1);
            default: return svf (x, g, k, s, 3);
        }
    }

    float sr = 44100.f;
    float st[2][4] {};
    std::vector<float> comb[2];
    int cpos = 0;
};

//==============================================================================
class SynthVoice
{
public:
    // Allocates for the highest rate the voice will ever run at
    void prepare (double maxSampleRate)
    {
        for (auto& f : flt) f.prepare ((float) maxSampleRate);
        setSampleRate (maxSampleRate);
    }

    void setSampleRate (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& e : env) e.sr = sr;
        for (auto& f : flt) f.setRate (sr);
        accentDecay = std::exp (-1.f / (0.15f * sr));
        reset();
    }

    void reset()
    {
        for (auto& e : env) e.reset();
        for (auto& f : flt) f.reset();
        note = -1;
        released = true;
    }

    // PANIC: silence instantly and clear every bit of state
    void hardReset()
    {
        reset();
        for (auto& o : phase) for (auto& ph : o) ph = 0.f;
        for (auto& l : lfo) l.phase = 0.f;
        subPhase = 0.f; noiseVal = 0.f; pk[0] = pk[1] = pk[2] = 0.f;
        prevCenter[0] = prevCenter[1] = 0.f;
        accentEnv = 0.f; gliding = false;
        for (auto& e : envOut) e = 0.f;
    }

    void seed (uint32_t s) { rng = s * 2654435761u + 1u; }

    // retrigger=false keeps envelopes running (legato); glideFrom=true slides pitch
    void start (int midiNote, float velocity, bool accented, bool retrigger, bool glideFrom, const VoiceParams& p)
    {
        if (! glideFrom || note < 0) currentNote = (float) midiNote;
        targetNote = (float) midiNote;
        gliding = glideFrom && note >= 0;
        note = midiNote;
        vel = velocity;
        accent = accented;
        released = false;
        age = 0;

        // Phase lock: every oscillator restarts at 0 degrees on every note-on, legato or not
        if (p.osc[0].phaseLock || p.osc[1].phaseLock)
        {
            for (int o = 0; o < 2; ++o)
                if (p.osc[o].phaseLock)
                    for (auto& ph : phase[o]) ph = 0.f;
            subPhase = 0.f;
            prevCenter[0] = prevCenter[1] = 0.f;
        }

        if (retrigger || ! env[0].isActive())
        {
            const bool wasSilent = ! env[0].isActive();
            for (int o = 0; o < 2; ++o)
                for (int u = 0; u < kMaxUnison; ++u)
                    if (p.osc[o].phaseLock) phase[o][u] = 0.f;
                    else if (p.osc[o].randPhase) { if (wasSilent || u > 0) phase[o][u] = rand01(); }
                    else phase[o][u] = p.osc[o].phase;
            for (auto& e : env) e.noteOn();
            for (int i = 0; i < kNumLfos; ++i)
                if (p.lfo[i].retrig) lfo[i].phase = p.lfo[i].phase;
            if (accented) accentEnv = 1.f;
            time = 0.f;
            randomVal = rand01() * 2.f - 1.f;
        }
    }

    void release() { for (auto& e : env) e.noteOff(); released = true; }
    bool isActive() const { return env[0].isActive(); }
    bool isReleased() const { return released; }
    int getNote() const { return note; }
    uint32_t getAge() const { return age; }

    float envOut[kNumEnvs] {};

    void process (const VoiceParams& p, const GlobalMod& g, float glideTime, float& outL, float& outR)
    {
        ++age;
        time += 1.f / sr;

        if (gliding && glideTime > 0.0005f)
            currentNote += (targetNote - currentNote) * (1.f - std::exp (-1.f / (glideTime * 0.3f * sr)));
        else
            currentNote = targetNote;

        for (int i = 0; i < kNumEnvs; ++i)
            envOut[i] = env[i].next (p.env[i].a, p.env[i].d, p.env[i].s, p.env[i].r);
        accentEnv *= accentDecay;

        // LFOs (per voice when retriggered, otherwise the shared global value)
        float lfoVal[kNumLfos];
        for (int i = 0; i < kNumLfos; ++i)
        {
            if ((p.lfoUsed & (1u << i)) == 0) { lfoVal[i] = 0.f; continue; }
            const auto& ls = p.lfo[i];
            float v;
            if (ls.retrig)
            {
                lfo[i].advance (ls.rateHz * std::exp2 (lfoRateMod[i] * 3.f) / sr);
                v = lfoShapeValue (ls.shape, lfo[i].phase, lfo[i].sh, ls.custom);
            }
            else v = g.lfo[i];
            const float fade = ls.fade > 0.001f ? std::min (1.f, time / ls.fade) : 1.f;
            lfoVal[i] = v * fade;
        }

        // Modulation matrix
        float src[S_Count] {};
        for (int i = 0; i < kNumLfos; ++i) src[S_Lfo1 + i] = lfoVal[i];
        src[S_Env2] = envOut[1]; src[S_Env3] = envOut[2]; src[S_Env4] = envOut[3];
        src[S_ModWheel] = g.modWheel; src[S_Velocity] = vel; src[S_Aftertouch] = g.aftertouch;
        src[S_Keytrack] = (currentNote - 60.f) / 24.f; src[S_Random] = randomVal;
        float mod[D_Count] {};
        for (int k = 0; k < p.numActiveMods; ++k)   // active slots are packed first by the processor
            mod[p.mods[k].dst] += src[p.mods[k].src] * p.mods[k].amt;
        for (int i = 0; i < kNumLfos; ++i) lfoRateMod[i] = mod[D_Lfo1Rate + i];

        // Oscillators
        float oL[2] {}, oR[2] {};
        float center[2] { prevCenter[0], prevCenter[1] };
        for (int o = 0; o < 2; ++o)
        {
            const auto& os = p.osc[o];
            if (! os.on) { center[o] = 0.f; continue; }
            const float level = std::clamp (os.level + mod[D_Osc1Level + o], 0.f, 1.f);
            const float semis = currentNote + os.semis + mod[D_Osc1Pitch + o] * 24.f + g.bend;
            const float baseHz = 440.f * std::exp2 ((semis - 69.f) / 12.f);
            const float wt = std::clamp (os.wtPos + mod[D_Osc1Wt + o], 0.f, 1.f);
            const float warpAmt = std::clamp (os.warpAmt + mod[D_Osc1Warp + o], 0.f, 1.f);
            const float srcSig = o == 0 ? prevCenter[1] : center[0];
            const int n = std::clamp (os.uni, 1, kMaxUnison);
            updateUnison (o, n, std::clamp (os.detune + mod[D_Detune], 0.f, 1.f), os.blend);
            const int c = n / 2;

            // Advance all unison phases in one tight (auto-vectorisable) loop
            const float baseDt = baseHz / sr;
            float* ph = phase[o];
            for (int u = 0; u < n; ++u)
            {
                const float x = ph[u] + std::min (0.45f, baseDt * uniRatio[o][u]);
                ph[u] = x >= 1.f ? x - 1.f : x;
            }
            const bool fm = os.warp == 1 && warpAmt > 0.f;
            const float fmOffset = fm ? warpAmt * srcSig * 1.5f : 0.f;
            const bool table = os.wave >= 4;
            Wavetables::Reader rd;
            if (table) rd = Wavetables::get().reader (os.wave - 4, wt, std::min (0.45f, baseDt * uniRatio[o][n - 1]));

            float sL = 0.f, sR = 0.f;
            for (int u = 0; u < n; ++u)
            {
                float phm = ph[u];
                if (fm)
                {
                    phm += fmOffset;
                    phm -= std::floor (phm);
                }
                const float s = table ? rd.read (phm) : oscSample (os.wave, wt, phm, std::min (0.45f, baseDt * uniRatio[o][u]));
                if (u == c) center[o] = s;
                sL += s * uniGainL[o][u];
                sR += s * uniGainR[o][u];
            }
            if (os.warp == 2 && warpAmt > 0.f)
            {
                const float m = 1.f - warpAmt + warpAmt * (0.5f + 0.5f * srcSig);
                sL *= m; sR *= m;
            }
            else if (os.warp == 3 && warpAmt > 0.f)
            {
                sL = sL * (1.f - warpAmt) + sL * srcSig * warpAmt;
                sR = sR * (1.f - warpAmt) + sR * srcSig * warpAmt;
            }
            oL[o] = sL * level;
            oR[o] = sR * level;
        }
        prevCenter[0] = center[0];
        prevCenter[1] = center[1];

        // Sub
        float subS = 0.f;
        const float subLevel = std::clamp (p.subLevel + mod[D_Sub], 0.f, 1.f);
        if (subLevel > 0.0001f)
        {
            const float hz = 440.f * std::exp2 ((currentNote + g.bend - (p.subOct == 0 ? 12.f : 24.f) - 69.f) / 12.f);
            const float dt = std::min (0.45f, hz / sr);
            subPhase += dt; if (subPhase >= 1.f) subPhase -= 1.f;
            const int w = p.subWave == 0 ? 0 : (p.subWave == 1 ? 3 : 2);
            subS = oscSample (w, 0.f, subPhase, dt) * subLevel * 0.8f;
        }

        // Noise
        float nz = 0.f;
        const float noiseLevel = std::clamp (p.noiseLevel + mod[D_Noise], 0.f, 1.f);
        if (noiseLevel > 0.0001f)
        {
            if (--noiseHold <= 0)
            {
                const float w = rand01() * 2.f - 1.f;
                if (p.noiseType == 1)
                {
                    pk[0] = 0.99765f * pk[0] + w * 0.0990460f;
                    pk[1] = 0.96300f * pk[1] + w * 0.2965164f;
                    pk[2] = 0.57000f * pk[2] + w * 1.0526913f;
                    noiseVal = (pk[0] + pk[1] + pk[2] + w * 0.1848f) * 0.22f;
                }
                else noiseVal = w;
                const float q = 1.f - p.noisePitch;
                noiseHold = 1 + (int) (q * q * 40.f);
            }
            nz = noiseVal * noiseLevel * 0.5f;
        }

        // Filters + routing
        const float accAmt = accent ? p.accent : 0.f;
        float fc[2], res[2];
        for (int f = 0; f < 2; ++f)
        {
            const auto& fs = p.flt[f];
            const float oct = envOut[1] * fs.env * 6.f * (1.f + accAmt * 0.5f)
                            + accentEnv * accAmt * 1.5f
                            + fs.key * (currentNote - 60.f) / 12.f
                            + mod[D_Cut1 + f] * 6.f;
            fc[f] = std::clamp (fs.cutoff * std::exp2 (oct), 20.f, std::min (20000.f, sr * 0.45f));
            res[f] = std::clamp (fs.reso + mod[D_Res1 + f] + accAmt * 0.08f, 0.f, 1.f);
        }

        float aL = oL[0] + subS, aR = oR[0] + subS;
        float bL = oL[1] + nz, bR = oR[1] + nz;
        if (p.warmth > 0.001f)
        {
            aL = warm (aL, p.warmth); aR = warm (aR, p.warmth);
            bL = warm (bL, p.warmth); bR = warm (bR, p.warmth);
        }
        float L, R;
        const int t1 = p.flt[0].type, t2 = p.flt[1].type;
        if (p.routing == 0)
        {
            L = aL + bL; R = aR + bR;
            flt[0].process (t1, fc[0], res[0], p.flt[0].drive, L, R);
            flt[1].process (t2, fc[1], res[1], p.flt[1].drive, L, R);
        }
        else if (p.routing == 1 && t2 != 0)
        {
            float l1 = aL + bL, r1 = aR + bR, l2 = l1, r2 = r1;
            flt[0].process (t1, fc[0], res[0], p.flt[0].drive, l1, r1);
            flt[1].process (t2, fc[1], res[1], p.flt[1].drive, l2, r2);
            L = (l1 + l2) * 0.6f; R = (r1 + r2) * 0.6f;
        }
        else if (p.routing == 1)
        {
            L = aL + bL; R = aR + bR;
            flt[0].process (t1, fc[0], res[0], p.flt[0].drive, L, R);
        }
        else
        {
            flt[0].process (t1, fc[0], res[0], p.flt[0].drive, aL, aR);
            flt[1].process (t2, fc[1], res[1], p.flt[1].drive, bL, bR);
            L = aL + bL; R = aR + bR;
        }

        if (p.warmth > 0.001f) { L = warm (L, p.warmth * 0.7f); R = warm (R, p.warmth * 0.7f); }

        const float velGain = 1.f - p.velSens * (1.f - vel);
        const float amp = envOut[0] * velGain * (1.f + accAmt * 0.4f) * std::clamp (1.f + mod[D_Amp], 0.f, 2.f);
        const float pan = std::clamp (mod[D_Pan], -1.f, 1.f);
        outL += L * amp * std::min (1.f, 1.f - pan);
        outR += R * amp * std::min (1.f, 1.f + pan);
    }

private:
    // Gentle analog-style saturation: transparent at low level, adds even + odd harmonics on peaks
    static float warm (float x, float w)
    {
        const float g = 1.f + w * 2.5f;
        return fastTanh (g * (x + 0.08f * w * x * x)) / g;
    }

    void updateUnison (int o, int n, float detune, float blend)
    {
        if (n == uniN[o] && std::abs (detune - uniDetune[o]) < 1.0e-4f && std::abs (blend - uniBlend[o]) < 1.0e-4f)
            return;
        uniN[o] = n; uniDetune[o] = detune; uniBlend[o] = blend;
        float sumSq = 0.f, w[kMaxUnison];
        for (int u = 0; u < n; ++u)
        {
            const float spread = n == 1 ? 0.f : 2.f * (float) u / (float) (n - 1) - 1.f;
            uniRatio[o][u] = std::exp2 (spread * detune * 0.5f / 12.f);
            const float a = std::abs (spread);
            w[u] = (1.f - 0.7f * blend) * (1.f - a) + (0.3f + 0.7f * blend) * a;
            if (n == 1) w[u] = 1.f;
            sumSq += w[u] * w[u];
            const float pan = spread * 0.9f;
            uniGainL[o][u] = 1.f - std::max (0.f, pan);
            uniGainR[o][u] = 1.f + std::min (0.f, pan);
        }
        const float norm = 1.f / std::sqrt (std::max (sumSq, 1.0e-6f));
        for (int u = 0; u < n; ++u)
        {
            uniGainL[o][u] *= w[u] * norm;
            uniGainR[o][u] *= w[u] * norm;
        }
    }

    float rand01()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) (rng >> 8) * (1.f / 16777216.f);
    }

    static float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    static float oscSample (int wave, float wt, float ph, float dt)
    {
        switch (wave)
        {
            case 0: return std::sin (6.2831853f * ph);
            case 1: return 2.f * ph - 1.f - polyBlep (ph, dt);
            case 2:
            {
                float ph2 = ph + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
                return (ph < 0.5f ? 1.f : -1.f) + polyBlep (ph, dt) - polyBlep (ph2, dt);
            }
            case 3: return 1.f - 4.f * std::abs (ph - 0.5f);
            default: return Wavetables::get().sample (wave - 4, wt, ph, dt);
        }
    }

    float sr = 44100.f;
    Adsr env[kNumEnvs];
    LfoState lfo[kNumLfos];
    float lfoRateMod[kNumLfos] {};
    VFilter flt[2];
    float phase[2][kMaxUnison] {};
    float uniRatio[2][kMaxUnison] {}, uniGainL[2][kMaxUnison] {}, uniGainR[2][kMaxUnison] {};
    int uniN[2] { -1, -1 };
    float uniDetune[2] { -1.f, -1.f }, uniBlend[2] { -1.f, -1.f };
    float prevCenter[2] {};
    float subPhase = 0.f, noiseVal = 0.f, pk[3] {};
    int noiseHold = 0;
    float currentNote = 60.f, targetNote = 60.f, vel = 1.f, accentEnv = 0.f, time = 0.f, randomVal = 0.f;
    float accentDecay = 0.99985f;
    int note = -1;
    bool accent = false, gliding = false, released = true;
    uint32_t age = 0, rng = 0x1234567u;
};
