#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "../../Common/SkaziCore.h"

namespace tahini
{
enum DelayMode { PingPong, Stereo, Tape };

const std::vector<skazi::ParamDef>& paramDefs();

struct Settings
{
    int mode = PingPong, time = 12; float feedback = 0.55f, loCut = 250, hiCut = 6500, wow = 0.25f, drive = 0.1f;
    bool freeze = false; float size = 0.65f, decay = 4.2f, predelayMs = 30, damp = 0.5f, width = 0.9f;
    float duck = 0.6f, duckRelMs = 200, delayMix = 0.4f, reverbMix = 0.3f; bool dry = true; float gain = 1.0f;
    static Settings fromValues (const std::function<float (const char*)>& v);
};

class Engine
{
public:
    void prepare (double sampleRate, int channels);
    void process (juce::AudioBuffer<float>& buffer, const Settings& s, juce::AudioPlayHead* playHead);

    std::atomic<float> uiWet { 0 }, uiOut { 0 }, uiDuck { 0 };
    std::atomic<double> uiBpm { 120.0 };

private:
    struct Line
    {
        std::vector<float> buf;
        int pos = 0;
        void init (int size) { buf.assign ((size_t) juce::jmax (4, size), 0.0f); pos = 0; }
        void write (float x) { buf[(size_t) pos] = x; pos = (pos + 1) % (int) buf.size(); }
        float read (double delaySamples) const
        {
            const int n = (int) buf.size();
            double rp = pos - delaySamples;
            while (rp < 0) rp += n;
            const int i0 = (int) rp;
            const float fr = (float) (rp - i0);
            const float a = buf[(size_t) (i0 % n)], b = buf[(size_t) ((i0 + 1) % n)];
            return a + (b - a) * fr;
        }
    };

    struct Allpass
    {
        Line line; int length = 1; float g = 0.6f;
        float process (float x)
        {
            const float d = line.read (length);
            const float y = -g * x + d;
            line.write (x + g * y);
            return y;
        }
    };

    double sr = 44100.0;
    // delay
    Line dl[2];
    double delaySmooth = 0, wowPh = 0, flutterPh = 0;
    float hp[2] {}, lpA[2] {}, lpB[2] {};
    // reverb
    static constexpr int N = 8;
    Line pre;
    Allpass diff[4];
    Line fdn[N];
    float fdnLp[N] {};
    double fdnLen[N] {};
    double modPh = 0;
    // duck
    float env = 0;
    float wetPeak = 0, outPeak = 0;
};
} // namespace tahini
