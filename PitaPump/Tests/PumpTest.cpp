// Feeds a steady tone through the pump engine and checks the ducking, triggers,
// pre-amp and presets behave.
#include "../Source/PumpEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

namespace
{
    int failures = 0;

    void expect (bool ok, const char* what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-46s %s  %s\n", what, ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    struct Run { float minEnv = 1e9f, maxEnv = 0, peak = 0; bool finite = true; double rms = 0; };

    // amp: input sine amplitude; scHits: sidechain impulse every N samples (0 = none); midiEvery likewise
    Run run (pump::Params p, float amp, double seconds = 4.0, int scEvery = 0, int midiEvery = 0)
    {
        const double sr = 48000.0;
        pump::Engine e;
        e.prepare (sr);
        juce::AudioBuffer<float> buf (2, 480), sc (2, 480);
        std::vector<int> notes;
        Run r;
        long long n = 0, total = (long long) (sr * seconds);
        double sum = 0; long long cnt = 0;
        const int win = 240; // 5 ms envelope windows
        float winPk = 0; int winN = 0;
        while (n < total)
        {
            notes.clear();
            sc.clear();
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const long long k = n + i;
                const float s = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) k / sr);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
                if (scEvery > 0 && k % scEvery < 200) { sc.setSample (0, i, 0.8f); sc.setSample (1, i, 0.8f); }
                if (midiEvery > 0 && k % midiEvery == 0) notes.push_back (i);
            }
            e.process (buf, scEvery > 0 ? &sc : nullptr, notes, p, nullptr);
            for (int i = 0; i < buf.getNumSamples(); ++i, ++n)
            {
                const float v = buf.getSample (0, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                winPk = std::max (winPk, std::abs (v));
                if (++winN == win)
                {
                    if (n > (long long) sr) { r.minEnv = std::min (r.minEnv, winPk); r.maxEnv = std::max (r.maxEnv, winPk); }
                    winPk = 0; winN = 0;
                }
                if (n > (long long) sr) { sum += (double) v * v; ++cnt; }
            }
        }
        r.rms = std::sqrt (sum / (double) std::max (1LL, cnt));
        return r;
    }

    juce::String db (float v) { return juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB"; }
}

int main()
{
    pump::Params p; // SYNC 1/4 at 120 bpm, PUMP shape, 85% depth

    auto r = run (p, 0.5f);
    expect (r.finite && r.peak <= 1.0f, "sync pump: clean output", "peak " + juce::String (r.peak, 2));
    expect (r.minEnv < 0.5f * 0.25f && r.maxEnv > 0.5f * 0.9f, "sync pump: ducks deep and recovers",
            "min " + db (r.minEnv) + "  max " + db (r.maxEnv));

    auto p0 = p; p0.depth = 0.0f;
    r = run (p0, 0.5f);
    expect (r.minEnv > 0.45f, "depth 0: no ducking", "min " + db (r.minEnv));

    for (int s = 0; s < pump::NumShapes; ++s)
    {
        auto ps = p;
        ps.shape = s;
        ps.rate = 6; // 1/16
        r = run (ps, 0.5f, 2.0);
        expect (r.finite && r.peak <= 1.0f && r.maxEnv > 0.3f, (juce::String ("shape ") + pump::shapeName (s)).toRawUTF8(),
                "min " + db (r.minEnv) + "  max " + db (r.maxEnv));
    }

    for (int rate = 0; rate < 16; ++rate)
    {
        auto pr = p;
        pr.rate = rate;
        pr.swing = 0.66f;
        pr.attackMs = 5.0f;
        pr.release = 0.5f;
        r = run (pr, 0.5f, 3.0);
        if (! (r.finite && r.peak <= 1.0f)) expect (false, "rate with swing", pump::rateNames()[rate]);
    }
    expect (true, "all 16 rates with swing/attack/release");

    auto pm = p; pm.trigger = pump::Midi;
    r = run (pm, 0.5f);
    expect (r.minEnv > 0.45f, "MIDI mode, no notes: no ducking", "min " + db (r.minEnv));
    r = run (pm, 0.5f, 4.0, 0, 24000);
    expect (r.minEnv < 0.15f, "MIDI mode, notes: ducks on each note", "min " + db (r.minEnv));

    auto pa = p; pa.trigger = pump::Audio;
    r = run (pa, 0.5f, 4.0, 24000);
    expect (r.minEnv < 0.15f && r.maxEnv > 0.45f, "AUDIO mode: ducks on sidechain hits", "min " + db (r.minEnv) + "  max " + db (r.maxEnv));

    auto pq = p; pq.depth = 0.0f; pq.autoGain = true;
    r = run (pq, 0.01f, 6.0); // -40 dBFS peak tone
    expect (r.rms > 0.08 && r.rms < 0.2, "AUTO pre-amp lifts a quiet signal to ~-18 dB", "rms " + db ((float) r.rms));
    pq.autoGain = false; pq.preGain = juce::Decibels::decibelsToGain (12.0f);
    r = run (pq, 0.01f);
    expect (std::abs (juce::Decibels::gainToDecibels (r.maxEnv) - (-28.0f)) < 1.0f, "PRE GAIN +12 dB", "max " + db (r.maxEnv));
    pq.preGain = juce::Decibels::decibelsToGain (24.0f);
    r = run (pq, 0.9f);
    expect (r.peak <= 1.0f, "hot pre-amp never clips past 0 dBFS", "peak " + juce::String (r.peak, 3));

    // presets
    const auto& list = presets::all();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> settings;
    int bad = 0;
    for (auto& pr : list)
    {
        names.insert (pr.name);
        settings.insert (pr.values);
        pump::Params pp;
        pp.trigger = (int) pr.values.at ("trigger");
        pp.rate = (int) pr.values.at ("rate");
        pp.shape = (int) pr.values.at ("shape");
        pp.depth = pr.values.at ("depth") * 0.01f;
        pp.curve = pr.values.at ("curve") * 0.01f;
        pp.attackMs = pr.values.at ("attack");
        pp.release = pr.values.at ("release") * 0.01f;
        pp.smoothMs = pr.values.at ("smooth");
        pp.swing = pr.values.at ("swing") * 0.01f;
        pp.mix = pr.values.at ("mix") * 0.01f;
        const auto rr = run (pp, 0.5f, 1.5, pp.trigger == pump::Audio ? 24000 : 0, pp.trigger == pump::Midi ? 24000 : 0);
        if (! (rr.finite && rr.peak <= 1.0f && rr.rms > 0.01)) ++bad;
    }
    std::printf ("\n%d presets\n", (int) list.size());
    expect (names.size() == list.size(), "every preset name unique");
    expect (settings.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset sounds right", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
