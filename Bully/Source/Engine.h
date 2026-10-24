#pragma once

// BULLY synth engine: 3 wavetable oscillators with dual warp, noise, 2 filters,
// 4 envelopes, 10 LFOs, 2 chaos generators, 4 macros, a 16-slot mod matrix and the FX rack.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Wavetables.h"
#include "Fx.h"
#include <array>
#include <atomic>

namespace bully
{
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float level = 0.0f, attackStep = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;

    void set (float aMs, float dMs, float s, float rMs, double sr)
    {
        attackStep = aMs < 0.05f ? 1.0f : (float) (1000.0 / (aMs * sr));
        decayCoef = (float) std::exp (-4.6 / (juce::jmax (0.5f, dMs) * 0.001 * sr));
        sustain = s;
        releaseCoef = (float) std::exp (-4.6 / (juce::jmax (0.5f, rMs) * 0.001 * sr));
    }
    void gate (bool on) { stage = on ? Attack : (stage == Idle ? Idle : Release); }
    void kill() { stage = Idle; level = 0.0f; }
    float next()
    {
        switch (stage)
        {
            case Attack:  level += attackStep; if (level >= 1.0f) { level = 1.0f; stage = Decay; } break;
            case Decay:   level = sustain + (level - sustain) * decayCoef; if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Sustain; } break;
            case Sustain: level = sustain; break;
            case Release: level *= releaseCoef; if (level < 1.0e-4f) { level = 0.0f; stage = Idle; } break;
            case Idle:    level = 0.0f; break;
        }
        return level;
    }
};

// Zero-delay-feedback state-variable filter
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() { ic1 = ic2 = 0.0f; }
    inline void tick (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// One channel of one of the voice's filters
struct FilterChannel
{
    Svf s[3];
    std::array<float, 4096> comb {};
    int combW = 0;
    float combLp = 0.0f, ap[4] {}, phFb = 0.0f, lad[4] {};
    double ringPh = 0.0;
    void reset() { for (auto& f : s) f.reset(); comb.fill (0.0f); combW = 0; combLp = phFb = 0.0f; for (auto& a : ap) a = 0.0f; for (auto& l : lad) l = 0.0f; ringPh = 0.0; }
};

// Per-slice filter settings (coefficients worked out once per slice)
struct FilterSetup
{
    bool on = false; int type = 0;
    float g[2] {}, k = 1.0f, k2 = 1.0f, drive = 0.0f, mix = 1.0f, var = 0.0f, res = 0.0f, ladG[2] {};
    float combDelay[2] {}, fbk = 0.0f, ringInc[2] {}, apA[2][4] {};
    float formG[2][3] {}, formK = 0.3f;
};

struct Voice
{
    bool active = false, released = false;
    int note = 60;
    float velocity = 1.0f, random = 0.0f;
    float pitch = 60.0f, pitchTarget = 60.0f;     // in semitones (glide moves pitch to target)
    juce::uint64 age = 0;
    Adsr env[kEnv];
    float killFade = 0.0f; bool killing = false;  // short fade when the voice is stolen
    std::array<std::array<double, 8>, kOsc> ph {};
    float oscLast[kOsc] {};
    float gainL[kOsc][8] {}, gainR[kOsc][8] {};   // last slice's unison gains (ramped)
    FilterChannel filt[2][2];
    // noise state
    float pink[7] {}, brown = 0.0f, noiseLp = 0.0f, digHold = 0.0f; double digPh = 0.0; juce::uint32 seed = 1;
    float lastAmp = 0.0f;
    // analog character
    float drift[kOsc] {}, driftTarget[kOsc] {}; // slow per-oscillator detune (semitones)
    float boostLp[2] {};
    float pan = 0.0f;                             // voice spread position, -1..1
};

class Engine
{
public:
    static constexpr int kMaxVoices = 24, kSlice = 16;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    // values: every parameter's real value, indexed like paramDefs()
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* values, const skazi::HostTime& ht);
    void allNotesOff() { panicFlag = true; }

    // Read by the editor
    std::atomic<float> uiOut { 0.0f }, uiBend { 0.0f }, uiMod { 0.0f };
    std::array<std::atomic<float>, NumDests> uiDestMod {}; // modulation (normalised) on each destination, newest voice
    std::array<std::atomic<float>, kLfo> uiLfo {}, uiLfoPhase {};
    std::array<std::atomic<float>, kEnv> uiEnv {};
    std::array<std::atomic<float>, kChaos> uiChaosX {}, uiChaosY {};
    std::atomic<int> uiVoices { 0 };

private:
    void noteOn (int note, float vel, const float* v);
    void noteOff (int note, const float* v);
    void startVoice (Voice& vc, int note, float vel, bool retrigger, const float* v);
    void updateGlobals (const float* v, int len, const skazi::HostTime& ht);
    void renderVoice (Voice& vc, const float* v, float* L, float* R, int len);
    void setupFilter (FilterSetup& fs, int f, const float* v, const float* mod, float noteHz);
    float runFilter (const FilterSetup& fs, FilterChannel& ch, int c, float x);
    void computeMod (const Voice* vc, float* mod);
    float noise (Voice& vc, int type, float tone);

    double sr = 44100.0;
    std::array<Voice, kMaxVoices> voices;
    juce::uint64 ageCounter = 0;
    std::vector<int> held;          // held notes, oldest first (mono / legato)
    float lastPitch = -1.0f;        // glide start for new voices
    bool panicFlag = false;

    // global sources
    float src[NumSrcs] {};
    float modWheel = 0.0f, bend = 0.0f, after = 0.0f;
    struct LfoState { double phase = 0.0; float out = 0.0f, smooth = 0.0f, shPrev = 0.0f, shNext = 0.0f; bool done = false; };
    std::array<LfoState, kLfo> lfo;
    struct ChaosState { double x = 0.1, y = 0.0, z = 0.0, acc = 0.0; float out = 0.0f, sm = 0.0f; };
    std::array<ChaosState, kChaos> chaos;
    juce::Random rng;
    bool noteStarted = false;       // a note started since the last slice (LFO retrigger)

    // matrix slots in use this block
    struct Slot { int src, dst; float amt; };
    std::vector<Slot> slots;

    FxRack fx;
    std::vector<float> bufL, bufR;
    float peak = 0.0f;
    const WaveBank* bank = nullptr;
};
} // namespace bully
