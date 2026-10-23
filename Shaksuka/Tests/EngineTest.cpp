// Runs drum-like audio through the chop engine in many configurations and checks
// that the output stays finite, bounded and audible.
#include "../Source/ChopEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

namespace
{
    struct Result { float peak = 0; double rms = 0; bool finite = true; };

    Result run (chop::Params p, double sr = 48000.0, double seconds = 12.0)
    {
        chop::Engine e;
        e.prepare (sr);
        juce::AudioBuffer<float> buf (2, 512);
        Result res;
        double sum = 0; long long count = 0;
        long long n = 0;
        const long long total = (long long) (sr * seconds);
        while (n < total)
        {
            for (int i = 0; i < buf.getNumSamples(); ++i, ++n)
            {
                const double t = (double) n / sr;
                const double beat = std::fmod (t * 2.0, 1.0); // 120 bpm kick
                const float s = (float) (0.8 * std::exp (-beat * 18.0) * std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * t)
                                         + 0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * t));
                buf.setSample (0, i, s);
                buf.setSample (1, i, s * 0.9f);
            }
            e.process (buf, p, nullptr);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) res.finite = false;
                    res.peak = std::max (res.peak, std::abs (v));
                    sum += (double) v * v; ++count;
                }
        }
        res.rms = std::sqrt (sum / (double) count);
        return res;
    }

    int failures = 0;

    // Same mapping as ShaksukaProcessor::processBlock
    chop::Params toParams (const std::map<juce::String, float>& v)
    {
        chop::Params p;
        p.freeMode  = v.at ("free") > 0.5f;
        p.grid      = (int) v.at ("grid");
        p.freeMs    = v.at ("freems");
        p.groove    = (int) v.at ("groove");
        p.swing     = v.at ("swing") * 0.01f;
        p.humanize  = v.at ("humanize") * 0.01f;
        p.bars      = 1 << (int) v.at ("bars");
        p.lengthVar = v.at ("lenvar") * 0.01f;
        p.chaos     = v.at ("chaos") * 0.01f;
        p.stack     = (int) v.at ("stack");
        p.evolve    = v.at ("evolve") * 0.01f;
        p.seed      = (int) v.at ("seed");
        p.mix       = v.at ("mix") * 0.01f;
        p.gain      = juce::Decibels::decibelsToGain (v.at ("gain"));
        for (int i = 0; i < chop::NumFx; ++i)
            p.weights[i] = v.at ("on" + juce::String (i)) > 0.5f ? v.at ("w" + juce::String (i)) * 0.01f : 0.0f;
        return p;
    }

    void check (const char* name, const Result& r)
    {
        const bool ok = r.finite && r.peak < 4.0f && r.rms > 0.005;
        if (! ok) ++failures;
        std::printf ("%-28s %s  peak %.2f  rms %.3f%s\n", name, ok ? "OK  " : "FAIL", r.peak, r.rms, r.finite ? "" : "  NaN!");
    }
}

int main()
{
    chop::Params base;
    base.chaos = 1.0f;
    base.stack = 5;
    base.lengthVar = 0.5f;
    for (auto& w : base.weights) w = 0.0f;

    // each effect alone
    for (int fx = 0; fx < chop::NumFx; ++fx)
    {
        auto p = base;
        p.weights[fx] = 1.0f;
        check (chop::fxName (fx), run (p));
    }

    // everything at once, across grids, grooves and modes
    auto all = base;
    for (auto& w : all.weights) w = 1.0f;
    all.evolve = 0.8f;
    all.humanize = 0.5f;
    for (int g = 0; g < 16; ++g)
    {
        auto p = all;
        p.grid = g;
        p.groove = g % 5;
        check (("all fx grid " + chop::gridNames()[g] + " groove " + juce::String (g % 5)).toRawUTF8(), run (p));
    }
    auto fr = all;
    fr.freeMode = true;
    fr.freeMs = 37.0f;
    check ("free mode 37ms", run (fr));
    fr.freeMs = 1800.0f;
    fr.bars = 4;
    check ("free mode 1800ms 4 bars", run (fr, 96000.0, 20.0));

    // presets: count, unique names and settings, all on the grid, all sound right
    const auto& list = presets::all();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> settings;
    int generated = 0, offGrid = 0, presetFails = 0;
    for (auto& pr : list)
    {
        names.insert (pr.name);
        settings.insert (pr.values);
        generated += pr.category != "FACTORY" ? 1 : 0;
        offGrid += pr.values.at ("free") > 0.5f ? 1 : 0;

        const auto r = run (toParams (pr.values), 44100.0, 2.0);
        if (! (r.finite && r.peak < 4.0f && r.rms > 0.005))
        {
            ++presetFails;
            std::printf ("preset FAIL: %s / %s  peak %.2f rms %.3f\n", pr.category.toRawUTF8(), pr.name.toRawUTF8(), r.peak, r.rms);
        }
    }
    juce::Random rnd (99);
    for (int i = 0; i < 50; ++i)
    {
        const auto rp = presets::makeRandom (rnd);
        offGrid += rp.values.at ("free") > 0.5f ? 1 : 0;
        const auto r = run (toParams (rp.values), 44100.0, 2.0);
        presetFails += (r.finite && r.peak < 4.0f && r.rms > 0.005) ? 0 : 1;
    }
    auto expect = [] (bool ok, const char* what) { if (! ok) ++failures; std::printf ("%-44s %s\n", what, ok ? "OK" : "FAIL"); };
    std::printf ("\n%d presets (%d generated)\n", (int) list.size(), generated);
    expect (generated >= 500, "at least 500 generated presets");
    expect (names.size() == list.size(), "every preset name unique");
    expect (settings.size() == list.size(), "every preset's settings different");
    expect (offGrid == 0, "every preset + 50 RANDOM rolls on the grid");
    expect (presetFails == 0, "every preset + RANDOM sounds right");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
