#include "TahiniEngine.h"
#include <cmath>

namespace tahini
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    // FDN line lengths in ms (mutually prime-ish so the echoes don't pile up into a metallic ring)
    constexpr double kFdnMs[8] = { 29.7, 37.1, 41.1, 43.7, 53.0, 59.3, 67.1, 73.9 };
    constexpr int kDiffSamples[4] = { 142, 107, 379, 277 };
}

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = {
        { "dmode", "Delay Mode", P::Choice, 0, 2, 0, "", 1, 1, { "PING-PONG", "STEREO", "TAPE" } },
        { "dtime", "Delay Time", P::Choice, 0, 15, 12, "", 1, 1, skazi::noteLengthNames() },
        { "fb", "Feedback", P::Float, 0, 95, 55, "%", 0.1f },
        { "locut", "Low Cut", P::Float, 20, 2000, 250, "Hz", 1, 0.4f },
        { "hicut", "High Cut", P::Float, 1000, 20000, 6500, "Hz", 1, 0.4f },
        { "wow", "Wow", P::Float, 0, 100, 25, "%", 0.1f },
        { "ddrive", "Delay Drive", P::Float, 0, 100, 10, "%", 0.1f },
        { "freeze", "Freeze", P::Bool, 0, 1, 0 },
        { "size", "Size", P::Float, 0, 100, 65, "%", 0.1f },
        { "decay", "Decay", P::Float, 0.2f, 20, 4.2f, "s", 0.01f, 0.4f },
        { "predelay", "Pre-Delay", P::Float, 0, 250, 30, "ms", 0.1f },
        { "damp", "Damping", P::Float, 0, 100, 50, "%", 0.1f },
        { "width", "Width", P::Float, 0, 100, 90, "%", 0.1f },
        { "duck", "Duck Amount", P::Float, 0, 100, 60, "%", 0.1f },
        { "duckrel", "Duck Release", P::Float, 20, 1000, 200, "ms", 1, 0.5f },
        { "dmix", "Delay Mix", P::Float, 0, 100, 40, "%", 0.1f },
        { "rmix", "Reverb Mix", P::Float, 0, 100, 30, "%", 0.1f },
        { "dry", "Dry Signal", P::Bool, 0, 1, 1 },
        { "gain", "Output", P::Float, -24, 12, 0, "dB", 0.1f },
    };
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    Settings s;
    s.mode = (int) v ("dmode"); s.time = (int) v ("dtime"); s.feedback = v ("fb") * 0.01f;
    s.loCut = v ("locut"); s.hiCut = v ("hicut"); s.wow = v ("wow") * 0.01f; s.drive = v ("ddrive") * 0.01f;
    s.freeze = v ("freeze") > 0.5f; s.size = v ("size") * 0.01f; s.decay = v ("decay"); s.predelayMs = v ("predelay");
    s.damp = v ("damp") * 0.01f; s.width = v ("width") * 0.01f;
    s.duck = v ("duck") * 0.01f; s.duckRelMs = v ("duckrel");
    s.delayMix = v ("dmix") * 0.01f; s.reverbMix = v ("rmix") * 0.01f; s.dry = v ("dry") > 0.5f;
    s.gain = juce::Decibels::decibelsToGain (v ("gain"));
    return s;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& d : dl) d.init ((int) (sr * 4.2));
    pre.init ((int) (sr * 0.3));
    for (int i = 0; i < 4; ++i)
    {
        diff[i].length = juce::jmax (1, (int) (kDiffSamples[i] * sr / 44100.0));
        diff[i].line.init (diff[i].length + 2);
    }
    for (int i = 0; i < N; ++i) fdn[i].init ((int) ((kFdnMs[i] * 1.6 + 12.0) * 0.001 * sr));
    std::fill (std::begin (fdnLp), std::end (fdnLp), 0.0f);
    delaySmooth = 0; env = 0;
    std::fill (std::begin (hp), std::end (hp), 0.0f);
    std::fill (std::begin (lpA), std::end (lpA), 0.0f);
    std::fill (std::begin (lpB), std::end (lpB), 0.0f);
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s, juce::AudioPlayHead* playHead)
{
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (numCh == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (numCh > 1 ? 1 : 0);

    const auto ht = skazi::hostTime (playHead);
    uiBpm.store (ht.bpm);
    const double target = juce::jlimit (1.0, sr * 4.0, skazi::noteBeats (s.time) * 60.0 / ht.bpm * sr);
    if (delaySmooth <= 0.0) delaySmooth = target;
    const double smooth = s.mode == Tape ? 0.00005 : 0.0015;
    const bool tape = s.mode == Tape;
    const double wowDepth = (tape ? juce::jmax (0.2f, s.wow) : s.wow) * 0.003 * sr;
    const float aLo = (float) (1.0 - std::exp (-twoPi * s.loCut / sr));
    const float aHi = (float) (1.0 - std::exp (-twoPi * (tape ? juce::jmin (s.hiCut, 5000.0f) : s.hiCut) / sr));
    const float drv = 1.0f + s.drive * 3.0f + (tape ? 1.0f : 0.0f);
    const float fb = juce::jmin (0.95f, s.feedback);

    const float att = (float) (1.0 - std::exp (-1.0 / (0.001 * sr)));
    const float rel = (float) (1.0 - std::exp (-1.0 / (s.duckRelMs * 0.001 * sr)));

    double lens[N];
    float gains[N];
    const float dampFc = 20000.0f * (1.0f - s.damp) + 1500.0f * s.damp;
    const float aDamp = (float) (1.0 - std::exp (-twoPi * juce::jmin (dampFc, (float) (0.45 * sr)) / sr));
    for (int i = 0; i < N; ++i)
    {
        lens[i] = kFdnMs[i] * (0.35 + 1.25 * s.size) * 0.001 * sr;
        gains[i] = s.freeze ? 1.0f : (float) std::pow (10.0, -3.0 * lens[i] / (juce::jmax (0.2f, s.decay) * sr));
    }
    const double preSamples = juce::jlimit (1.0, sr * 0.29, s.predelayMs * 0.001 * sr);

    auto feedbackPath = [&] (float x, int c)
    {
        hp[c] += (x - hp[c]) * aLo;              // low cut
        x -= hp[c];
        lpA[c] += (x - lpA[c]) * aHi;            // high cut, 2 poles
        lpB[c] += (lpA[c] - lpB[c]) * aHi;
        return std::tanh (lpB[c] * drv) / drv;   // drive never adds gain, so feedback stays stable
    };

    const float decayMeter = (float) std::exp (-(double) n / (0.2 * sr));
    for (int i = 0; i < n; ++i)
    {
        const float dryL = L[i], dryR = R[i];

        // duck the wet signal while the dry signal plays
        const float x = juce::jmax (std::abs (dryL), std::abs (dryR));
        env += (x - env) * (x > env ? att : rel);
        const float duckGain = 1.0f - s.duck * juce::jlimit (0.0f, 1.0f, env * 3.0f);

        // delay
        delaySmooth += (target - delaySmooth) * smooth;
        wowPh += 0.6 / sr; flutterPh += 7.0 / sr;
        if (wowPh > 1.0) wowPh -= 1.0;
        if (flutterPh > 1.0) flutterPh -= 1.0;
        const double mod = wowDepth * (std::sin (twoPi * wowPh) + 0.2 * std::sin (twoPi * flutterPh));
        const double dBase = juce::jmax (2.0, delaySmooth);
        const float eL = dl[0].read (juce::jmax (1.0, dBase + mod));
        const float eR = dl[1].read (juce::jmax (1.0, dBase + (s.mode == Stereo ? -mod : mod)));
        if (s.mode == PingPong)
        {
            dl[0].write (0.5f * (dryL + dryR) + feedbackPath (eR, 1) * fb);
            dl[1].write (feedbackPath (eL, 0) * fb);
        }
        else
        {
            dl[0].write (dryL + feedbackPath (eL, 0) * fb);
            dl[1].write (dryR + feedbackPath (eR, 1) * fb);
        }

        // reverb: pre-delay -> 4 diffusers -> 8-line FDN with Householder mixing
        pre.write (s.freeze ? 0.0f : 0.5f * (dryL + dryR) + 0.25f * (eL + eR) * s.delayMix);
        float in = pre.read (preSamples);
        for (auto& d : diff) in = d.process (in);

        modPh += 0.5 / sr;
        if (modPh > 1.0) modPh -= 1.0;
        const double m = std::sin (twoPi * modPh) * 8.0;
        float o[N], sum = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            o[k] = fdn[k].read (lens[k] + (k == 0 ? m : k == 3 ? -m : 0.0));
            fdnLp[k] += (o[k] - fdnLp[k]) * aDamp;
            const float damped = s.freeze ? o[k] : fdnLp[k];
            o[k] = damped * gains[k];
            sum += o[k];
        }
        float revL = 0.0f, revR = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            const float y = o[k] - 0.25f * sum; // Householder: I - 2/N * ones
            fdn[k].write (y + in * ((k & 1) ? -0.5f : 0.5f));
            if (k & 1) revR += o[k] * ((k & 2) ? -0.35f : 0.35f);
            else       revL += o[k] * ((k & 2) ? -0.35f : 0.35f);
        }
        const float mid = 0.5f * (revL + revR), side = 0.5f * (revL - revR) * s.width;
        revL = mid + side; revR = mid - side;

        const float wetL = (eL * s.delayMix + revL * s.reverbMix) * duckGain;
        const float wetR = (eR * s.delayMix + revR * s.reverbMix) * duckGain;
        const float outL = skazi::safeClip (((s.dry ? dryL : 0.0f) + wetL) * s.gain);
        const float outR = skazi::safeClip (((s.dry ? dryR : 0.0f) + wetR) * s.gain);
        if (numCh > 1) { L[i] = outL; R[i] = outR; }
        else           { L[i] = 0.5f * (outL + outR); }

        wetPeak = juce::jmax (wetPeak, std::abs (wetL), std::abs (wetR));
        outPeak = juce::jmax (outPeak, std::abs (outL), std::abs (outR));
    }
    uiWet.store (wetPeak);
    uiOut.store (outPeak);
    uiDuck.store (1.0f - s.duck * juce::jlimit (0.0f, 1.0f, env * 3.0f));
    wetPeak *= decayMeter;
    outPeak *= decayMeter;
}
} // namespace tahini
