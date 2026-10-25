#pragma once

// Spatial FX for the mono voice: stereo chorus/flanger, a bucket-brigade style
// ping-pong delay, and a width control that keeps the low end mono.
// (The room reverb uses juce::Reverb in the processor.)

#include "Oscillator.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace s303
{
/**
    Shared FX mix law. 0..50%: dry stays at full level while the effect fades in.
    50..100%: the effect stays full while the dry fades out, so 100% is fully wet.
*/
inline void mixGains (float mix, float& dry, float& wet)
{
    mix = std::clamp (mix, 0.0f, 1.0f);
    dry = mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix);
    wet = mix >= 0.5f ? 1.0f : 2.0f * mix;
}

class DelayBuffer
{
public:
    void resize (int samples)
    {
        data.assign ((size_t) samples, 0.0f);
        pos = 0;
    }

    void clear() { std::fill (data.begin(), data.end(), 0.0f); }

    void write (float x)
    {
        data[(size_t) pos] = x;
        if (++pos >= (int) data.size())
            pos = 0;
    }

    /** Linear-interpolated read, delay measured from the most recent write. */
    float read (double delaySamples) const
    {
        const int size = (int) data.size();
        delaySamples = std::clamp (delaySamples, 1.0, (double) size - 2.0);
        double rp = pos - 1 - delaySamples;
        while (rp < 0.0)
            rp += size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float t = (float) (rp - i0);
        return data[(size_t) i0] + (data[(size_t) i1] - data[(size_t) i0]) * t;
    }

private:
    std::vector<float> data;
    int pos = 0;
};

/** Stereo chorus (mode 1) or flanger (mode 2). Mode 0 bypasses. */
class ModFX
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        buf.resize ((int) (sr * 0.05) + 4);
        phase = 0.0;
        fbState = 0.0f;
    }

    void reset()
    {
        buf.clear();
        fbState = 0.0f;
    }

    void process (float in, int mode, float rateHz, float depth, float mix, float& outL, float& outR)
    {
        if (mode == 0 || mix <= 0.0001f)
        {
            buf.write (in);
            outL = outR = in;
            return;
        }

        const bool flanger = mode == 2;
        const double centre = (flanger ? 0.0025 : 0.008) * sr;
        const double swing = (flanger ? 0.0020 : 0.005) * sr * depth;

        phase += rateHz / sr;
        if (phase >= 1.0)
            phase -= 1.0;

        // L and R LFOs run in quadrature for chorus, inverted for flanger.
        const double offset = flanger ? 0.5 : 0.25;
        const double dL = centre + swing * std::sin (kTwoPi * phase);
        const double dR = centre + swing * std::sin (kTwoPi * (phase + offset));

        const float fb = flanger ? 0.72f * depth : 0.0f;
        buf.write (in + fbState * fb);

        const float tapL = buf.read (dL);
        const float tapR = buf.read (dR);
        fbState = std::tanh (tapL);

        float dry, wet;
        mixGains (mix, dry, wet);
        outL = in * dry + tapL * wet;
        outR = in * dry + tapR * wet;
    }

private:
    double sr = 44100.0, phase = 0.0;
    DelayBuffer buf;
    float fbState = 0.0f;
};

/**
    Bucket-brigade flavoured ping-pong delay: darkened, saturating repeats, a little
    clock wobble, and delay-time changes that glide (pitch-bend) instead of jumping.
*/
class BBDDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufL.resize ((int) (sr * 2.5) + 4);
        bufR.resize ((int) (sr * 2.5) + 4);
        reset();
    }

    void reset()
    {
        bufL.clear();
        bufR.clear();
        lpL = lpR = hpL = hpR = 0.0;
        time = -1.0;
        wow = 0.0;
    }

    void process (float inL, float inR, double timeSec, float feedback, float toneHz, float mix,
                  float& outL, float& outR)
    {
        if (time < 0.0)
            time = timeSec;
        time += (timeSec - time) * (1.0 - std::exp (-1.0 / (0.08 * sr)));

        wow += 0.45 / sr;
        if (wow >= 1.0)
            wow -= 1.0;
        const double d = (time + 0.0003 * std::sin (kTwoPi * wow)) * sr;

        const float wetL = bufL.read (d);
        const float wetR = bufR.read (d);

        // Repeats lose top end and the lowest bass on each pass, like a BBD + the
        // usual low cut you'd want on a club bass delay.
        const double lpC = 1.0 - std::exp (-kTwoPi * std::min ((double) toneHz, sr * 0.45) / sr);
        const double hpC = 1.0 - std::exp (-kTwoPi * 140.0 / sr);
        lpL += (wetL - lpL) * lpC;
        lpR += (wetR - lpR) * lpC;
        hpL += (lpL - hpL) * hpC;
        hpR += (lpR - hpR) * hpC;
        const float fbL = std::tanh ((float) (lpL - hpL) * feedback * 1.1f);
        const float fbR = std::tanh ((float) (lpR - hpR) * feedback * 1.1f);

        // Ping-pong: input enters left, each side feeds the other.
        bufL.write (0.5f * (inL + inR) + fbR);
        bufR.write (fbL);

        float dry, wet;
        mixGains (mix, dry, wet);
        outL = inL * dry + wetL * wet;
        outR = inR * dry + wetR * wet;
    }

private:
    double sr = 44100.0, time = -1.0, wow = 0.0;
    DelayBuffer bufL, bufR;
    double lpL = 0.0, lpR = 0.0, hpL = 0.0, hpR = 0.0;
};

/** Mid/side width that leaves everything below ~150 Hz mono. */
class StereoWidth
{
public:
    void prepare (double sampleRate)
    {
        coeff = 1.0 - std::exp (-kTwoPi * 150.0 / sampleRate);
        lowSide = 0.0;
    }

    void process (float& l, float& r, float width)
    {
        const double mid = 0.5 * (l + r);
        const double side = 0.5 * (l - r);
        lowSide += (side - lowSide) * coeff;
        const double s = (side - lowSide) * width;
        l = (float) (mid + s);
        r = (float) (mid - s);
    }

private:
    double coeff = 0.0, lowSide = 0.0;
};
} // namespace s303
