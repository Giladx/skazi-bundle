#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "../../Common/SkaziCore.h"

namespace tahini
{
// The first three are the 1.0 modes; the rest were appended so saved projects keep their mode
enum DelayMode { PingPong, Stereo, Tape, Digital, Bbd, MultiTap, Dual };
enum DriveType { DriveTape, DriveTube, DriveFuzz, DriveCrush };

const std::vector<skazi::ParamDef>& paramDefs();

struct Settings
{
    int mode = PingPong, time = 12; float feedback = 0.55f, loCut = 250, hiCut = 6500, wow = 0.25f, drive = 0.1f;
    bool freeze = false; float size = 0.65f, decay = 4.2f, predelayMs = 30, damp = 0.5f, width = 0.9f;
    float duck = 0.6f, duckRelMs = 200, delayMix = 0.4f, reverbMix = 0.3f; bool dry = true; float gain = 1.0f;
    float pitch = 0, reverse = 0, tapeStop = 0, age = 0; // tape FX
    float loRes = 0, hiRes = 0; int driveType = DriveTape; float pitchMix = 1.0f; bool pitchSnap = true; // 1.1 echo FX
    static Settings fromValues (const std::function<float (const char*)>& v);
};

// The echo filters as the engine runs them (TAPE, BBD and AGE darken the high cut), for the engine and the curve on screen
float echoLowCut (const Settings& s);
float echoHighCut (const Settings& s);
double resoQ (float reso);
double echoFilterGain (const Settings& s, double hz);

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

    // Zero-delay state-variable filter (2 poles): low, band and high outputs at once
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        void tick (float x, float g, float r2, float& lp, float& bp, float& hp)
        {
            hp = (x - (r2 + g) * ic1 - ic2) / (1.0f + g * (r2 + g));
            bp = g * hp + ic1; ic1 = g * hp + bp;
            lp = g * bp + ic2; ic2 = g * bp + lp;
        }
    };

    struct EchoCoeffs { float gLo, r2Lo, kLo, gHi, r2Hi, drive, dcA; int type, hold; float step; bool bbd; float aBbd; };

    // What happens to an echo on every pass: low cut, high cut, then drive. Never adds gain at low levels.
    struct EchoFx
    {
        Svf lo, hi;
        float bbdLp = 0, dcIn = 0, dcOut = 0, held = 0; int holdCount = 0;
        void reset() { lo.reset(); hi.reset(); bbdLp = dcIn = dcOut = held = 0; holdCount = 0; }
        float process (float x, const EchoCoeffs& c);
    };

    // Pitch shifter for the feedback path: two read heads sweeping a short buffer, crossfaded
    struct Shifter
    {
        Line line; double phase = 0; int window = 2048;
        float process (float x, double ratio)
        {
            line.write (x);
            if (std::abs (ratio - 1.0) < 1.0e-4) return x;
            phase += (1.0 - ratio) / window;
            phase -= std::floor (phase);
            const double p2 = phase + 0.5 - std::floor (phase + 0.5);
            const float w1 = (float) std::sin (juce::MathConstants<double>::pi * phase);
            const float w2 = (float) std::sin (juce::MathConstants<double>::pi * p2);
            return line.read (1.0 + phase * window) * w1 * w1 + line.read (1.0 + p2 * window) * w2 * w2;
        }
    };

    double sr = 44100.0;
    // delay
    Line dl[2];
    Shifter shift[2];
    EchoFx echoFx[2], tapFx[2], loopFx[2]; // heard, multi-tap heads, softer copy for the loop
    double delaySmoothR = 0, pitchSmooth = 0, bbdPh = 0;
    float loSmooth = 0, hiSmooth = 0;
    double revPhase = 0;                      // reverse heads: position inside the current echo
    float stopEnv = 0; bool stopping = false; // tape stop
    double stopCount = 0, stopHold = 0;
    float ageLp[2] {}, dropEnv = 1.0f;
    juce::Random rng;
    double delaySmooth = 0, wowPh = 0, flutterPh = 0;
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
