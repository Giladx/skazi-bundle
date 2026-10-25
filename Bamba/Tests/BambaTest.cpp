// Renders every BAMBA effect type and preset and checks the shapes behave; checks the Splice
// sample layer, the riser library scan and that old presets and sessions still load.
#include "../Source/BambaEngine.h"
#include "../Source/Presets.h"
#include "../Source/RiserLibrary.h"
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace bamba;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-46s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    struct Run { std::vector<float> env; float peak = 0; bool finite = true; double rms = 0; };

    // renders `seconds`; a note starts at each `noteAt` sample and ends at `offAt` (-1: never);
    // env holds the RMS of each 1/8 of the run
    Run render (const Settings& s, double seconds, std::vector<int> noteAt = {}, int offAt = -1, SamplePtr smp = nullptr)
    {
        const double sr = 48000.0;
        Engine e;
        e.prepare (sr);
        e.setSample (smp);
        juce::AudioBuffer<float> buf (2, 480);
        const int total = (int) (sr * seconds);
        Run r;
        std::vector<double> parts (8, 0.0);
        double sum = 0;
        for (int pos = 0; pos < total; pos += 480)
        {
            juce::MidiBuffer midi;
            for (int at : noteAt)
                if (at >= pos && at < pos + 480) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), at - pos);
            if (offAt >= pos && offAt < pos + 480) midi.addEvent (juce::MidiMessage::noteOff (1, 60), offAt - pos);
            e.render (buf, midi, s, nullptr);
            for (int i = 0; i < 480; ++i)
            {
                const float v = buf.getSample (0, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                sum += (double) v * v;
                parts[(size_t) juce::jmin (7, (pos + i) * 8 / total)] += (double) v * v;
            }
        }
        for (auto& p : parts) r.env.push_back ((float) std::sqrt (p / (total / 8.0)));
        r.rms = std::sqrt (sum / total);
        return r;
    }

    // A noise riser that gets louder over `seconds` (reversed: gets quieter)
    SamplePtr testSample (double seconds, double rate = 44100.0, bool fallInstead = false)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * rate));
        juce::Random rnd (7);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float p = (float) i / b.getNumSamples();
            const float env = 0.05f + 0.95f * (fallInstead ? 1.0f - p : p);
            b.setSample (0, i, (rnd.nextFloat() * 2 - 1) * env * 0.5f);
            b.setSample (1, i, (rnd.nextFloat() * 2 - 1) * env * 0.5f);
        }
        return makeSample (b, rate);
    }

    bool writeWav (const juce::File& f, double seconds, double rate = 44100.0)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::AudioBuffer<float> b (2, juce::jmax (1, (int) (seconds * rate)));
        juce::Random rnd (3);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, (rnd.nextFloat() * 2 - 1) * 0.3f);
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        if (out == nullptr) return false;
        juce::WavAudioFormat wav;
        auto* raw = out.get();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (raw, rate, 2, 16, {}, 0));
        if (w == nullptr) return false;
        out.release(); // the writer owns the stream now
        return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const char* names[] = { "UPLIFTER", "DOWNLIFTER", "NOISE SWEEP", "IMPACT", "SNARE ROLL", "REVERSE CRASH", "LASER", "SIREN", "SUB DROP", "RANDOM FX" };
    Settings s = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
    s.bars = 1; // 2 s at 120 bpm
    expect (s.trigger == Midi, "default: plays while the note is held");
    {
        // PITCH CURVE: 50% is exactly 1.0's pitch path; low climbs early, high saves it for the end
        expect (bendPitch (0.3f, 0.5f) == 0.3f && bendPitch (0.3f, 0.1f) > 0.6f && bendPitch (0.3f, 0.9f) < 0.05f
                    && bendPitch (1.0f, 0.0f) == 1.0f && bendPitch (0.0f, 1.0f) == 0.0f, "PITCH CURVE bends the climb, ends where it should");
        auto t = s; t.trigger = Sync; t.pitchTo = 72; t.tone = 1; t.noise = 0; t.pitchCurve = 0.2f;
        const auto r = render (t, 3.0, { 0 });
        expect (r.finite && r.peak <= 1.0f && r.rms > 0.005, "PITCH TO +72: six octaves up stays clean");
    }
    expect (barsFor (0) == 1 && barsFor (3) == 8 && barsFor (4) == 16 && barsFor (5) == 24, "lengths 1 .. 24 bars, 1.0 indexes unchanged");
    {
        auto t = s; t.trigger = Sync; t.bars = 24; t.reverb = 0; t.delay = 0; // 48 s at 120 bpm
        const auto r = render (t, 50.0, { 0 });
        expect (r.finite && r.env[7] > r.env[0] * 2.0f && r.env[7] > 0.01f, "24 BARS: one long rise", juce::String (r.env[0], 3) + " -> " + juce::String (r.env[7], 3));
    }

    for (int type = 0; type < NumTypes; ++type)
    {
        auto t = s; t.type = type; t.roll = 0.5f; t.trigger = Sync;
        const auto r = render (t, 4.0, { 0, 96000 });
        expect (r.finite && r.peak <= 1.0f && r.rms > 0.005, juce::String ("note fires: ") + names[type], "rms " + juce::String (r.rms, 3) + "  peak " + juce::String (r.peak, 2));
    }

    // shapes: an uplifter gets louder, a downlifter quieter (dry, one 2-second pass)
    {
        auto t = s; t.reverb = 0; t.delay = 0; t.type = Uplifter; t.curve = 0.6f; t.trigger = Sync;
        auto r = render (t, 2.0, { 0 });
        expect (r.env[7] > r.env[0] * 2.0f, "UPLIFTER rises", juce::String (r.env[0], 3) + " -> " + juce::String (r.env[7], 3));
        t.type = Downlifter;
        r = render (t, 2.0, { 0 });
        expect (r.env[0] > r.env[7] * 2.0f, "DOWNLIFTER falls", juce::String (r.env[0], 3) + " -> " + juce::String (r.env[7], 3));
        t.type = ReverseCrash;
        r = render (t, 2.0, { 0 });
        expect (r.env[7] > r.env[2] * 3.0f, "REVERSE CRASH swells into the end", juce::String (r.env[2], 3) + " -> " + juce::String (r.env[7], 3));
    }

    // no notes, no sound - in both modes
    for (int trig : { (int) Sync, (int) Midi })
    {
        auto t = s; t.trigger = trig;
        const auto r = render (t, 2.0);
        expect (r.peak == 0.0f, juce::String ("silent without notes (") + (trig == Sync ? "FULL BARS)" : "HOLD NOTE)"));
    }

    // FULL BARS: a short note still plays the whole pass, then stops
    {
        auto t = s; t.trigger = Sync; t.reverb = 0; t.delay = 0;
        const auto r = render (t, 6.0, { 0 }, 4800);
        expect (r.env[2] > 0.01f && r.env[7] < 1.0e-4f, "FULL BARS: short note plays one full pass",
                "mid " + juce::String (r.env[2], 3) + "  after " + juce::String (r.env[7], 5));
    }

    // HOLD NOTE: stops soon after the note ends
    {
        auto t = s; t.trigger = Midi; t.reverb = 0; t.delay = 0; t.type = NoiseSweep;
        const auto r = render (t, 2.0, { 0 }, 24000); // note held 0.5 s of a 2 s effect
        expect (r.env[0] > 0.0005f && r.env[3] < 1.0e-4f, "HOLD NOTE: stops when the note ends",
                "start " + juce::String (r.env[0], 4) + "  after " + juce::String (r.env[3], 5));
        const auto full = render (t, 2.0, { 0 });
        expect (full.env[5] > 0.01f, "HOLD NOTE: keeps going while held", juce::String (full.env[5], 3));
    }

    // ---- Splice sample layer -------------------------------------------------------------
    {
        auto t = s; t.reverb = 0; t.delay = 0; t.trigger = Sync; t.synth = 0; t.sampleShape = 0; t.sampleLevel = 1;
        // one 2-second pass (1 bar at 120) rendered over 4 s: env parts are 0.5 s each
        t.sampleFit = FitStretch;
        auto r = render (t, 4.0, { 0 }, -1, testSample (0.7));
        expect (r.finite && r.peak <= 1.0f && r.env[0] > 0.005f && r.env[3] > 0.02f && r.env[5] < 1.0e-4f,
                "SAMPLE STRETCH fills the bars, then stops", juce::String (r.env[0], 3) + " .. " + juce::String (r.env[3], 3) + "  after " + juce::String (r.env[5], 5));
        expect (r.env[3] > r.env[0] * 2.0f, "SAMPLE STRETCH keeps the riser's shape");

        t.sampleFit = FitVarispeed;
        r = render (t, 4.0, { 0 }, -1, testSample (5.0, 48000.0));
        expect (r.env[0] > 0.003f && r.env[3] > 0.02f && r.env[5] < 1.0e-4f, "SAMPLE VARISPEED squeezes a long file into the bars");

        t.sampleFit = FitOriginal; t.sampleAlign = AlignEnd;
        r = render (t, 4.0, { 0 }, -1, testSample (1.0));
        expect (r.env[0] < 1.0e-4f && r.env[3] > 0.02f && r.env[5] < 1.0e-4f, "SAMPLE ORIGINAL + END: a short riser lands on the drop",
                juce::String (r.env[0], 5) + " / " + juce::String (r.env[3], 3));

        r = render (t, 4.0, { 0 }, -1, testSample (8.0));
        expect (r.env[0] > 0.01f && r.env[3] > 0.02f && r.env[5] < 1.0e-4f, "SAMPLE ORIGINAL + END: a long riser plays its last bars");

        t.sampleAlign = AlignStart;
        r = render (t, 4.0, { 0 }, -1, testSample (1.0));
        expect (r.env[0] > 0.005f && r.env[2] < 1.0e-4f, "SAMPLE ORIGINAL + START: plays from the note");
        r = render (t, 4.0, { 0 }, -1, testSample (3.0));
        expect (r.env[5] > 0.01f && r.env[7] < 1.0e-4f, "SAMPLE ORIGINAL + START: an impact rings past the bars");

        t.sampleFit = FitStretch; t.sampleReverse = true;
        r = render (t, 4.0, { 0 }, -1, testSample (2.0));
        expect (r.env[0] > r.env[3] * 2.0f, "SAMPLE REVERSE turns a riser into a downlifter",
                juce::String (r.env[0], 3) + " -> " + juce::String (r.env[3], 3));

        t.sampleReverse = false; t.trigger = Midi;
        r = render (t, 4.0, { 0 }, 24000, testSample (2.0));
        expect (r.env[2] < 1.0e-4f, "SAMPLE stops with the note in HOLD NOTE");

        t.trigger = Sync; t.sampleShape = 1.0f; t.type = Uplifter; t.sweep = 1.0f;
        r = render (t, 4.0, { 0 }, -1, testSample (2.0, 44100.0, true));
        expect (r.env[3] > r.env[0], "SAMPLE FOLLOW puts BAMBA's rise on a flat sample",
                juce::String (r.env[0], 4) + " -> " + juce::String (r.env[3], 4));

        // layered: synth + sample together stay clean at full level
        auto lay = s; lay.trigger = Sync; lay.sampleLevel = 1; lay.synth = 1; lay.gain = juce::Decibels::decibelsToGain (6.0f);
        r = render (lay, 4.0, { 0 }, -1, testSample (2.0));
        expect (r.finite && r.peak <= 1.0f, "synth + sample at full level stay under 0 dBFS", "peak " + juce::String (r.peak, 3));

        // a tiny or empty sample does nothing bad
        juce::AudioBuffer<float> empty (2, 0);
        expect (makeSample (empty, 44100.0) == nullptr, "an empty file is not a sample");
        juce::AudioBuffer<float> one (1, 1);
        one.setSample (0, 0, 0.5f);
        r = render (t, 1.0, { 0 }, -1, makeSample (one, 44100.0));
        expect (r.finite, "a one-sample file is harmless");
    }

    // ---- roll quantize and mixer --------------------------------------------------------
    {
        // count the roll's hits: rising edges of a gated tone across one bar (2 s at 120 bpm)
        auto hits = [] (const Settings& t)
        {
            const double sr = 48000.0;
            Engine e;
            e.prepare (sr);
            juce::AudioBuffer<float> buf (2, 480);
            std::vector<float> env;
            for (int pos = 0; pos < 96000; pos += 480)
            {
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
                e.render (buf, midi, t, nullptr);
                for (int i = 0; i < 480; i += 240) // 5 ms blocks: longer than a cycle of the tone
                {
                    float pk = 0;
                    for (int j = 0; j < 240; ++j) pk = std::max (pk, std::abs (buf.getSample (0, i + j)));
                    env.push_back (pk);
                }
            }
            // a hit = rising above 60% of the loudest nearby block after dipping under 25%
            int n = 0;
            bool armed = true;
            const int w = (int) env.size();
            for (int i = 0; i < w; ++i)
            {
                float local = 0;
                for (int j = std::max (0, i - 25); j < std::min (w, i + 25); ++j) local = std::max (local, env[(size_t) j]);
                if (local < 1.0e-3f) continue;
                if (env[(size_t) i] < 0.25f * local) armed = true;
                else if (armed && env[(size_t) i] > 0.6f * local) { ++n; armed = false; }
            }
            return n;
        };
        auto t = s; t.trigger = Sync; t.reverb = 0; t.delay = 0; t.type = Laser; t.roll = 1; t.rollDecay = 0.9f;
        t.type = Uplifter; t.noise = 0; t.tone = 1; t.curve = 0.0f; t.pitchFrom = 0; t.pitchTo = 0; t.sweep = 0;
        t.rollRate = Roll4;
        const int q = hits (t);
        t.rollRate = Roll8;
        const int e8 = hits (t);
        t.rollRate = Roll16;
        const int s16 = hits (t);
        expect (q == 4 && e8 == 8 && s16 == 16, "ROLL 1/4, 1/8, 1/16: 4, 8, 16 hits in a bar",
                juce::String (q) + ", " + juce::String (e8) + ", " + juce::String (s16));
        t.rollRate = RollBuild; t.bars = 4; // 8 s: 2 bars of 1/4... a bar each of 1/4, 1/8, 1/16, 1/32 over 4 bars
        t.rollRate = Roll8; t.rollShift = 1.0f;
        const int shifted = hits (t);
        t.rollShift = 0.0f;
        expect (shifted == 16, "RATE +1 moves 1/8 up to 1/16", juce::String (shifted) + " hits");
        expect (rollHitsPerBeat (Roll2, 0, -6) == 0.5 && rollHitsPerBeat (Roll2, 0, 6) == 32.0 && rollHitsPerBeat (Roll16, 0, 0.4f) == 4.0
                    && rollHitsPerBeat (RollBuild, 0.9f, -2) == 2.0 && rollRateText (Roll8, 2) == "1/32" && rollRateText (RollFree, 1) == "x2.00",
                "RATE steps the grid 1/2 .. 1/128 and stays in range");
        {
            // automating RATE in FREE: the roll speeds up smoothly, no clicks or NaNs
            auto f = s; f.trigger = Sync; f.roll = 1; f.rollRate = RollFree;
            bool ok = true;
            for (float sh : { -6.0f, -2.5f, 0.0f, 1.7f, 6.0f })
            {
                f.rollShift = sh;
                const auto r = render (f, 2.5, { 0 });
                ok = ok && r.finite && r.peak <= 1.0f && r.rms > 0.005;
            }
            expect (ok, "RATE in FREE: -6 .. +6 all clean");
        }
        expect (rollHitsPerBeat (RollBuild, 0.1f) == 1.0 && rollHitsPerBeat (RollBuild, 0.9f) == 8.0
                    && rollHitsPerBeat (Roll2, 0) == 0.5 && rollHitsPerBeat (Roll128, 0) == 32.0, "BUILD steps 1/4 > 1/32; 1/2 .. 1/128 rates");

        // every rate stays clean, with and without a sample
        bool clean = true;
        for (int rate = RollFree; rate <= Roll128; ++rate)
        {
            auto u = s; u.trigger = Sync; u.rollRate = rate; u.roll = 1; u.type = SnareRoll;
            const auto r = render (u, 2.5, { 0 }, -1, testSample (2.0));
            clean = clean && r.finite && r.peak <= 1.0f && r.rms > 0.005;
        }
        expect (clean, "every ROLL rate: clean and audible");

        // the roll gates the sample too
        auto g = s; g.trigger = Sync; g.reverb = 0; g.delay = 0; g.synth = 0; g.sampleShape = 0; g.roll = 1; g.rollRate = Roll4; g.rollDecay = 0.9f;
        const auto gated = render (g, 2.0, { 0 }, -1, testSample (2.0));
        g.roll = 0;
        const auto open = render (g, 2.0, { 0 }, -1, testSample (2.0));
        expect (gated.rms < open.rms * 0.6, "ROLL chops the Splice sample", juce::String (gated.rms, 3) + " vs " + juce::String (open.rms, 3));

        // MIXER: a dry sample has no reverb tail; at 100% it rings like 1.0
        auto m = s; m.trigger = Sync; m.synth = 0; m.reverb = 0.9f; m.delay = 0.6f; m.sampleFx = 0;
        auto dry = render (m, 4.0, { 0 }, -1, testSample (1.0));
        m.sampleFx = 1;
        auto wet = render (m, 4.0, { 0 }, -1, testSample (1.0));
        expect (dry.env[3] > 0.02f && dry.env[6] < 1.0e-4f && wet.env[6] > 0.003f, "SAMPLE FX 0%: no echoes or reverb after it",
                "tail dry " + juce::String (dry.env[6], 5) + " wet " + juce::String (wet.env[6], 4));
        auto y = s; y.trigger = Sync; y.reverb = 0.9f; y.synthFx = 0;
        auto ydry = render (y, 4.0, { 0 });
        expect (ydry.env[2] > 0.02f && ydry.env[6] < 1.0e-4f, "SYNTH FX 0%: synth stays dry");
    }

    // ---- riser library -------------------------------------------------------------------
    {
        struct Case { const char* name; int kind; };
        const Case cases[] = {
            { "SS_3SE_fx_uplifter_mmabatho.wav", RiserLibrary::Rise },
            { "PMAFT_Uplifter_01_D_126BPM.wav", RiserLibrary::Rise },
            { "WHITE_NOISE_SWEEP_UP.wav", RiserLibrary::Rise },
            { "TSP_AEONMODE_174_fx_long_riser_liftoff.wav", RiserLibrary::Rise },
            { "ESM_AMT_fx_risers_one_shot_drop_rise_designed_dead_wind_long_white_noise_woosh_am.wav", RiserLibrary::Rise },
            { "5_Kit_128_G_Build-up.wav", RiserLibrary::Rise },
            { "AA_Woop_FX_Down_Long_4.wav", RiserLibrary::Down },
            { "Downlifter_04.wav", RiserLibrary::Down },
            { "PMAV_Reverse_Cymbal_01.wav", RiserLibrary::Reverse },
            { "MBPAHC_Reverse_Crash_FX_04.wav", RiserLibrary::Reverse },
            { "ZEN_ALL_impact_fx_one_shot_spark.wav", RiserLibrary::Impact },
            { "BRLY_ALVE_impact_subdive.wav", RiserLibrary::Impact },
            { "Tech_Whoosh_02.wav", RiserLibrary::Sweep },
            { "DS_ASPYER_drum_kick_ordinary_night.wav", -1 },
            { "RARE_AMP4_112_perc_Bongo_drop.wav", -1 },
            { "Kit_2_AHT_FX_Loop_Gm_123.wav", -1 },
            { "OS_pad_loop_warm_Cmin.wav", -1 },
            { "BPM65_Gm_Uplifting_Cello_Ensemble.wav", -1 },
        };
        int wrong = 0;
        for (auto& c : cases)
        {
            const int k = RiserLibrary::classify (juce::File ("C:/x/packs/Some Pack/Stuff").getChildFile (c.name));
            if (k != c.kind && ++wrong <= 5) std::printf ("  classify %s -> %d (want %d)\n", c.name, k, c.kind);
        }
        expect (wrong == 0, "names sort into RISE / DOWN / SWEEP / IMPACT / REVERSE", juce::String (wrong) + " wrong");

        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bamba_splice_test");
        tmp.deleteRecursively();
        const auto pack = tmp.getChildFile ("packs").getChildFile ("Psy Pack");
        bool ok = writeWav (pack.getChildFile ("FX/PSY_fx_uplifter_long.wav"), 9.0)
               && writeWav (pack.getChildFile ("FX/PSY_fx_riser_short.wav"), 1.5)
               && writeWav (pack.getChildFile ("FX/PSY_downlifter_01.wav"), 3.0)
               && writeWav (pack.getChildFile ("FX/PSY_impact_boom.wav"), 2.5)
               && writeWav (pack.getChildFile ("FX/PSY_reverse_crash.wav"), 4.5)
               && writeWav (pack.getChildFile ("FX/PSY_white_noise_sweep.wav"), 2.0)
               && writeWav (pack.getChildFile ("Risers/SPLC_042.wav"), 6.0)
               && writeWav (pack.getChildFile ("Drums/PSY_kick_01.wav"), 0.5)
               && writeWav (pack.getChildFile ("Loops/PSY_bass_loop_145.wav"), 3.0);
        pack.getChildFile ("FX/PSY_broken_riser.wav").replaceWithText ("not audio");
        expect (ok, "test samples written");

        const auto found = RiserLibrary::scan (tmp);
        int kinds[RiserLibrary::NumKinds] {}, shortOnes = 0, longOnes = 0;
        bool sorted = true, packOk = true;
        for (size_t i = 0; i < found.size(); ++i)
        {
            ++kinds[found[i].kind];
            if (found[i].matches (-1, RiserLibrary::Short)) ++shortOnes;
            if (found[i].matches (-1, RiserLibrary::Long)) ++longOnes;
            if (i > 0 && found[i].seconds < found[i - 1].seconds) sorted = false;
            if (found[i].pack != "Psy Pack") packOk = false;
        }
        expect (found.size() == 7, "scan finds the 7 FX (no kicks, loops or broken files)", juce::String ((int) found.size()) + " found");
        expect (kinds[RiserLibrary::Rise] == 3 && kinds[RiserLibrary::Down] == 1 && kinds[RiserLibrary::Impact] == 1
                    && kinds[RiserLibrary::Reverse] == 1 && kinds[RiserLibrary::Sweep] == 1, "scan sorts them by kind");
        expect (shortOnes == 4 && longOnes == 3, "SHORT / LONG filters", juce::String (shortOnes) + " short, " + juce::String (longOnes) + " long");
        expect (sorted, "shortest first");
        expect (packOk, "pack name from the Splice folder");
        expect (std::count_if (found.begin(), found.end(), [] (auto& e) { return e.matches (RiserLibrary::Down, RiserLibrary::AnyLength); }) == 1, "kind filter");

        expect (RiserLibrary::scan (tmp.getChildFile ("no such folder")).empty(), "missing Splice folder: nothing found, no crash");
        expect (RiserLibrary::scan (juce::File()).empty(), "no folder at all: nothing found, no crash");
        tmp.getChildFile ("empty").createDirectory();
        expect (RiserLibrary::scan (tmp.getChildFile ("empty")).empty(), "empty folder: nothing found");

        // the plugin: loading, saving and reopening a session with a sample, and with one that's gone
        {
            BambaProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            expect (! proc.loadSample (tmp.getChildFile ("missing.wav")), "a missing file doesn't load");
            expect (! proc.loadSample (pack.getChildFile ("FX/PSY_broken_riser.wav")), "a broken file doesn't load");
            expect (proc.loadSample (pack.getChildFile ("FX/PSY_fx_uplifter_long.wav")),
                    "a Splice riser loads");
            juce::MemoryBlock state;
            proc.getStateInformation (state);

            BambaProcessor reopened;
            reopened.setStateInformation (state.getData(), (int) state.getSize());
            expect (reopened.getSample() != nullptr && reopened.getSampleFile() == proc.getSampleFile(), "a saved session brings its sample back");

            pack.getChildFile ("FX/PSY_fx_uplifter_long.wav").deleteFile();
            BambaProcessor moved;
            moved.setStateInformation (state.getData(), (int) state.getSize());
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
            moved.prepareToPlay (48000.0, 512);
            moved.processBlock (buf, midi);
            expect (moved.getSample() == nullptr && buf.getMagnitude (0, 0, 512) > 0.0f, "sample gone from disk: session opens, synth still plays");

            // An old session (no sample-layer parameters saved) keeps sounding exactly like before
            auto xml = std::make_unique<juce::XmlElement> ("BAMBA");
            for (auto* id : { "type", "bars", "noise" })
            {
                auto* e = xml->createNewChildElement ("PARAM");
                e->setAttribute ("id", id);
                e->setAttribute ("value", juce::String (id) == "type" ? 1 : (juce::String (id) == "bars" ? 2 : 55));
            }
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            BambaProcessor oldProj;
            oldProj.setStateInformation (old.getData(), (int) old.getSize());
            const auto os = oldProj.settings();
            expect (os.type == Downlifter && os.bars == 4 && std::abs (os.noise - 0.55f) < 0.001f && os.synth == 1.0f && oldProj.getSample() == nullptr,
                    "an old BAMBA session loads with the synth at full level");
        }
        tmp.deleteRecursively();
    }

    // presets
    const auto& list = presets();
    std::set<juce::String> pnames;
    std::set<std::map<juce::String, float>> values;
    int bad = 0;
    for (auto& p : list)
    {
        pnames.insert (p.name);
        values.insert (p.values);
        auto ps = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
        ps.trigger = Sync;
        const auto r = render (ps, ps.bars * 2.0 + 0.5, { 0 }); // one full pass at 120 bpm
        if (! (r.finite && r.peak <= 1.0f && r.rms > 0.002))
            if (++bad <= 5) std::printf ("  bad preset %s / %s rms %.4f\n", p.category.toRawUTF8(), p.name.toRawUTF8(), r.rms);
    }
    std::printf ("\n%d presets\n", (int) list.size());

    // every preset name from BAMBA 1.0 is still there
    {
        int missing = 0;
        juce::StringArray old { "Init", "8 Bar Rocket", "Drop Impact", "Snare Build", "Wash Down", "Laser Rain", "Reverse Wash", "Sub Bomb" };
        for (int g = 0; g < 140; ++g) old.add (skazi::presetName (g + 200));
        for (auto& n : old)
            if (! pnames.count (n) && ++missing <= 3) std::printf ("  missing preset %s\n", n.toRawUTF8());
        expect (missing == 0, "every 1.0 preset name still loads", juce::String (missing) + " missing");

        juce::StringArray folders;
        for (auto& p : list) folders.addIfNotAlreadyThere (p.category);
        bool all = true;
        for (auto* f : { "SNARE ROLLS", "QUANTIZED UPLIFTS", "SHORT RISES", "LONG RISES", "FX: DOWNLIFTERS", "FX: LASER + SIREN", "NOISE SWEEPS", "PITCHED UPLIFTS", "FILTERED BUILDS", "REVERSE SWELLS",
                         "FX: IMPACTS", "PSYTRANCE", "TECHNO", "HOUSE", "SPLICE LAYER" })
            all = all && folders.contains (f);
        expect (all, "new preset folders", folders.joinIntoString (", "));

        // every non-uplift sound lives in an FX folder, and those come last
        bool fxLast = true, fxSorted = true, seenFx = false;
        for (auto& p : list)
        {
            const bool fx = p.category.startsWith ("FX:");
            if (seenFx && ! fx) fxLast = false;
            seenFx = seenFx || fx;
            const int t = (int) p.values.at ("type");
            const bool notUp = t == Downlifter || t == Impact || t == SubDrop || t == Laser || t == Siren || t == RandomFx;
            if (notUp != fx && p.name != "Init") fxSorted = false;
        }
        expect (fxLast && fxSorted, "impacts, downlifters and other FX: own section at the end");
    }

    // SPLICE LAYER presets with a sample loaded: clean and audible
    {
        int badSplice = 0, n = 0;
        const auto smp = testSample (5.0);
        for (auto& p : list)
        {
            if (p.category != "SPLICE LAYER") continue;
            ++n;
            auto ps = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
            ps.trigger = Sync;
            const auto r = render (ps, ps.bars * 2.0 + 1.0, { 0 }, -1, smp);
            if (! (r.finite && r.peak <= 1.0f && r.rms > 0.002))
                if (++badSplice <= 5) std::printf ("  bad splice preset %s rms %.4f peak %.3f\n", p.name.toRawUTF8(), r.rms, r.peak);
        }
        expect (n >= 12 && badSplice == 0, "SPLICE LAYER presets with a sample: clean and audible", juce::String (n) + " presets, " + juce::String (badSplice) + " bad");
    }
    expect (pnames.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset sounds", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
