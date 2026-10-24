#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "../../Common/SkaziCore.h"

namespace bamba
{
enum Type { Uplifter, Downlifter, NoiseSweep, Impact, SnareRoll, ReverseCrash, Laser, Siren, SubDrop, RandomFx, NumTypes };
enum Trigger { Sync, Midi };

const std::vector<skazi::ParamDef>& paramDefs();
int barsFor (int barsChoice); // 1, 2, 4, 8, 16

struct Settings
{
    int type = Uplifter, trigger = Sync, bars = 8; float curve = 0.6f; int offset = 0;
    float noise = 0.7f, color = 0.5f, tone = 0.4f, pitchFrom = -12, pitchTo = 24;
    float sweep = 0.75f, reso = 0.5f, roll = 0.0f, width = 0.7f, phaser = 0.3f;
    float reverb = 0.6f, delay = 0.4f, gain = 1.0f;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// The shape of an effect at progress p (0..1) through its length
struct Shape { float amp, pitch, filter; };
Shape shapeAt (int type, float p, float curve, float lengthSeconds);

class Engine
{
public:
    void prepare (double sampleRate);
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead);

    // Read by the editor
    std::atomic<float> uiProgress { -1.0f }, uiOut { 0.0f };
    std::atomic<int> uiType { 0 };

private:
    void start (int note, const Settings& s, juce::int64 seed);
    void sample (const Settings& s, float& l, float& r);

    double sr = 44100.0, ppq = 0, bpm = 120.0;

    // event
    bool active = false, releasing = false;
    int heldNote = -1;
    float fade = 1.0f;
    double t = 0, len = 1;
    int type = Uplifter;
    float noteSemis = 0;

    // random-FX overrides
    float rPitchFrom = 0, rPitchTo = 0, rSweep = 0, rRoll = 0;
    // generators
    juce::Random rng;
    double ph[2] {}, zapPh = 0, rollPh = 0, sirenPh = 0, subPh = 0;
    float noiseLp[2] {}, ic1[2] {}, ic2[2] {};
    float ap[2][4] {}; double phaserPh = 0;
    std::vector<float> dl[2]; int dlPos = 0;
    juce::Reverb reverb;
    float peak = 0;
};
} // namespace bamba
