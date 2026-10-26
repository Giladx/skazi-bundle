#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <limits>
#include "../../Common/SkaziCore.h"

namespace salat
{
enum DriveMode { Tube, Fuzz, Fold, Rectify };
enum FilterType { LowPass, BandPass, HighPass, Notch };
enum LfoShape { Sine, Triangle, Saw, Square, SampleHold };
enum ModType { Flanger, Chorus };
enum ChopPos { First, Middle, Last };
enum Event { None, Stutter, Reverse, Gate, Tape, Hold, NumEvents };

const std::vector<skazi::ParamDef>& paramDefs();
double chopStepBeats (int index);
int chopRepeats (int index);
double lfoBeats (int index);
double delayBeats (int index);
int riseBars (int index);

struct Settings
{
    float inGain = 1, mix = 1, outGain = 1;
    bool chopOn = true, hold = false; int chopPos = Middle, chopStep = 2, repeats = 4; float chance = 0.35f;
    std::array<float, 4> weights { 1.0f, 0.5f, 0.4f, 0.25f }; // stutter, reverse, gate, tape
    bool driveOn = true; int driveMode = Tube; float driveDb = 9;
    bool crushOn = false; float bits = 8, downsample = 4;
    bool filterOn = true; int filterType = LowPass; float cutoff = 8000, reso = 0.3f; int lfoRate = 2, lfoShape = Sine; float lfoDepth = 0;
    bool ringOn = false; float ringFreq = 440, ringMix = 0.5f;
    bool modOn = false; int modType = Flanger; float modRate = 0.3f, modDepth = 0.6f, modFb = 0.5f, modMix = 0.5f;
    bool delayOn = false; int delayTime = 4; float delayFb = 0.4f, delayTone = 0.5f, delayMix = 0.3f;
    bool reverbOn = false; float reverbSize = 0.6f, reverbDamp = 0.5f, reverbMix = 0.25f;
    bool riseOn = false; int riseLen = 2; float riseSemis = 12;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

float shape (int mode, float x);

// Magnitude of the filter at freq (for the display): the analog prototype of the SVF below
float filterMagnitude (int type, float cutoff, float reso, float freq);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    int latency() const { return latencySamples; }
    void process (juce::AudioBuffer<float>& buffer, const Settings& s, const skazi::HostTime& host);

    // MAKE A SALAT: glitch the next few steps whatever CHANCE says (any thread)
    void burst (int steps) { burstSteps.store (steps); }

    // Read by the editor
    std::atomic<float> uiIn { 0 }, uiOut { 0 }, uiReduction { 1 }, uiCutoff { 8000 }, uiStepPhase { 0 };
    std::atomic<float> uiRiseProgress { 0 }, uiRiseSemis { 0 }; // progress is -1 while waiting for the bar line
    static constexpr int kHistory = 16, kScope = 256;
    std::array<std::atomic<int>, kHistory> history {};   // event per step, ring buffer
    std::atomic<int> historyCount { 0 };
    std::array<std::atomic<float>, kScope> scope {};     // output peak, about 2 s
    std::array<std::atomic<int>, kScope> scopeEvent {};  // what the chopper was doing at that point
    std::atomic<int> scopeWrite { 0 };

private:
    struct Voice
    {
        int type = None, start = 0, t = 0, len = 0, slice = 0, fade = 64;
        bool active = false, hold = false;
        double tapePos = 0;
        float blend = 0, gate = 1;
    };

    void processChop (int i, float& l, float& r, const Settings& s);
    void processRise (double beat, float& l, float& r, const Settings& s);
    void startEvent (int type, const Settings& s, bool hold);
    float readChop (int c, double idx) const;

    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int latencySamples = 0;
    juce::AudioBuffer<float> work, dry, scratch;
    std::vector<float> dryDelay[2]; int dryPos = 0;

    // per-sample beat position for the block being processed
    double blockBeat = 0, beatsPerSample = 0, freeBeat = 0, stepSamples = 1;
    juce::int64 lastStep = std::numeric_limits<juce::int64>::min(), lastLfoStep = 0;
    bool playingBefore = false;

    // chopper
    std::vector<float> chopBuf[2]; int chopSize = 1, chopWrite = 0;
    std::array<Voice, 2> voices;
    std::atomic<int> burstSteps { 0 };
    juce::Random rng { 0x5a1a7 };
    int currentEvent = None;

    // smoothed controls (snapped to the settings on the first block, so a loaded sound starts as itself)
    bool primed = false;
    float inSm = 1, mixSm = 1, outSm = 1, driveSm = 1, driveOnSm = 1, crushOnSm = 0, filterOnSm = 1;
    float cutLogSm = 0, resoSm = 0.3f, ringOnSm = 0, ringMixSm = 0.5f, ringFreqSm = 440, modOnSm = 0, modMixSm = 0.5f;
    float delayOnSm = 0, delayMixSm = 0.3f, delaySamplesSm = 0, reverbOnSm = 0, reverbMixSm = 0.25f;

    // effects state
    float crushHold[2] {}; int crushCount = 0;
    float ic1[2] {}, ic2[2] {}, lfoHeld = 0;
    double ringPhase = 0, modPhase = 0;
    std::vector<float> modBuf[2]; int modSize = 1, modWrite = 0; float modFbState[2] {};
    std::vector<float> delayBuf[2]; int delaySize = 1, delayWrite = 0; float delayLp[2] {};
    juce::Reverb reverb;
    float limiterGain = 1;

    // RISE: a pitch shifter swept up or down over 2-16 bars. One read point drifts through a
    // delay line; when it runs out of room it jumps to the best-matching spot and crossfades (SOLA).
    std::vector<float> riseBuf[2]; int riseSize = 1, riseWrite = 0;
    double riseDelay = 0, riseFadeDelay = 0, riseStartBeat = 0, riseLastBeat = 0, beatsPerBar = 4;
    int riseFadePos = 0, riseFadeLen = 256;
    bool riseWasOn = false, riseFading = false;
    float riseSemisSm = 0;
    float riseRead (int c, double delay) const;
    double riseBestJump (double from, double lo, double hi) const;

    float inPk = 0, outPk = 0, scopePk = 0; int scopeCount = 0, scopeEventNow = None;
};
} // namespace salat
