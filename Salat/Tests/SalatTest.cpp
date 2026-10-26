// Checks SALAT's dry path, the chopper (stutter, reverse, hold, MAKE A SALAT burst), clicks, presets and CPU.
//   SalatTest [render-folder]   also renders a test beat through some presets as WAVs
#include "../Source/SalatEngine.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace salat;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-48s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    const double sr = 48000.0;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int kBlock = 512;
    constexpr int kStep = 6000; // 1/16 at 120 bpm, 48 kHz

    struct Out { std::vector<float> L, R, inL; int latency = 0; std::vector<int> history; };

    Out run (const Settings& s, int total, const std::function<float (int)>& input, const std::function<void (Engine&, int)>& before = {})
    {
        Engine e;
        e.prepare (sr, kBlock);
        juce::AudioBuffer<float> buf (2, kBlock);
        Out o;
        o.latency = e.latency();
        skazi::HostTime host; // stopped: the engine runs its own 120 bpm clock from beat 0
        for (int pos = 0; pos < total; pos += kBlock)
        {
            if (before) before (e, pos);
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = input (pos + i);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
                o.inL.push_back (x);
            }
            const int countBefore = e.historyCount.load();
            e.process (buf, s, host);
            for (int c = countBefore; c < e.historyCount.load(); ++c)
                o.history.push_back (e.history[(size_t) (c % Engine::kHistory)].load());
            for (int i = 0; i < kBlock; ++i) { o.L.push_back (buf.getSample (0, i)); o.R.push_back (buf.getSample (1, i)); }
        }
        return o;
    }

    bool clean (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x) || std::abs (x) > 1.0f) return false; return true; }
    float maxStep (const std::vector<float>& v, size_t from)
    {
        float m = 0;
        for (size_t i = from + 1; i < v.size(); ++i) m = std::max (m, std::abs (v[i] - v[i - 1]));
        return m;
    }

    float noise (int i) { juce::Random r (i * 7919 + 13); return (r.nextFloat() * 2.0f - 1.0f) * 0.2f; }

    // Strongest frequency between lo and hi Hz in [from, to) seconds (a DFT scan): a pitch shifter's
    // read jumps would fool a zero-crossing count
    double peakHz (const std::vector<float>& v, double from, double to, double lo, double hi)
    {
        double best = 0, bestHz = 0;
        for (double f = lo; f <= hi; f += 0.5)
        {
            double re = 0, im = 0;
            for (size_t i = (size_t) (from * sr); i < (size_t) (to * sr); ++i)
            {
                re += v[i] * std::cos (twoPi * f * (double) i / sr);
                im += v[i] * std::sin (twoPi * f * (double) i / sr);
            }
            if (re * re + im * im > best) { best = re * re + im * im; bestHz = f; }
        }
        return bestHz;
    }

    // kick on the beat, hats on the off-16ths, a saw bass: something with rhythm to chop
    float beat (int i)
    {
        const double t = i / sr, beatLen = 0.5;
        const double inBeat = std::fmod (t, beatLen), in16 = std::fmod (t, beatLen / 4);
        const double kickPh = 50.0 * inBeat + 120.0 * 0.03 * (1.0 - std::exp (-inBeat / 0.03));
        float x = (float) (std::sin (twoPi * kickPh) * std::exp (-inBeat / 0.12)) * 0.6f;
        juce::Random r (i * 31 + 7);
        if (std::fmod (t, beatLen / 2) >= beatLen / 4) x += (r.nextFloat() * 2.0f - 1.0f) * 0.15f * (float) std::exp (-in16 / 0.015);
        const double bassHz = 55.0 * (std::fmod (t, 2.0) < 1.0 ? 1.0 : 1.5);
        x += (float) (2.0 * std::fmod (bassHz * t, 1.0) - 1.0) * 0.12f * (float) (1.0 - std::exp (-inBeat / 0.01));
        return x;
    }

    Settings everythingOff()
    {
        auto s = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
        s.chopOn = s.driveOn = s.crushOn = s.filterOn = s.ringOn = s.modOn = s.delayOn = s.reverbOn = false;
        return s;
    }

    Settings fromPreset (const skazi::Preset& p) { return Settings::fromValues ([&] (const char* id) { return p.values.at (id); }); }

    void writeWav (const juce::File& f, const Out& o)
    {
        f.deleteFile();
        juce::AudioBuffer<float> b (2, (int) o.L.size());
        for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample (0, i, o.L[(size_t) i]); b.setSample (1, i, o.R[(size_t) i]); }
        juce::WavAudioFormat wav;
        if (auto out = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
            out->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main (int argc, char** argv)
{
    // everything off: the output is the input, delayed by exactly the reported latency
    {
        // a sine: the oversampling filters are only flat below ~20 kHz, so white noise would not come back exactly
        const auto o = run (everythingOff(), 24000, [] (int i) { return 0.3f * (float) std::sin (twoPi * 220.0 * i / sr); });
        float err = 0;
        for (size_t i = 2000; i < 23000; ++i) err = std::max (err, std::abs (o.L[i] - o.inL[i - (size_t) o.latency]));
        expect (err < 2.0e-3f, "all off = input (latency " + juce::String (o.latency) + ")", "max error " + juce::String (err, 6));
    }

    // MIX 0: the dry path, with every effect on
    {
        auto s = fromPreset (salat::presets()[4]); s.mix = 0;
        const auto o = run (s, 48000, noise);
        float err = 0;
        for (size_t i = 24000; i < 47000; ++i) err = std::max (err, std::abs (o.L[i] - o.inL[i - (size_t) o.latency])); // after the mix control has glided to 0
        expect (err < 1.0e-4f, "mix 0 = dry, latency aligned", "max error " + juce::String (err, 6));
    }

    // stutter: the step from 6000 repeats its first quarter
    {
        auto s = everythingOff(); s.chopOn = true; s.chance = 1; s.weights = { 1, 0, 0, 0 }; s.repeats = 4;
        const auto o = run (s, 20000, noise);
        float err = 0;
        for (int m = 1; m < 4; ++m)
            for (int k = 300; k < 1300; ++k)
                err = std::max (err, std::abs (o.L[(size_t) (kStep + k + 1500 * m)] - o.L[(size_t) (kStep + k)]));
        expect (err < 1.0e-5f && ! o.history.empty() && o.history[0] == Stutter, "stutter repeats the slice", "max error " + juce::String (err, 7));
    }

    // reverse: the step from 6000 plays the one before it backwards
    {
        auto s = everythingOff(); s.chopOn = true; s.chance = 1; s.weights = { 0, 1, 0, 0 };
        const auto o = run (s, 14000, noise);
        float err = 0;
        for (int t = 300; t < 5500; ++t) err = std::max (err, std::abs (o.L[(size_t) (kStep + t)] - o.L[(size_t) (kStep - 1 - t)]));
        expect (err < 1.0e-5f, "reverse plays the last step backwards", "max error " + juce::String (err, 7));
    }

    // chance 0: nothing happens, until MAKE A SALAT asks for four steps
    {
        auto s = everythingOff(); s.chopOn = true; s.chance = 0;
        const auto o = run (s, 30 * kStep, noise, [] (Engine& e, int pos) { if (pos <= 10 * kStep && pos + kBlock > 10 * kStep) e.burst (4); });
        int events = 0;
        for (int ev : o.history) events += ev != None ? 1 : 0;
        expect (events == 4, "burst chops exactly four steps", juce::String (events) + " events in " + juce::String ((int) o.history.size()) + " steps");
    }

    // hold
    {
        auto s = everythingOff(); s.chopOn = true; s.hold = true;
        const auto o = run (s, 10 * kStep, noise);
        bool allHold = ! o.history.empty();
        for (int ev : o.history) allHold = allHold && ev == Hold;
        expect (allHold, "hold loops every step");
    }

    // no clicks: chopping a sine never jumps much more than the sine itself does
    {
        auto s = everythingOff(); s.chopOn = true; s.chance = 1; s.weights = { 1, 1, 1, 1 }; s.repeats = 8;
        auto sine = [] (int i) { return 0.5f * (float) std::sin (twoPi * 110.0 * i / sr); };
        const auto plain = run (everythingOff(), 200000, sine);
        const auto chopped = run (s, 200000, sine);
        const float a = maxStep (plain.L, 2000), b = maxStep (chopped.L, 2000);
        expect (b < a * 2.5f, "chop crossfades (no clicks)", "max step " + juce::String (b, 4) + " vs sine " + juce::String (a, 4));
    }

    // every factory preset and 200 random salats stay finite and under 0 dBFS
    {
        bool ok = true; int bad = -1;
        const auto& list = salat::presets();
        for (size_t i = 0; i < list.size() && ok; ++i)
        {
            const auto o = run (fromPreset (list[i]), 96000, beat);
            if (! clean (o.L) || ! clean (o.R)) { ok = false; bad = (int) i; }
        }
        expect (ok, "factory presets clean (" + juce::String ((int) list.size()) + ")", bad >= 0 ? list[(size_t) bad].name : juce::String());

        juce::Random rng (42);
        ok = true; int silent = 0;
        for (int i = 0; i < 200 && ok; ++i)
        {
            const auto o = run (fromPreset (salat::randomPreset (rng)), 48000, beat);
            ok = clean (o.L) && clean (o.R);
            float pk = 0; for (float x : o.L) pk = std::max (pk, std::abs (x));
            if (pk < 0.01f) ++silent;
        }
        expect (ok && silent == 0, "200 random salats clean and audible", juce::String (silent) + " silent");
    }

    // RISE: +12 st over 2 bars (4 s at 120 bpm), then holds; measured by zero crossings of a 220 Hz sine
    {
        auto s = everythingOff(); s.riseOn = true; s.riseLen = 0; s.riseSemis = 12;
        const auto o = run (s, (int) sr * 6, [] (int i) { return 0.3f * (float) std::sin (twoPi * 220.0 * i / sr); });
        const double mid = peakHz (o.L, 1.9, 2.1, 180, 520), top = peakHz (o.L, 4.5, 5.9, 180, 520);
        expect (std::abs (mid / (220.0 * std::exp2 (0.5)) - 1.0) < 0.04 && std::abs (top / 440.0 - 1.0) < 0.02 && clean (o.L),
                "rise: +6 st half way, +12 st at the top", juce::String (mid, 1) + " Hz / " + juce::String (top, 1) + " Hz");

        // the widest range: +48 st = four octaves, 220 Hz -> 3520 Hz
        auto s48 = s; s48.riseSemis = 48;
        const auto o48 = run (s48, (int) sr * 6, [] (int i) { return 0.3f * (float) std::sin (twoPi * 220.0 * i / sr); });
        const double top48 = peakHz (o48.L, 4.5, 5.9, 3300, 3700);
        expect (std::abs (top48 / 3520.0 - 1.0) < 0.01 && clean (o48.L), "rise: +48 st reaches four octaves up", juce::String (top48, 1) + " Hz");
    }

    // RISE follows the bar grid: switched on half way through a bar, it waits for the next bar line
    {
        Engine e;
        e.prepare (sr, kBlock);
        juce::AudioBuffer<float> buf (2, kBlock);
        auto s = everythingOff(); s.riseLen = 0; s.riseSemis = 12;
        skazi::HostTime host;                                 // stopped: 120 bpm from beat 0, 4/4
        const int beat = (int) (sr / 2);                       // samples per beat
        float waitingFlag = 0, progressAtBeat5 = 0;
        for (int pos = 0; pos < beat * 6; pos += kBlock)
        {
            s.riseOn = pos >= beat * 3 / 2;                   // switched on at beat 1.5 (past the first quarter)
            buf.clear();
            e.process (buf, s, host);
            if (pos >= beat * 2 && pos < beat * 2 + kBlock) waitingFlag = e.uiRiseProgress.load();
            if (pos >= beat * 5 && pos < beat * 5 + kBlock) progressAtBeat5 = e.uiRiseProgress.load();
        }
        // the rise counts from beat 4 (the next bar); at beat 5 it is 1 of 8 beats in
        expect (waitingFlag < 0.0f && std::abs (progressAtBeat5 - 0.125f) < 0.01f, "rise waits for the bar line, then runs on the grid",
                "beat 2: " + juce::String (waitingFlag, 3) + ", beat 5: " + juce::String (progressAtBeat5, 3));
    }

    // CPU with every effect on
    {
        auto s = fromPreset (salat::presets()[0]);
        s.crushOn = s.ringOn = s.modOn = s.delayOn = s.reverbOn = s.riseOn = true; s.lfoDepth = 2; s.chance = 1; s.riseSemis = 24;
        const int total = (int) sr * 20;
        const auto t0 = std::chrono::steady_clock::now();
        run (s, total, beat);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        expect (secs < 20.0 / 20.0, "CPU: 20 s of audio, everything on", juce::String (secs * 1000.0, 1) + " ms (" + juce::String (20.0 / secs, 0) + "x realtime)");
    }

    if (argc > 1)
    {
        const juce::File dir (juce::String::fromUTF8 (argv[1]));
        dir.createDirectory();
        writeWav (dir.getChildFile ("00 dry beat.wav"), run (everythingOff(), (int) sr * 8, beat));
        const auto& list = salat::presets();
        for (size_t i = 0; i < 8 && i < list.size(); ++i)
            writeWav (dir.getChildFile (juce::String::formatted ("%02d ", (int) i + 1) + list[i].name + ".wav"), run (fromPreset (list[i]), (int) sr * 8, beat));
        juce::Random rng (7);
        for (int i = 0; i < 4; ++i)
            writeWav (dir.getChildFile ("salat " + juce::String (i + 1) + ".wav"), run (fromPreset (salat::randomPreset (rng)), (int) sr * 8, beat));
        std::printf ("rendered to %s\n", dir.getFullPathName().toRawUTF8());
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
