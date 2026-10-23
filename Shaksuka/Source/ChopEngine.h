#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

namespace chop
{
// The first six effects replace where the audio is read from; the last five process on top.
enum Fx { Repeat, Stutter, Reverse, Scramble, Tape, Pitch, Crush, Filter, Gate, Ring, Pan, NumFx };
constexpr int kNumSources = 6;
constexpr int kMaxSteps   = 256;
constexpr int kNoSource   = -1;

enum Groove { Straight, Swing, Shuffle, Drunk, Push };

const char* fxName (int fx);
const char* fxShortName (int fx);
juce::StringArray gridNames();
double gridBeats (int gridIndex);

struct Params
{
    bool  freeMode  = false;
    int   grid      = 4;
    float freeMs    = 125.0f;
    int   groove    = Straight;
    float swing     = 0.58f;
    float humanize  = 0.0f;
    int   bars      = 1;
    float chaos     = 0.6f;
    int   stack     = 2;
    float evolve    = 0.2f;
    float lengthVar = 0.3f;
    int   seed      = 427;
    float weights[NumFx] {};
    float mix  = 1.0f;
    float gain = 1.0f;
};

struct Step
{
    bool active = false, continuation = false;
    int len = 1;
    int source = kNoSource;
    uint32_t procMask = 0; // bit n = effect (kNumSources + n)
    float r[12] {};        // random values that pick each effect's flavour
};

// Packed step for the UI: bit0 active, bit1 continuation, bits2-5 source+1, bits6-10 procs, bits11-15 len
uint32_t packStep (const Step& s);

class Engine
{
public:
    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>& buffer, const Params& p, juce::AudioPlayHead* playHead);

    // Read by the editor
    std::atomic<int> uiStep { 0 }, uiNumSteps { 16 }, uiSource { kNoSource };
    std::atomic<uint32_t> uiMask { 0 };
    std::array<std::atomic<uint32_t>, kMaxSteps> uiPattern {};
    std::atomic<double> uiBpm { 120.0 };

    // Output peaks across one pattern loop (newest pass), for the waveform display
    static constexpr int kDisplaySlots = 1024;
    std::array<std::atomic<float>, kDisplaySlots> uiPeaks {};
    std::atomic<float> uiPhase { 0.0f }; // playhead position in the pattern, 0..1

private:
    struct Voice
    {
        Step d;
        bool active = false;
        double t = 0, start = 0, step = 1, len = 1;
        double loopLen = 1, speed = 1, back = 0, stopLen = 1;
        float levels = 8; int crushFactor = 1, crushCount = 0; float held[2] {};
        int filterMode = 0; double fa = 1000, fb = 1000; float k = 1, a1 = 0, a2 = 0, a3 = 0;
        float ic1[2] {}, ic2[2] {};
        int gateDiv = 8; uint32_t gateBits = 0xffff; float gateEnv = 1;
        double ringInc = 0, ringPhase = 0; float ringMix = 1;
        double panCycles = 1;
    };

    Step makeStep (juce::Random& rng, const Params& p, int maxLen, int forcedLen) const;
    void generate (const Params& p);
    void publish (int idx);
    void onNewStep (juce::int64 stepIndex, const Params& p);
    void trigger (int patternIdx, const Params& p);
    void setupVoice (Voice& v, const Step& d) const;
    void renderVoice (Voice& v, float* out);
    float read (int ch, double pos) const;
    juce::int64 grooveIndex (double pos, double unit, const Params& p) const;
    static uint64_t hashParams (const Params& p, int numSteps);

    double sr = 44100.0;
    std::vector<float> hist[2];
    juce::int64 histMask = 0, histSize = 0, written = 0;

    Step pattern[kMaxSteps];
    int numSteps = 16;
    uint64_t lastHash = 0;
    juce::Random rng;

    Voice cur, old;
    int fadeRemaining = 0, fadeLen = 192;
    int remaining = 0;
    juce::int64 lastIdx = std::numeric_limits<juce::int64>::min();
    int pendingIdx = -1, pendingCountdown = 0;

    double ppq = 0, freePos = 0, stepSamples = 1;
    float gateCoef = 0.02f;
    int curPatternIdx = 0;
    double samplesInStep = 0;
    float peakAcc = 0; int peakSlot = -1;
};
} // namespace chop
