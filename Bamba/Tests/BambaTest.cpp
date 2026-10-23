// Renders every BAMBA effect type and preset and checks the shapes behave.
#include "../Source/BambaEngine.h"
#include "../Source/Presets.h"
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

    // renders `seconds`; env holds the RMS of each 1/8 of the run
    Run render (const Settings& s, double seconds, bool freeRun, std::vector<int> noteAt = {})
    {
        const double sr = 48000.0;
        Engine e;
        e.prepare (sr);
        e.setFreeRun (freeRun);
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
}

int main()
{
    const char* names[] = { "UPLIFTER", "DOWNLIFTER", "NOISE SWEEP", "IMPACT", "SNARE ROLL", "REVERSE CRASH", "LASER", "SIREN", "SUB DROP", "RANDOM FX" };
    Settings s = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
    s.bars = 1; // 2 s at 120 bpm

    for (int type = 0; type < NumTypes; ++type)
    {
        auto t = s; t.type = type; t.roll = 0.5f;
        const auto r = render (t, 4.0, true);
        expect (r.finite && r.peak <= 1.0f && r.rms > 0.005, juce::String ("SYNC loop: ") + names[type], "rms " + juce::String (r.rms, 3) + "  peak " + juce::String (r.peak, 2));
    }

    // shapes: an uplifter gets louder, a downlifter quieter (dry, one 2-second pass)
    {
        auto t = s; t.reverb = 0; t.delay = 0; t.type = Uplifter; t.curve = 0.6f;
        auto r = render (t, 2.0, true);
        expect (r.env[7] > r.env[0] * 2.0f, "UPLIFTER rises", juce::String (r.env[0], 3) + " -> " + juce::String (r.env[7], 3));
        t.type = Downlifter;
        r = render (t, 2.0, true);
        expect (r.env[0] > r.env[7] * 2.0f, "DOWNLIFTER falls", juce::String (r.env[0], 3) + " -> " + juce::String (r.env[7], 3));
        t.type = ReverseCrash;
        r = render (t, 2.0, true);
        expect (r.env[7] > r.env[2] * 3.0f, "REVERSE CRASH swells into the end", juce::String (r.env[2], 3) + " -> " + juce::String (r.env[7], 3));
    }

    // SYNC without a playing song is silent (inside a DAW)
    {
        const auto r = render (s, 1.0, false);
        expect (r.peak == 0.0f, "SYNC waits for the song to play");
    }

    // MIDI trigger: silent until a note, then plays one pass and stops
    {
        auto t = s; t.trigger = Midi; t.reverb = 0; t.delay = 0;
        auto r = render (t, 1.0, false);
        expect (r.peak == 0.0f, "MIDI mode silent without notes");
        r = render (t, 6.0, false, { 0 });
        expect (r.env[0] > 0.01f && r.env[6] < 1.0e-4f && r.env[7] < 1.0e-4f, "MIDI note plays one 1-bar pass",
                "start " + juce::String (r.env[0], 3) + "  after " + juce::String (r.env[7], 5));
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
        const auto r = render (ps, ps.bars * 2.0 + 0.5, true); // one full pass at 120 bpm
        if (! (r.finite && r.peak <= 1.0f && r.rms > 0.002))
            if (++bad <= 5) std::printf ("  bad preset %s / %s rms %.4f\n", p.category.toRawUTF8(), p.name.toRawUTF8(), r.rms);
    }
    std::printf ("\n%d presets\n", (int) list.size());
    expect (pnames.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset sounds", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
