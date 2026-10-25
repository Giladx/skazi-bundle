#pragma once

// JUCE-free kick voice with two layers:
//   SUB   - a sine with a two-stage pitch envelope, the way real kicks move:
//             SNAP:  Start (kHz) -> Punch in a few ms; heard as attack, not as a pitch
//             SWEEP: Punch -> End (the root note) over tens of ms; this is the "thump"
//           and an amp envelope that holds at full level, then releases to silence
//           over Decay ms on a curve, so the tail ends cleanly instead of ringing on
//   SAMPLE - a loaded one-shot (any kick or click WAV, e.g. from Splice; a 2 ms noise burst when
//            none is loaded), delayed by Offset, trimmed by Length,
//            repitched by Tune and shaped by a 12 dB/oct high-pass and low-pass
// The sum goes through optional tanh drive (clean at 0) and a LOW / MID / HIGH EQ.
// The editor runs its own copy offline to draw the waveform.

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace kick
{
struct Params
{
    float startHz    = 3000.0f; // pitch at the very first sample
    float punchHz    = 180.0f;  // where the snap lands and the body sweep starts
    float endHz      = 50.0f;   // root note the body settles on
    float snapMs     = 3.0f;    // snap time constant (Start -> Punch)
    float pitchMs    = 35.0f;   // sweep time constant (Punch -> End)
    float holdMs     = 40.0f;   // full level before the release starts
    float ampMs      = 300.0f;  // release length, to silence
    float click      = 0.3f;    // 0..1 click layer level
    float offsetMs   = 0.0f;    // click layer delay, for lining its phase up with the sub
    float drive      = 0.0f;    // 0..1 saturation; 0 is perfectly clean
    float levelDb    = -3.0f;
    float clickTune  = 0.0f;    // click layer pitch, semitones (playback speed)
    float clickHpfHz = 20.0f;   // click layer high-pass; 20 Hz is off
    float clickLpfHz = 20000.0f; // click layer low-pass; 20 kHz is off
    float subLevel   = 1.0f;    // 0..1 synth sub level, so a sample kick can carry the sound
    float sampleLenMs = 2000.0f; // sample layer length: fades out over its last 30%; 2000 is off
    float eqLowDb    = 0.0f;    // output EQ: low shelf at 100 Hz
    float eqMidDb    = 0.0f;    //            bell at eqMidHz
    float eqMidHz    = 500.0f;
    float eqHighDb   = 0.0f;    //            high shelf at 5 kHz
};

constexpr float eqLowHz = 100.0f, eqHighHz = 5000.0f;

/** Drive: tanh(k x) / tanh(k), which tends to plain x as k -> 0, so DRIVE 0 is exactly clean.
    k tops out at 4 (gentle warmth to firm saturation), not 10, which crushed a kick. */
inline float driveSample (float x, float drive)
{
    const float k = drive * 4.0f;
    return k < 1.0e-3f ? x : std::tanh (x * k) / std::tanh (k);
}

/** Sample layer gain at ms after the layer starts: LENGTH trims the tail with a squared fade. */
inline float sampleGainAt (const Params& p, float ms)
{
    if (p.sampleLenMs >= 1999.0f)
        return 1.0f;
    const float fadeStart = p.sampleLenMs * 0.7f;
    if (ms <= fadeStart)
        return 1.0f;
    const float u = (ms - fadeStart) / (p.sampleLenMs * 0.3f);
    return u >= 1.0f ? 0.0f : (1.0f - u) * (1.0f - u);
}

/** Sub pitch at ms after the hit (before key tracking). */
inline float pitchAt (const Params& p, float ms)
{
    const float punch = std::fmax (p.punchHz, p.endHz);
    const float start = std::fmax (p.startHz, punch);
    return p.endHz + (punch - p.endHz) * std::exp (-ms / p.pitchMs)
                   + (start - punch) * std::exp (-ms / p.snapMs);
}

/** Sub level at ms after the hit: hold at 1, then a squared release that reaches 0 at hold + decay. */
inline float ampAt (const Params& p, float ms)
{
    if (ms <= p.holdMs)
        return 1.0f;
    const float u = (ms - p.holdMs) / p.ampMs;
    return u >= 1.0f ? 0.0f : (1.0f - u) * (1.0f - u);
}

/** RBJ biquad: 12 dB/oct high-pass / low-pass for the sample layer, shelves and a bell for the EQ. */
class Biquad
{
public:
    void setup (bool highPass, float hz, float sampleRate)
    {
        const float w = 6.2831853f * std::fmin (hz, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos (w), alpha = std::sin (w) / (2.0f * 0.7071f);
        const float b1v = highPass ? -(1.0f + cosw) : 1.0f - cosw;
        const float b0v = (highPass ? (1.0f + cosw) : (1.0f - cosw)) * 0.5f;
        set (b0v, b1v, b0v, 1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    /** Low (or high) shelf, slope 1. */
    void setupShelf (bool high, float hz, float gainDb, float sampleRate)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f), sqA = std::sqrt (A);
        const float w = 6.2831853f * std::fmin (hz, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos (w), alpha = std::sin (w) * 0.70710678f; // sin(w)/2 * sqrt(2)
        const float s = high ? 1.0f : -1.0f; // flips the cos terms between low and high shelf
        set (A * ((A + 1) + s * (A - 1) * cosw + 2 * sqA * alpha),
             -2 * s * A * ((A - 1) + s * (A + 1) * cosw),
             A * ((A + 1) + s * (A - 1) * cosw - 2 * sqA * alpha),
             (A + 1) - s * (A - 1) * cosw + 2 * sqA * alpha,
             2 * s * ((A - 1) - s * (A + 1) * cosw),
             (A + 1) - s * (A - 1) * cosw - 2 * sqA * alpha);
    }

    void setupBell (float hz, float gainDb, float q, float sampleRate)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * std::fmin (hz, sampleRate * 0.45f) / sampleRate;
        const float cosw = std::cos (w), alpha = std::sin (w) / (2.0f * q);
        set (1 + alpha * A, -2 * cosw, 1 - alpha * A, 1 + alpha / A, -2 * cosw, 1 - alpha / A);
    }

    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
        a1 = na1 / na0; a2 = na2 / na0;
    }

    void reset() { z1 = z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
};

/** A mono one-shot, kept at its own sample rate and read by time. */
struct ClickSample
{
    std::vector<float> data;
    double sampleRate = 44100.0;
};

using ClickSamplePtr = std::shared_ptr<const ClickSample>;

/** One output sample, plus the two layers before drive (for drawing). */
struct Frame
{
    float out = 0.0f, sub = 0.0f, click = 0.0f;
};

class Engine
{
public:
    void setParams (const Params& p)
    {
        const bool filtersChanged = p.clickHpfHz != params.clickHpfHz || p.clickLpfHz != params.clickLpfHz
                                 || p.eqLowDb != params.eqLowDb || p.eqMidDb != params.eqMidDb
                                 || p.eqMidHz != params.eqMidHz || p.eqHighDb != params.eqHighDb;
        params = p;
        clickRate = std::pow (2.0f, p.clickTune / 12.0f);
        if (filtersChanged)
            filterRate = 0.0f; // recalculated on the next sample
    }
    void setClickSample (ClickSamplePtr s) { sample = std::move (s); }

    /** pitchRatio scales the whole sub sweep (key tracking); the click keeps its own tune. */
    void trigger (float velocity = 1.0f, float pitchRatio = 1.0f)
    {
        // Retriggering mid-kick would jump the output to zero; fade the old value out instead.
        fadeFrom = active ? lastOut : 0.0f;
        ratio = pitchRatio;
        t = 0.0;
        phase = 0.0;
        vel = velocity;
        noise = 0x9e3779b9u; // same seed every hit, so the drawn click matches what you hear
        hpf.reset();
        lpf.reset();
        active = true;
    }

    bool isActive() const { return active; }

    float processSample (float dt) { return processFrame (dt).out; }

    /** Advances one sample; dt is 1 / sampleRate. */
    Frame processFrame (float dt)
    {
        Frame f;
        if (! active)
            return f;

        const float tMs = (float) (t * 1000.0);
        const float freq = ratio * pitchAt (params, tMs);
        const float amp = ampAt (params, tMs);

        phase += (double) freq * dt;
        phase -= std::floor (phase);
        f.sub = std::sin ((float) (phase * 6.283185307179586)) * amp * params.subLevel;
        // Tune plays the click faster or slower, so it also gets shorter or longer.
        float c = clickAt ((tMs - params.offsetMs) * clickRate);
        const float sr = 1.0f / dt;
        if (sr != filterRate)
        {
            hpf.setup (true, params.clickHpfHz, sr);
            lpf.setup (false, params.clickLpfHz, sr);
            eqLow.setupShelf (false, eqLowHz, params.eqLowDb, sr);
            eqMid.setupBell (params.eqMidHz, params.eqMidDb, 0.9f, sr);
            eqHigh.setupShelf (true, eqHighHz, params.eqHighDb, sr);
            filterRate = sr;
        }
        if (params.clickHpfHz > 20.5f)
            c = hpf.process (c);
        if (params.clickLpfHz < 19999.0f)
            c = lpf.process (c);
        f.click = params.click * c * sampleGainAt (params, tMs - params.offsetMs);

        // Drive (off at 0), then the EQ; a flat band is skipped so it can't colour the sound at all.
        float y = driveSample (f.sub + f.click, params.drive);
        if (std::abs (params.eqLowDb) > 0.01f)  y = eqLow.process (y);
        if (std::abs (params.eqMidDb) > 0.01f)  y = eqMid.process (y);
        if (std::abs (params.eqHighDb) > 0.01f) y = eqHigh.process (y);
        f.out = y * vel * std::pow (10.0f, params.levelDb / 20.0f);
        if (tMs < 3.0f)
            f.out += fadeFrom * (1.0f - tMs / 3.0f);
        lastOut = f.out;

        t += dt;
        if (amp <= 0.0f && tMs > params.offsetMs + clickLengthMs() / clickRate + 5.0f) // +5 ms filter ring-out
            active = false;

        return f;
    }

private:
    float clickAt (float ms)
    {
        if (ms < 0.0f)
            return 0.0f;

        if (sample == nullptr || sample->data.empty())
            return ms < 20.0f ? nextNoise() * std::exp (-ms / 2.0f) : 0.0f;

        // Linear interpolation, so any file rate plays at the right speed.
        const double pos = ms * 0.001 * sample->sampleRate;
        const auto i = (size_t) pos;
        if (i + 1 >= sample->data.size())
            return 0.0f;
        const float frac = (float) (pos - (double) i);
        return sample->data[i] + frac * (sample->data[i + 1] - sample->data[i]);
    }

    float clickLengthMs() const
    {
        if (sample == nullptr || sample->data.empty())
            return 20.0f;
        return (float) (1000.0 * (double) sample->data.size() / sample->sampleRate);
    }

    float nextNoise()
    {
        noise = noise * 1664525u + 1013904223u;
        return (float) (noise >> 8) / 8388608.0f - 1.0f;
    }

    Params params;
    ClickSamplePtr sample;
    Biquad hpf, lpf, eqLow, eqMid, eqHigh;
    float clickRate = 1.0f, filterRate = 0.0f, ratio = 1.0f, fadeFrom = 0.0f, lastOut = 0.0f;
    double t = 0.0, phase = 0.0;
    float vel = 1.0f;
    uint32_t noise = 0;
    bool active = false;
};
} // namespace kick
