#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "Params.h"

namespace subich
{
// Plain parameter snapshot, filled once per block
struct Settings
{
    int o1wave = Saw, o1oct = -1, o1semi = 0; float o1fine = 0, o1shape = 0, o1level = 0.8f;
    int o2wave = Square, o2oct = -1, o2semi = 0; float o2detune = 7, o2shape = 0.5f, o2level = 0; bool o2sync = false, o2fm = false;
    int subwave = 0, suboct = 0; float sublevel = 0.6f, noise = 0;
    int f1type = LP24; float f1cut = 420, f1res = 0.35f, f1env = 0.6f, f1drive = 0.2f, f1key = 0.5f;
    int f2type = HP12; float f2cut = 35, f2res = 0.2f, f2env = 0, f2drive = 0, f2key = 0;
    bool parallel = false;
    float aa = 1, ad = 180, as = 0.8f, ar = 60, fa = 0, fd = 120, fs = 0.1f, fr = 40; // ms / 0..1
    int l1shape = LSine; bool l1sync = true; int l1rate = 3; float l1hz = 2, l1depth = 0; int l1dest = DCut1;
    int l2shape = LSH; bool l2sync = false; int l2rate = 4; float l2hz = 0.8f, l2depth = 0; int l2dest = DPitch;
    int mode = Mono; float glideMs = 0, velo = 0.5f; int bend = 2; float drive = 0.25f, volume = 0.7f;

    static Settings fromValues (const std::function<float (const char*)>& v);
};

class Engine
{
public:
    void prepare (double sampleRate);
    void reset();
    void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead);
    void panic(); // silence every voice at once and forget held notes

    // Wheels: the MOD wheel (CC1) fades LFO 1 in to full depth; PITCH bends by the BEND range
    std::atomic<float> uiBend { 0.0f }, uiMod { 0.0f };

    // Read by the editor
    static constexpr int kScopeSize = 2048;
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> uiCut1 { 420 }, uiCut2 { 35 }, uiOut { 0 }, uiFreq { 0 };
    std::atomic<int> uiNote { -1 };

private:
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float level = 0;
        void gate (bool on) { stage = on ? Attack : (stage == Idle ? Idle : Release); }
        float next (float a, float d, float s, float r, double sr);
    };

    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        // returns lp, bp, hp through refs
        inline void tick (float x, float g, float k, float& lp, float& bp, float& hp)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    struct Filter
    {
        Svf a, b;
        float g = 0.1f, k = 1.0f, kb = 1.4f;
        void reset() { a.reset(); b.reset(); }
        void setCoefs (float cutHz, float res, int type, double sr);
        float process (float x, int type, float drive);
    };

    struct Voice
    {
        int note = -1;
        bool gated = false;
        float velocity = 1.0f;
        double logFreq = 0, logTarget = 0; // log2(Hz)
        double ph1 = 0, ph2 = 0, phSub = 0;
        float noiseLp = 0;
        Env amp, filt;
        Filter f1, f2;
        int coefCounter = 0;
        juce::uint32 age = 0;
        bool isActive() const { return amp.stage != Env::Idle; }
    };

    void noteOn (int note, float velocity, const Settings& s);
    void noteOff (int note, const Settings& s);
    void startVoice (Voice& v, int note, float velocity, bool retrigger, bool glide, const Settings& s);
    float renderVoice (Voice& v, const Settings& s, float lfo1, float lfo2);
    float osc (int wave, double& phase, double dt, float shape, float pmod, bool& wrapped);
    static float lfoValue (int shape, double phase, float& held, double& lastPhase, juce::Random& rng);

    double sr = 44100.0;
    std::array<Voice, 8> voices;
    std::vector<int> held; // held notes, newest last (mono modes)
    juce::uint32 ageCounter = 0;
    double l1phase = 0, l2phase = 0, l1last = 0, l2last = 0;
    float l1held = 0, l2held = 0;
    double ppq = 0;
    float bendSemis = 0, bendRaw = 0, modWheel = 0;
    juce::Random rng;
    float outPeak = 0;
};
} // namespace subich
