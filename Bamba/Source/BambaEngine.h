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
enum SampleFit { FitOriginal, FitStretch, FitVarispeed };
enum SampleAlign { AlignStart, AlignEnd };
// ROLL timing: FREE speeds up smoothly (1.0 behaviour), BUILD steps 1/4 -> 1/8 -> 1/16 -> 1/32 on the beat,
// the rest are a fixed note value locked to the host's beat grid
enum RollRate { RollFree, RollBuild, Roll2, Roll4, Roll8, Roll16, Roll32, Roll64, Roll128 };
double rollHitsPerBeat (int rate, float progress); // 0 for FREE

const std::vector<skazi::ParamDef>& paramDefs();
int barsFor (int barsChoice); // 1, 2, 4, 8, 16

struct Settings
{
    int type = Uplifter, trigger = Sync, bars = 8; float curve = 0.6f; int offset = 0;
    float noise = 0.7f, color = 0.5f, tone = 0.4f, pitchFrom = -12, pitchTo = 24;
    float sweep = 0.75f, reso = 0.5f, roll = 0.0f, width = 0.7f, phaser = 0.3f;
    float reverb = 0.6f, delay = 0.4f, gain = 1.0f;
    // sample layer
    float synth = 1.0f, sampleLevel = 0.8f, sampleTune = 0, sampleShape = 0.3f;
    int sampleFit = FitStretch, sampleAlign = AlignEnd; bool sampleReverse = false;
    // roll quantize and mixer
    int rollRate = RollFree; float rollDecay = 0.5f, synthFx = 1.0f, sampleFx = 1.0f;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// A loaded riser sample, peak-normalised, at its own sample rate
struct SampleData
{
    std::vector<float> l, r;
    double rate = 44100.0;
    std::vector<float> overview; // 512 peaks across the file, for drawing
    int frames() const { return (int) l.size(); }
    double seconds() const { return frames() / rate; }
};
using SamplePtr = std::shared_ptr<const SampleData>;
SamplePtr makeSample (const juce::AudioBuffer<float>& audio, double rate);

// Where the sample sits inside an effect of lenSeconds: it starts at `start` and plays for `length` seconds
struct SamplePlacement { double start, length; };
SamplePlacement placeSample (const Settings& s, double sampleSeconds, double lenSeconds);

// The shape of an effect at progress p (0..1) through its length
struct Shape { float amp, pitch, filter; };
Shape shapeAt (int type, float p, float curve, float lengthSeconds);

class Engine
{
public:
    void prepare (double sampleRate);
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s, juce::AudioPlayHead* playHead);
    /** The sample layer for the next blocks (may be null). */
    void setSample (SamplePtr p) { smp = std::move (p); }

    // Read by the editor
    std::atomic<float> uiProgress { -1.0f }, uiOut { 0.0f };
    std::atomic<int> uiType { 0 };

private:
    void start (int note, const Settings& s, juce::int64 seed);
    void sample (const Settings& s, float& l, float& r);
    void startSample (const Settings& s);
    void samplePlay (const Settings& s, float& l, float& r);

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
    std::vector<float> dryL, dryR; // the part of the mix that skips the delay and reverb (FX sends below 100%)
    double evT = 0;                // samples since the note started
    juce::Reverb reverb;
    float peak = 0;

    // sample layer voice
    SamplePtr smp, voiceSmp;
    bool sActive = false;
    double sT = 0, sStep = 1, sOffset = 0, sDelay = 0, sLen = 0;
    int sFit = FitStretch;
    struct Grain { double pos = 0; int age = -1; } grains[2];
    float sic1[2] {}, sic2[2] {};
};
} // namespace bamba
