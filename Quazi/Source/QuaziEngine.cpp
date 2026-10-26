#include "QuaziEngine.h"
#include <cmath>
#include <string>

namespace quazi
{
namespace
{
    using LR = juce::dsp::LinkwitzRileyFilter<float>;
    using ArrayCoefs = juce::dsp::IIR::ArrayCoefficients<float>;

    const char* const kBandKey[NumBands] = { "lo", "lm", "hm", "hi" };

    float smoothCoef (double rate, double seconds) { return (float) (1.0 - std::exp (-1.0 / (seconds * rate))); }

    // The splits must rise and stay below Nyquist of the base rate
    std::array<float, 3> orderedXover (std::array<float, 3> x, double sampleRate)
    {
        const float top = (float) juce::jmin (18000.0, sampleRate * 0.42);
        x[0] = juce::jlimit (30.0f, top / 1.6f, x[0]);
        x[1] = juce::jlimit (x[0] * 1.25f, top / 1.25f, x[1]);
        x[2] = juce::jlimit (x[1] * 1.25f, top, x[2]);
        return x;
    }

    // Driving harder keeps the band about as loud: the gain goes in, half of it (in dB) comes back off
    float makeup (float drive) { return 1.0f / std::sqrt (drive); }
}

juce::String bandId (int band, const char* what) { return juce::String (kBandKey[juce::jlimit (0, NumBands - 1, band)]) + what; }

const std::vector<skazi::ParamDef>& paramDefs()
{
    using P = skazi::ParamDef;
    static const std::vector<P> defs = []
    {
        std::vector<P> d = {
            { "in", "Input", P::Float, -24, 24, 0, "dB", 0.1f },
            { "mix", "Mix", P::Float, 0, 100, 100, "%", 0.1f },
            { "out", "Output", P::Float, -24, 12, 0, "dB", 0.1f },
            { "xLo", "Crossover Low", P::Float, 40, 1000, 150, "Hz", 1, 0.4f },
            { "xMid", "Crossover Mid", P::Float, 200, 5000, 1200, "Hz", 1, 0.4f },
            { "xHi", "Crossover High", P::Float, 1000, 16000, 5000, "Hz", 1, 0.4f },
        };
        static const char* const names[NumBands] = { "Low", "Low-Mid", "High-Mid", "High" };
        static std::vector<std::string> ids, labels; // ParamDef keeps char pointers, so the strings live here
        ids.reserve (NumBands * 4); labels.reserve (NumBands * 4);
        for (int b = 0; b < NumBands; ++b)
        {
            auto add = [&] (const char* what, P::Kind kind, float lo, float hi, float def, const char* unit, float step, juce::StringArray choices = {})
            {
                ids.push_back (bandId (b, what).toStdString());
                labels.push_back (std::string (names[b]) + " " + what);
                d.push_back ({ ids.back().c_str(), labels.back().c_str(), kind, lo, hi, def, unit, step, 1.0f, choices });
            };
            add ("On", P::Bool, 0, 1, 1, "", 1);
            add ("Mode", P::Choice, 0, NumModes - 1, Soft, "", 1, { "SOFT", "HARD", "FUZZ", "TUBE" });
            add ("Drive", P::Float, 0, 48, 12, "dB", 0.1f);
            add ("Level", P::Float, -24, 12, 0, "dB", 0.1f);
        }
        d.push_back ({ "width", "Stereo Width", P::Float, 0, 200, 100, "%", 0.1f });
        d.push_back ({ "monoBass", "Mono Bass", P::Bool, 0, 1, 1 });
        d.push_back ({ "eqLowDb", "EQ Low Gain", P::Float, -15, 15, 0, "dB", 0.1f });
        d.push_back ({ "eqLowF", "EQ Low Freq", P::Float, 30, 500, 100, "Hz", 1, 0.5f });
        d.push_back ({ "eqMidDb", "EQ Mid Gain", P::Float, -15, 15, 0, "dB", 0.1f });
        d.push_back ({ "eqMidF", "EQ Mid Freq", P::Float, 200, 8000, 1000, "Hz", 1, 0.4f });
        d.push_back ({ "eqMidQ", "EQ Mid Q", P::Float, 0.3f, 6, 0.7f, "", 0.01f, 0.5f });
        d.push_back ({ "eqHighDb", "EQ High Gain", P::Float, -15, 15, 0, "dB", 0.1f });
        d.push_back ({ "eqHighF", "EQ High Freq", P::Float, 1500, 16000, 8000, "Hz", 1, 0.5f });
        return d;
    }();
    return defs;
}

Settings Settings::fromValues (const std::function<float (const char*)>& v)
{
    using juce::Decibels;
    Settings s;
    s.inGain = Decibels::decibelsToGain (v ("in"));
    s.mix = v ("mix") * 0.01f;
    s.outGain = Decibels::decibelsToGain (v ("out"));
    s.xover = { v ("xLo"), v ("xMid"), v ("xHi") };
    for (int b = 0; b < NumBands; ++b)
    {
        s.on[(size_t) b] = v (bandId (b, "On").toRawUTF8()) > 0.5f;
        s.mode[(size_t) b] = (int) v (bandId (b, "Mode").toRawUTF8());
        s.driveDb[(size_t) b] = v (bandId (b, "Drive").toRawUTF8());
        s.level[(size_t) b] = Decibels::decibelsToGain (v (bandId (b, "Level").toRawUTF8()));
    }
    s.width = v ("width") * 0.01f;
    s.monoBass = v ("monoBass") > 0.5f;
    s.lowDb = v ("eqLowDb"); s.lowFreq = v ("eqLowF");
    s.midDb = v ("eqMidDb"); s.midFreq = v ("eqMidF"); s.midQ = v ("eqMidQ");
    s.highDb = v ("eqHighDb"); s.highFreq = v ("eqHighF");
    return s;
}

float shape (int mode, float x)
{
    switch (mode)
    {
        case Hard: return juce::jlimit (-1.0f, 1.0f, x);
        case Fuzz: return juce::jlimit (-1.0f, 1.0f, 1.3f * (std::tanh (1.5f * (x + 0.2f)) - std::tanh (0.3f))); // biased: buzzy even harmonics
        case Tube: return x >= 0.0f ? std::tanh (x) : std::tanh (1.6f * x) / 1.6f;                                // the negative side flattens first
        default:   return std::tanh (x);
    }
}

float eqMagnitude (const Settings& s, double sampleRate, double freq)
{
    using C = juce::dsp::IIR::Coefficients<float>;
    using juce::Decibels;
    const auto nyq = (float) (sampleRate * 0.45);
    const double m = C (ArrayCoefs::makeLowShelf (sampleRate, juce::jmin (s.lowFreq, nyq), 0.707f, Decibels::decibelsToGain (s.lowDb))).getMagnitudeForFrequency (freq, sampleRate)
                   * C (ArrayCoefs::makePeakFilter (sampleRate, juce::jmin (s.midFreq, nyq), s.midQ, Decibels::decibelsToGain (s.midDb))).getMagnitudeForFrequency (freq, sampleRate)
                   * C (ArrayCoefs::makeHighShelf (sampleRate, juce::jmin (s.highFreq, nyq), 0.707f, Decibels::decibelsToGain (s.highDb))).getMagnitudeForFrequency (freq, sampleRate);
    return (float) m;
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    // 4x with linear-phase filters: hard clipping and fuzz stay clean of aliasing, and the delay is a whole number of samples
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    maxBlock = juce::jmax (1, maxBlock);
    maxBlockSize = maxBlock;
    os->initProcessing ((size_t) maxBlock);
    latencySamples = (int) std::round (os->getLatencyInSamples());

    const juce::dsp::ProcessSpec osSpec { sr * 4.0, (juce::uint32) maxBlock * 4, 2 };
    for (auto& f : split) { f.prepare (osSpec); }
    for (auto& f : allpass) { f.setType (juce::dsp::LinkwitzRileyFilterType::allpass); f.prepare (osSpec); }

    const juce::dsp::ProcessSpec mono { sr, (juce::uint32) maxBlock, 1 };
    for (int c = 0; c < 2; ++c)
    {
        eqLow[(size_t) c].coefficients = new juce::dsp::IIR::Coefficients<float> (ArrayCoefs::makeLowShelf (sr, 100.0f, 0.707f, 1.0f));
        eqMid[(size_t) c].coefficients = new juce::dsp::IIR::Coefficients<float> (ArrayCoefs::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f));
        eqHigh[(size_t) c].coefficients = new juce::dsp::IIR::Coefficients<float> (ArrayCoefs::makeHighShelf (sr, 8000.0f, 0.707f, 1.0f));
        for (auto* f : { &eqLow[(size_t) c], &eqMid[(size_t) c], &eqHigh[(size_t) c] }) f->prepare (mono);
        dcX[c] = dcY[c] = 0.0f;
    }

    primed = false;
    inPk = outPk = sidePk = midPk = 0.0f;
    bandPk.fill (0.0f);
}

void Engine::updateFilters (const Settings& s, bool snap)
{
    // Block-rate glide for the filter frequencies and EQ (log domain for frequencies)
    const float k = snap ? 1.0f : 0.25f;
    const auto target = orderedXover (s.xover, sr);
    for (size_t i = 0; i < 3; ++i)
        xoverSm[i] = std::exp (std::log (juce::jmax (1.0f, xoverSm[i])) + k * (std::log (target[i]) - std::log (juce::jmax (1.0f, xoverSm[i]))));
    if (snap) xoverSm = target;

    for (size_t i = 0; i < 3; ++i) split[i].setCutoffFrequency (xoverSm[i]);
    allpass[0].setCutoffFrequency (xoverSm[1]);
    allpass[1].setCutoffFrequency (xoverSm[2]);
    allpass[2].setCutoffFrequency (xoverSm[2]);

    auto glide = [k] (float& v, float t) { const bool changed = std::abs (v - t) > 1.0e-4f; v += k * (t - v); if (std::abs (v - t) < 1.0e-4f) v = t; return changed; };
    auto glideLog = [k] (float& v, float t) { const bool changed = std::abs (v - t) > 0.01f; v = std::exp (std::log (v) + k * (std::log (t) - std::log (v))); if (std::abs (v - t) < 0.01f) v = t; return changed; };
    const float nyq = (float) (sr * 0.45);
    bool lowChanged = glide (lowDbSm, s.lowDb) | glideLog (lowFreqSm, juce::jmin (s.lowFreq, nyq));
    bool midChanged = glide (midDbSm, s.midDb) | glideLog (midFreqSm, juce::jmin (s.midFreq, nyq)) | glideLog (midQSm, s.midQ);
    bool highChanged = glide (highDbSm, s.highDb) | glideLog (highFreqSm, juce::jmin (s.highFreq, nyq));
    if (snap) lowChanged = midChanged = highChanged = true;

    using juce::Decibels;
    for (size_t c = 0; c < 2; ++c)
    {
        if (lowChanged)  *eqLow[c].coefficients  = ArrayCoefs::makeLowShelf (sr, lowFreqSm, 0.707f, Decibels::decibelsToGain (lowDbSm));
        if (midChanged)  *eqMid[c].coefficients  = ArrayCoefs::makePeakFilter (sr, midFreqSm, midQSm, Decibels::decibelsToGain (midDbSm));
        if (highChanged) *eqHigh[c].coefficients = ArrayCoefs::makeHighShelf (sr, highFreqSm, 0.707f, Decibels::decibelsToGain (highDbSm));
    }
}

void Engine::resetState()
{
    for (auto& f : split) f.reset();
    for (auto& f : allpass) f.reset();
    for (size_t c = 0; c < 2; ++c) { eqLow[c].reset(); eqMid[c].reset(); eqHigh[c].reset(); }
    if (os != nullptr) os->reset();
    for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = 0.0f;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels()), n = buffer.getNumSamples();
    if (numCh == 0 || n == 0 || os == nullptr) return;

    // Hosts may send more than they promised in prepare; the oversampler only has room for maxBlock
    for (int start = 0; start < n; start += maxBlockSize)
    {
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, start, juce::jmin (maxBlockSize, n - start));
        processChunk (chunk, s);
    }
}

void Engine::processChunk (juce::AudioBuffer<float>& buffer, const Settings& s)
{
    const int numCh = juce::jmin (2, buffer.getNumChannels()), n = buffer.getNumSamples();

    if (! primed)
    {
        inSm = s.inGain; mixSm = s.mix; outSm = s.outGain; widthSm = s.width;
        for (size_t b = 0; b < NumBands; ++b)
        {
            driveSm[b] = juce::Decibels::decibelsToGain (s.driveDb[b]);
            levelSm[b] = s.level[b];
            onSm[b] = s.on[b] ? 1.0f : 0.0f;
        }
        lowDbSm = s.lowDb; lowFreqSm = s.lowFreq; midDbSm = s.midDb; midFreqSm = s.midFreq; midQSm = s.midQ;
        highDbSm = s.highDb; highFreqSm = s.highFreq;
        updateFilters (s, true);
        primed = true;
    }
    else updateFilters (s, false);

    // Input gain
    const float a = smoothCoef (sr, 0.02);
    {
        float g = inSm;
        for (int i = 0; i < n; ++i)
        {
            g += a * (s.inGain - g);
            for (int c = 0; c < numCh; ++c)
            {
                auto& x = buffer.getWritePointer (c)[i];
                x = std::isfinite (x) ? x * g : 0.0f; // a NaN from upstream would stick in the filters for good
            }
        }
        inSm = g;
    }
    float pkIn = 0.0f;
    for (int c = 0; c < numCh; ++c) pkIn = juce::jmax (pkIn, buffer.getMagnitude (c, 0, n));

    // Split, fuzz each band, blend with its clean self, sum: all at 4x
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int un = (int) up.getNumSamples();
    const float ua = smoothCoef (sr * 4.0, 0.02);

    std::array<float, NumBands> driveT {}, peaks {};
    for (size_t b = 0; b < NumBands; ++b) driveT[b] = juce::Decibels::decibelsToGain (s.driveDb[b]);

    float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer ((size_t) (numCh - 1)) };
    for (int i = 0; i < un; ++i)
    {
        for (size_t b = 0; b < NumBands; ++b)
        {
            driveSm[b] += ua * (driveT[b] - driveSm[b]);
            levelSm[b] += ua * (s.level[b] - levelSm[b]);
            onSm[b] += ua * ((s.on[b] ? 1.0f : 0.0f) - onSm[b]);
        }
        mixSm += ua * (s.mix - mixSm);
        const float wetAmt = mixSm;

        float lowBand[2] {}, rest[2] {};
        for (int c = 0; c < numCh; ++c)
        {
            std::array<float, NumBands> band {};
            float r1, r2;
            split[0].processSample (c, ch[c][i], band[Low], r1);
            split[1].processSample (c, r1, band[LowMid], r2);
            split[2].processSample (c, r2, band[HighMid], band[High]);
            band[Low] = allpass[1].processSample (c, allpass[0].processSample (c, band[Low]));
            band[LowMid] = allpass[2].processSample (c, band[LowMid]);

            float sum = 0.0f;
            for (size_t b = 0; b < NumBands; ++b)
            {
                const float wet = shape (s.mode[b], band[b] * driveSm[b]) * makeup (driveSm[b]);
                const float y = (band[b] + wetAmt * onSm[b] * (wet - band[b])) * levelSm[b]; // an OFF band passes through clean
                peaks[b] = juce::jmax (peaks[b], std::abs (y));
                if (b == Low) lowBand[c] = y; else sum += y;
            }
            rest[c] = sum;
        }

        if (numCh == 2 && s.monoBass)
        {
            const float m = 0.5f * (lowBand[0] + lowBand[1]);
            lowBand[0] = lowBand[1] = m;
        }
        for (int c = 0; c < numCh; ++c) ch[c][i] = lowBand[c] + rest[c];
    }
    os->processSamplesDown (block);

    // Width, post EQ, DC block, output
    const float dcR = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sr);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (numCh - 1);
    float pkOut = 0.0f, pkMid = 0.0f, pkSide = 0.0f;
    bool blewUp = false;
    for (int i = 0; i < n; ++i)
    {
        widthSm += a * (s.width - widthSm);
        outSm += a * (s.outGain - outSm);

        float x[2] = { outL[i], outR[i] };
        if (numCh == 2)
        {
            const float mid = 0.5f * (x[0] + x[1]), side = 0.5f * (x[0] - x[1]) * widthSm;
            x[0] = mid + side; x[1] = mid - side;
            pkMid = juce::jmax (pkMid, std::abs (mid));
            pkSide = juce::jmax (pkSide, std::abs (side));
        }
        for (int c = 0; c < numCh; ++c)
        {
            float y = eqHigh[(size_t) c].processSample (eqMid[(size_t) c].processSample (eqLow[(size_t) c].processSample (x[c])));
            const float d = y - dcX[c] + dcR * dcY[c];
            dcX[c] = y; dcY[c] = d;
            y = skazi::safeClip (d * outSm);
            (c == 0 ? outL : outR)[i] = y;
            pkOut = juce::jmax (pkOut, std::abs (y));
            blewUp = blewUp || ! std::isfinite (y);
        }
    }

    // Last line of defence: if anything still blew up, start clean instead of staying broken
    if (blewUp)
    {
        resetState();
        for (int c = 0; c < numCh; ++c) buffer.clear (c, 0, n);
        pkOut = 0.0f;
    }

    // Meters
    const float fall = std::exp (-(float) n / (float) (sr * 0.25));
    inPk = juce::jmax (pkIn, inPk * fall);
    outPk = juce::jmax (pkOut, outPk * fall);
    for (size_t b = 0; b < NumBands; ++b) bandPk[b] = juce::jmax (peaks[b], bandPk[b] * fall);
    midPk = juce::jmax (pkMid, midPk * fall);
    sidePk = juce::jmax (pkSide, sidePk * fall);
    uiIn.store (inPk);
    uiOut.store (outPk);
    for (size_t b = 0; b < NumBands; ++b) uiBand[b].store (bandPk[b]);
    uiWidth.store (midPk > 1.0e-4f ? sidePk / midPk : 0.0f);
}
} // namespace quazi
