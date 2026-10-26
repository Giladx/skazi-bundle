#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "../../Common/SkaziCore.h"

namespace quazi
{
enum Mode { Soft, Hard, Fuzz, Tube, NumModes };
enum Band { Low, LowMid, HighMid, High, NumBands };

const std::vector<skazi::ParamDef>& paramDefs();

// Parameter IDs of one band ("lo", "lm", "hm", "hi" + On / Mode / Drive / Level)
juce::String bandId (int band, const char* what);

struct Settings
{
    float inGain = 1, mix = 1, outGain = 1;
    std::array<float, 3> xover { 150.0f, 1200.0f, 5000.0f }; // Hz, always rising
    std::array<bool, NumBands> on { true, true, true, true };
    std::array<int, NumBands> mode { Soft, Soft, Soft, Soft };
    std::array<float, NumBands> driveDb { 12, 12, 12, 12 }, level { 1, 1, 1, 1 };
    float width = 1; bool monoBass = true;
    float lowDb = 0, lowFreq = 100, midDb = 0, midFreq = 1000, midQ = 0.7f, highDb = 0, highFreq = 8000;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// One band's transfer curve: x is the driven signal, the result stays within about +-1
float shape (int mode, float x);

// Gain the post EQ applies at freq (for the display)
float eqMagnitude (const Settings& s, double sampleRate, double freq);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    int latency() const { return latencySamples; }
    void process (juce::AudioBuffer<float>& buffer, const Settings& s);

    // Read by the editor
    std::atomic<float> uiIn { 0 }, uiOut { 0 };
    std::array<std::atomic<float>, NumBands> uiBand {}; // peak of each band after its fuzz
    std::atomic<float> uiWidth { 0 };                    // side / mid of the output, smoothed

private:
    void updateFilters (const Settings& s, bool snap);

    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int latencySamples = 0;

    // Crossover at the oversampled rate: three LR4 splits; the lower bands go through the
    // allpasses of the splits above them so all four line up and sum back flat. MIX blends each band
    // with its own clean copy, so the dry sound is phase-matched to the fuzz and never combs.
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> split;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> allpass; // low band at split 2 and 3, low-mid at split 3

    std::array<juce::dsp::IIR::Filter<float>, 2> eqLow, eqMid, eqHigh;
    float dcX[2] {}, dcY[2] {};

    // smoothed controls (snapped on the first block, so a loaded sound starts as itself)
    bool primed = false;
    float inSm = 1, mixSm = 1, outSm = 1, widthSm = 1;
    std::array<float, 3> xoverSm {};
    std::array<float, NumBands> driveSm {}, levelSm {}, onSm {};
    float lowDbSm = 0, lowFreqSm = 100, midDbSm = 0, midFreqSm = 1000, midQSm = 0.7f, highDbSm = 0, highFreqSm = 8000;

    float inPk = 0, outPk = 0, sidePk = 0, midPk = 0;
    std::array<float, NumBands> bandPk {};
};
} // namespace quazi
