#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

namespace pump
{
enum Shape { Pump, HardCut, Smooth, Expo, Log, SawDown, Triangle, Gate, Bounce, Draw, NumShapes };
enum Trigger { Sync, Midi, Audio };

const char* shapeName (int shape);
juce::StringArray rateNames();
double rateBeats (int rateIndex);
juce::StringArray snapNames();
double snapBeats (int snapIndex); // 0 = off

struct Point { float x, y; };
constexpr int kMaxPoints = 32;
std::vector<Point> defaultPoints();

struct Params
{
    int   trigger  = Sync;
    int   rate     = 4;      // 1/4
    float phase    = 0.0f;   // degrees
    float swing    = 0.5f;   // 0.5..0.75
    float threshDb = -24.0f;
    int   shape    = Pump;
    float depth    = 0.85f;
    float curve    = 0.5f;   // 0..1, 0.5 = shape as drawn
    float attackMs = 0.0f;
    float release  = 1.0f;   // part of the cycle the curve uses, 0.1..1
    float smoothMs = 2.0f;
    float preGain  = 1.0f;   // linear
    bool  autoGain = false;
    float mix      = 1.0f;
    float outGain  = 1.0f;
};

// Gain (0..1) at cycle position x (0..1), tMs after the cycle started
float gainAt (const Params& p, float x, float tMs, const Point* pts, int numPts);

class Engine
{
public:
    void prepare (double sampleRate);
    // sidechain may be null; noteOns are sample offsets of MIDI note-ons in this block
    void process (juce::AudioBuffer<float>& main, const juce::AudioBuffer<float>* sidechain,
                  const std::vector<int>& noteOns, const Params& p, juce::AudioPlayHead* playHead);

    void setPoints (const std::vector<Point>& pts);
    std::vector<Point> getPoints() const;

    // Read by the editor
    static constexpr int kCyclesShown = 4, kDisplaySlots = 512;
    std::array<std::atomic<float>, kDisplaySlots> uiPeaks {};
    std::atomic<float> uiPos { 0 }, uiGain { 1 }, uiIn { 0 }, uiOut { 0 }, uiAutoDb { 0 };
    std::atomic<double> uiBpm { 120.0 };

private:
    std::array<std::atomic<float>, kMaxPoints * 2> pointData {};
    std::atomic<int> numPoints { 0 };

    double sr = 44100.0;
    double ppq = 0;
    // MIDI / audio trigger state
    juce::int64 trigCycle = 0;
    double sinceTrig = 1.0e12; // samples since the last trigger
    float env = 0; bool armed = true;
    // smoothing
    float gainSm = 1.0f, ms = 0.0f, autoGain = 1.0f, inPk = 0, outPk = 0;
    float peakAcc = 0; int peakSlot = -1;
};
} // namespace pump
