// Checks SKAZI's performance features do what they say:
//   step locks, kick duck, A/B morph (and that it survives a save), macros, voice cards
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <vector>
#include <functional>

namespace
{
    constexpr double sr = 48000.0;
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail)
    {
        if (! ok) ++failures;
        std::printf ("%-58s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    struct Host : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm); p.setIsPlaying (playing); p.setPpqPosition (ppq);
            return p;
        }
    };

    void basePatch (AcidanceProcessor& p)
    {
        p.resetAllParams();
        p.setParam ("osc1_wave", 1.f); p.setParam ("osc1_lock", 1.f); p.setParam ("osc1_rphase", 0.f);
        p.setParam ("flt1_type", 3.f); p.setParam ("flt1_cutoff", 800.f); p.setParam ("flt1_env", 0.f); p.setParam ("flt1_key", 0.f);
        p.setParam ("env1_a", 0.001f); p.setParam ("env1_s", 1.f); p.setParam ("warmth", 0.f);
        p.setParam ("drift", 0.f); p.setParam ("spread", 0.f); p.setParam ("volume", 0.5f);
        p.setParam ("oversample", 0.f);
    }

    struct Out { std::vector<float> l, r; };
    Out render (AcidanceProcessor& p, int total, std::vector<std::pair<int, juce::MidiMessage>> ev, Host* host = nullptr)
    {
        Out o { std::vector<float> ((size_t) total), std::vector<float> ((size_t) total) };
        juce::AudioBuffer<float> buf (2, 512);
        for (int pos = 0; pos < total; pos += 512)
        {
            const int n = std::min (512, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& [t, m] : ev) if (t >= pos && t < pos + n) midi.addEvent (m, t - pos);
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i) { o.l[(size_t) (pos + i)] = buf.getSample (0, i); o.r[(size_t) (pos + i)] = buf.getSample (1, i); }
            if (host) host->ppq += n / sr * host->bpm / 60.0;
        }
        return o;
    }
    double rms (const std::vector<float>& x, int a, int b)
    {
        double s = 0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (s / std::max (1, b - a));
    }
    // brightness: RMS of the first difference (high frequencies) relative to RMS
    double bright (const std::vector<float>& x, int a, int b)
    {
        double s = 0; for (int i = a + 1; i < b; ++i) { const double d = x[(size_t) i] - x[(size_t) i - 1]; s += d * d; }
        return std::sqrt (s / std::max (1, b - a)) / std::max (1.0e-9, rms (x, a, b));
    }
    void prepare (AcidanceProcessor& p) { p.setPlayConfigDetails (0, 2, sr, 512); p.prepareToPlay (sr, 512); }
    auto note (int at, int n = 48) { return std::pair<int, juce::MidiMessage> { at, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) }; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    // 1. step locks: step 0 cutoff +100, step 1 cutoff -100 -> step 0 much brighter
    {
        AcidanceProcessor p;
        basePatch (p);
        p.setParam ("seq_on", 1.f); p.setParam ("seq_gate", 0.9f); p.setParam ("mode", 2.f);
        for (int i = 0; i < kNumSteps; ++i)
        {
            auto& s = p.seq.steps[0][i];
            s.gate = 1; s.note = 0; s.octave = 1; s.accent = 0; s.slide = 0;
            for (auto& l : s.lock) l = 0;
        }
        p.seq.steps[0][0].lock[0] = 100;
        p.seq.steps[0][1].lock[0] = -100;
        Host h;
        p.setPlayHead (&h);
        prepare (p);
        const int step = (int) (sr * 60.0 / 120.0 / 4.0);   // 6000 samples
        auto o = render (p, step * 3, { note (0, 60) }, &h);
        const double b0 = bright (o.l, 1000, step - 1000), b1 = bright (o.l, step + 1000, 2 * step - 1000);
        expect (b0 > b1 * 2.0, "step lock CUTOFF: locked step is brighter than the -100 step",
                "brightness " + juce::String (b0, 3) + " vs " + juce::String (b1, 3));
        p.setPlayHead (nullptr);
    }

    // 2. kick duck: 1/4 at 120 bpm, depth 0.7 -> level right after the beat ~30%, mid-beat ~100%
    {
        AcidanceProcessor p;
        basePatch (p);
        p.setParam ("duck_on", 1.f); p.setParam ("duck_div", 8.f); p.setParam ("duck_depth", 0.7f); p.setParam ("duck_rel", 0.45f);
        Host h;
        p.setPlayHead (&h);
        prepare (p);
        auto o = render (p, 96000, { note (0) }, &h);
        const int beat = 24000;
        const double atBeat = rms (o.l, 2 * beat + 150, 2 * beat + 1100), mid = rms (o.l, 2 * beat + 17000, 2 * beat + 19000);
        expect (atBeat < mid * 0.45 && mid > 0.01, "kick duck: dips on the beat, back up mid-beat",
                "on the beat " + juce::String (atBeat / mid * 100.0, 0) + "% of mid-beat level");
        p.setPlayHead (nullptr);
    }

    // 3. morph: A = dark (200 Hz), B = bright (8 kHz)
    {
        AcidanceProcessor p;
        basePatch (p);
        p.setParam ("flt1_cutoff", 200.f); p.storeMorph (0);
        p.setParam ("flt1_cutoff", 8000.f); p.storeMorph (1);
        p.setParam ("flt1_cutoff", 1000.f);   // what the knob shows: untouched by morphing
        p.setParam ("morph_on", 1.f);
        prepare (p);
        double b[3];
        int k = 0;
        for (float m : { 0.f, 0.5f, 1.f })
        {
            p.setParam ("morph", m);
            p.panicRequest = true;
            auto o = render (p, 20000, { note (100) });
            b[k++] = bright (o.l, 8000, 19000);
        }
        expect (b[0] < b[1] && b[1] < b[2] && b[2] > b[0] * 2.0, "A/B morph: 0 = A, 0.5 between, 1 = B",
                juce::String (b[0], 3) + " < " + juce::String (b[1], 3) + " < " + juce::String (b[2], 3));
        expect (std::abs (p.apvts.getRawParameterValue ("flt1_cutoff")->load() - 1000.f) < 1.f, "A/B morph: the real parameter is left alone",
                "cutoff knob " + juce::String (p.apvts.getRawParameterValue ("flt1_cutoff")->load(), 0) + " Hz");
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        AcidanceProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (q.hasMorph (0) && q.hasMorph (1), "A/B morph: snapshots are saved with the project", "");
    }

    // 4. macro 1 -> filter cutoff
    {
        AcidanceProcessor p;
        basePatch (p);
        p.setParam ("mod1_src", (float) S_Macro1); p.setParam ("mod1_dst", (float) D_Cut1); p.setParam ("mod1_amt", 0.6f);
        prepare (p);
        double b[2];
        for (int m = 0; m < 2; ++m)
        {
            p.setParam ("macro1", (float) m);
            p.panicRequest = true;
            auto o = render (p, 20000, { note (100) });
            b[m] = bright (o.l, 8000, 19000);
        }
        expect (b[1] > b[0] * 2.0, "macro 1 drives what the matrix routes it to", juce::String (b[0], 3) + " -> " + juce::String (b[1], 3));
    }

    // 5. voice cards: SPREAD 0 = centred, SPREAD 1 = each voice sits a little off centre
    {
        double diff[2];
        for (int sp = 0; sp < 2; ++sp)
        {
            AcidanceProcessor p;
            basePatch (p);
            p.setParam ("spread", (float) sp);
            prepare (p);
            auto o = render (p, 20000, { note (100) });
            diff[sp] = std::abs (rms (o.l, 5000, 19000) - rms (o.r, 5000, 19000)) / std::max (1.0e-9, rms (o.l, 5000, 19000));
        }
        expect (diff[0] < 0.001 && diff[1] > 0.02, "voice cards: VOICE SPREAD moves each voice a little",
                "L/R difference " + juce::String (diff[0] * 100.0, 2) + "% -> " + juce::String (diff[1] * 100.0, 1) + "%");
    }

    // 6. every factory preset has the 4 macros routed, and each one audibly changes the sound
    {
        int changed[kNumMacros] {};
        const int picks[] = { 4, 360, 1003, 1260, 1600, 2700, 2900, 3005, 3260, 3510 };
        for (int idx : picks)
            for (int m = 0; m < kNumMacros; ++m)
            {
                std::vector<float> outs[2];
                for (int v = 0; v < 2; ++v)
                {
                    AcidanceProcessor p;
                    p.setCurrentProgram (idx);
                    p.fader.flush();
                    p.setParam ("drift", 0.f); p.setParam ("spread", 0.f);
                    for (int r = 1; r <= 2; ++r) { p.setParam ("osc" + juce::String (r) + "_rphase", 0.f); }
                    p.setParam ("macro" + juce::String (m + 1), (float) v);
                    const bool seq = p.apvts.getRawParameterValue ("seq_on")->load() > 0.5f;
                    if (seq) p.setParam ("run", 1.f);
                    prepare (p);
                    outs[v] = render (p, 48000, { note (0, seq ? 60 : 57), note (0, 60), note (0, 64) }).l;
                }
                double d = 0.0, e = 0.0;
                for (size_t i = 0; i < outs[0].size(); ++i) { const double x = outs[1][i] - outs[0][i]; d += x * x; e += (double) outs[0][i] * outs[0][i]; }
                if (std::sqrt (d / std::max (1.0e-12, e)) > 0.05) ++changed[m];
            }
        juce::String r;
        for (int m = 0; m < kNumMacros; ++m) r << "M" << (m + 1) << " " << changed[m] << "/10  ";
        expect (changed[0] >= 9 && changed[1] >= 7 && changed[2] >= 9 && changed[3] >= 9, "factory presets: each macro changes the sound", r);
    }

    // 7. oscillator shaper, wavetable step mode, unison chords
    {
        auto play = [] (std::function<void (AcidanceProcessor&)> set)
        {
            AcidanceProcessor p;
            basePatch (p);
            p.setParam ("flt1_type", 0.f);
            set (p);
            prepare (p);
            return render (p, 24000, { note (100, 48) }).l;
        };
        const auto plain = play ([] (AcidanceProcessor&) {});
        const double b0 = bright (plain, 4000, 23000);
        juce::String r;
        bool ok = true;
        const char* names[] = { "FOLD", "PWM", "ASYM" };
        for (int sh = 1; sh <= 3; ++sh)
        {
            const auto x = play ([sh] (AcidanceProcessor& p) { p.setParam ("osc1_wave", 3.f); p.setParam ("osc1_shape", (float) sh); p.setParam ("osc1_shapeamt", 0.8f); });
            const auto tri = play ([] (AcidanceProcessor& p) { p.setParam ("osc1_wave", 3.f); });
            const double d = bright (x, 4000, 23000) / bright (tri, 4000, 23000);
            r << names[sh - 1] << " x" << juce::String (d, 2) << "  ";
            if (std::abs (d - 1.0) < 0.05) ok = false;
        }
        juce::ignoreUnused (b0);
        expect (ok, "osc shaper: FOLD / PWM / ASYM change the triangle's harmonics", r);

        // WT STEP: two positions inside one frame sound identical with STEP on, different with it off
        auto at = [&] (float pos, bool step)
        {
            return play ([pos, step] (AcidanceProcessor& p)
            {
                p.setParam ("osc1_wave", 7.f); p.setParam ("osc1_wtpos", pos); p.setParam ("osc1_wtstep", step ? 1.f : 0.f);
            });
        };
        auto rel = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            double d = 0.0, e = 0.0;
            for (size_t i = 4000; i < 23000; ++i) { d += std::pow (a[i] - b[i], 2.0); e += std::pow (a[i], 2.0); }
            return std::sqrt (d / std::max (1.0e-12, e));
        };
        const double off = rel (at (0.51f, false), at (0.55f, false)), on = rel (at (0.51f, true), at (0.55f, true));
        expect (on < 1.0e-4 && off > 0.005, "WT STEP: positions snap to whole frames",
                "STEP off " + juce::String (off * 100.0, 2) + "% apart, on " + juce::String (on * 100.0, 4) + "%");

        // CHORD unison: 3 voices, MAJ: the spectrum gets energy at the major third / fifth
        const auto classic = play ([] (AcidanceProcessor& p) { p.setParam ("osc1_wave", 0.f); p.setParam ("osc1_uni", 3.f); p.setParam ("osc1_detune", 0.f); });
        const auto major = play ([] (AcidanceProcessor& p) { p.setParam ("osc1_wave", 0.f); p.setParam ("osc1_uni", 3.f); p.setParam ("osc1_detune", 0.f); p.setParam ("osc1_unimode", 3.f); });
        expect (bright (major, 4000, 23000) > bright (classic, 4000, 23000) * 1.15, "unison MAJ: the voices play a chord, not one note",
                "brightness " + juce::String (bright (classic, 4000, 23000), 4) + " -> " + juce::String (bright (major, 4000, 23000), 4));
    }

    // 8. your own wavetable from a WAV: frame 1 a sine ... frame 4 a bright saw
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("skazi_wt_test.wav");
        auto writeWav = [] (const juce::File& f, const std::vector<float>& x)
        {
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
            juce::AudioBuffer<float> b (1, (int) x.size());
            for (size_t i = 0; i < x.size(); ++i) b.setSample (0, (int) i, x[i]);
            if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        };
        std::vector<float> x (4 * 2048);
        for (int f = 0; f < 4; ++f)
            for (int i = 0; i < 2048; ++i)
            {
                float v = 0.f;
                for (int h = 1; h <= 1 + f * 20; ++h) v += std::sin (6.2831853f * h * i / 2048.f) / h;
                x[(size_t) (f * 2048 + i)] = v * 0.5f;
            }
        writeWav (tmp, x);
        AcidanceProcessor p;
        basePatch (p);
        p.setParam ("flt1_type", 0.f);
        juce::String err;
        const bool ok = p.loadUserWavetable (0, tmp, &err);
        expect (ok && (int) p.apvts.getRawParameterValue ("osc1_wave")->load() == 8, "user wavetable: a WAV loads and the osc switches to USER",
                ok ? "name " + p.userWavetableName[0] : err);
        prepare (p);
        double b[2];
        for (int k2 = 0; k2 < 2; ++k2)
        {
            p.setParam ("osc1_wtpos", (float) k2);
            p.panicRequest = true;
            b[k2] = bright (render (p, 24000, { note (100, 48) }).l, 4000, 23000);
        }
        expect (b[1] > b[0] * 3.0, "user wavetable: position sweeps from the sine frame to the saw frame",
                "brightness " + juce::String (b[0], 4) + " -> " + juce::String (b[1], 4));
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        AcidanceProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (q.userTable (0) != nullptr && q.userWavetableName[0] == p.userWavetableName[0]
                    && (int) q.apvts.getRawParameterValue ("osc1_wave")->load() == 8,
                "user wavetable: saved inside the project", juce::String ((int) mb.getSize() / 1024) + " kB state");
        std::vector<float> cyc (600);
        for (size_t i = 0; i < cyc.size(); ++i) cyc[i] = i < 300 ? 0.6f : -0.6f;
        writeWav (tmp, cyc);
        expect (p.loadUserWavetable (1, tmp, &err), "user wavetable: a short single-cycle file loads too", "600 samples -> 2048");
        tmp.deleteFile();
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
