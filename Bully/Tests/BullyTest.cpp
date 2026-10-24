// Renders every BULLY preset and a pile of FULL RANDOM sounds and checks they behave.
#include "../Source/Engine.h"
#include "../Source/Presets.h"
#include <chrono>
#include <cstdio>
#include <set>

using namespace bully;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-50s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    std::vector<float> valuesOf (const std::map<juce::String, float>& m)
    {
        std::vector<float> v;
        for (auto& d : paramDefs()) v.push_back (m.at (d.id));
        return v;
    }

    struct Result { float peak = 0; double rms = 0; bool finite = true; double seconds = 0; std::vector<float> left; };

    // plays notes (chord or one note) for `holdSec`, then lets go
    Result render (const std::vector<float>& v, std::vector<int> notes, double holdSec, double totalSec, bool keep = false)
    {
        const double sr = 48000.0;
        const int block = 256;
        auto engine = std::make_unique<Engine>(); // far too big for the stack
        auto& e = *engine;
        e.prepare (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        Result r;
        const int total = (int) (sr * totalSec), off = (int) (sr * holdSec);
        double sum = 0;
        skazi::HostTime ht;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            if (pos == 0) for (int n : notes) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.9f), 0);
            if (off >= pos && off < pos + block) for (int n : notes) midi.addEvent (juce::MidiMessage::noteOff (1, n), off - pos);
            e.render (buf, midi, v.data(), ht);
            for (int i = 0; i < block; ++i)
            {
                const float l = buf.getSample (0, i), rr = buf.getSample (1, i);
                if (! std::isfinite (l) || ! std::isfinite (rr)) r.finite = false;
                r.peak = std::max ({ r.peak, std::abs (l), std::abs (rr) });
                sum += 0.5 * ((double) l * l + (double) rr * rr);
                if (keep) r.left.push_back (l);
            }
        }
        r.seconds = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        r.rms = std::sqrt (sum / total);
        return r;
    }

    // rough pitch by counting upward zero crossings
    double pitchOf (const std::vector<float>& x, int from, int to, double sr)
    {
        int c = 0;
        for (int i = from + 1; i < to; ++i) if (x[(size_t) i - 1] < 0 && x[(size_t) i] >= 0) ++c;
        return c * sr / (to - from);
    }
}

int main (int argc, char** argv)
{
    WaveBank::get();
    if (argc > 1 && juce::String (argv[1]) == "--levels") // measure every preset and write Source/PresetLevels.h
    {
        juce::String out ("#pragma once\n// Written by BullyTest --levels: each preset's VOLUME so it peaks near -6 dB\n#define BULLY_HAS_PRESET_LEVELS 1\nstatic const float kPresetVolume[] = {\n");
        int i = 0;
        for (auto& p : presets())
        {
            const bool bassish = p.category.contains ("BASS") || p.category == "REESE" || p.category == "GROWL";
            const bool slow = p.category == "PAD" || p.category == "FX RISER" || p.category == "CHAOS";
            const double hold = slow ? 3.0 : 0.8;
            const auto r = render (valuesOf (p.values), bassish ? std::vector<int> { 36 } : std::vector<int> { 60, 64, 67 }, hold, hold + 0.8);
            float vol = p.values.at ("vol") + juce::Decibels::gainToDecibels (0.45f / juce::jmax (0.01f, r.peak));
            vol = juce::jlimit (-30.0f, 12.0f, std::round (vol * 10.0f) / 10.0f);
            out << "    " << juce::String (vol, 1) << "f," << ((++i % 12) == 0 ? "\n" : "");
        }
        out << "\n};\n";
        juce::File (argv[2]).replaceWithText (out);
        std::printf ("wrote %d levels\n", i);
        return 0;
    }
    const auto defaults = valuesOf (skazi::defaultValues (paramDefs()));

    // init sound plays at the right pitch
    {
        auto v = defaults;
        v[(size_t) pid ("oatab")] = 0; v[(size_t) pid ("oapos")] = 0; // pure sine
        v[(size_t) pid ("f1on")] = 0;
        const auto r = render (v, { 69 }, 1.0, 1.0, true);
        const double hz = pitchOf (r.left, 12000, 36000, 48000.0);
        expect (std::abs (hz - 440.0) < 3.0, "A4 sine plays at 440 Hz", juce::String (hz, 1) + " Hz");
    }

    // every table, every warp: finite and not silent at a high note (aliasing guard keeps it clean)
    {
        int bad = 0;
        for (int t = 0; t < kTables; ++t)
            for (int w = 0; w < NumWarps; ++w)
            {
                auto v = defaults;
                v[(size_t) pid ("oatab")] = (float) t; v[(size_t) pid ("oapos")] = 60;
                v[(size_t) pid ("oaw1t")] = (float) w; v[(size_t) pid ("oaw1")] = 70;
                v[(size_t) pid ("obon")] = 1;
                const auto r = render (v, { 96 }, 0.3, 0.4);
                if (! r.finite || r.peak > 1.0f || r.rms < 0.003) if (++bad <= 4) std::printf ("  table %d warp %d rms %.4f peak %.2f\n", t, w, r.rms, r.peak);
            }
        expect (bad == 0, "12 tables x 10 warps sound", juce::String (bad) + " bad");
    }

    // every filter type
    {
        int bad = 0;
        for (int f = 0; f < NumFilterTypes; ++f)
        {
            auto v = defaults;
            v[(size_t) pid ("f1type")] = (float) f; v[(size_t) pid ("f1res")] = 80; v[(size_t) pid ("f1cut")] = 800; v[(size_t) pid ("f1drv")] = 50;
            const auto r = render (v, { 48, 55, 60 }, 0.5, 0.7);
            if (! r.finite || r.peak > 1.0f || r.rms < 0.002) if (++bad <= 4) std::printf ("  filter %d rms %.4f peak %.2f\n", f, r.rms, r.peak);
        }
        expect (bad == 0, "13 filter types sound at high resonance", juce::String (bad) + " bad");
    }

    // the psy recipe: ENV 2 on pitch gives a click that settles back to the note
    {
        const auto& list = presets();
        const auto it = std::find_if (list.begin(), list.end(), [] (const skazi::Preset& p) { return p.name == "Bully Roller"; });
        expect (it != list.end(), "Bully Roller preset exists");
        if (it != list.end())
        {
            auto v = valuesOf (it->values);
            const auto r = render (v, { 45 }, 0.12, 0.2, true);
            expect (r.finite && r.peak <= 1.0f && r.rms > 0.01, "Bully Roller plays", "rms " + juce::String (r.rms, 3));
            const auto r2 = render (v, { 45 }, 0.12, 0.2, true);
            bool same = r.left.size() == r2.left.size();
            for (size_t i = 0; same && i < r.left.size(); ++i) same = std::abs (r.left[i] - r2.left[i]) < 1.0e-6f;
            expect (same, "phase-locked: every hit identical");
        }
    }

    // presets
    const auto& list = presets();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> distinct;
    int bad = 0;
    double cpu = 0, audio = 0;
    std::map<juce::String, float> catPeak;
    std::map<juce::String, int> catCount;
    int plain = 0; // presets without a warp or with fewer than 3 FX
    for (auto& p : list)
    {
        names.insert (p.category + "/" + p.name);
        distinct.insert (p.values);
        const auto v = valuesOf (p.values);
        const bool bassish = p.category.contains ("BASS") || p.category == "REESE" || p.category == "GROWL";
        const bool slow = p.category == "PAD" || p.category == "FX RISER" || p.category == "CHAOS";
        const double hold = slow ? 3.0 : 0.8, total = hold + 0.8;
        const auto r = render (v, bassish ? std::vector<int> { 36 } : std::vector<int> { 60, 64, 67 }, hold, total);
        cpu += r.seconds; audio += total;
        catPeak[p.category] += r.peak; catCount[p.category]++;
        if (p.category != "FACTORY")
        {
            int fx = 0;
            for (const char* id : { "hyon", "dson", "moon", "fson", "oton", "eqon", "dlon", "rvon" }) fx += p.values.at (id) > 0.5f ? 1 : 0;
            if (fx < 3 || (int) p.values.at ("oaw1t") == WNone) ++plain;
        }
        if (! (r.finite && r.peak < 0.99f && r.peak > 0.15f))
            if (++bad <= 8) std::printf ("  bad preset %s / %s rms %.4f peak %.2f\n", p.category.toRawUTF8(), p.name.toRawUTF8(), r.rms, r.peak);
    }
    for (auto& [cat, sum] : catPeak) std::printf ("  %-14s average peak %.2f\n", cat.toRawUTF8(), sum / catCount[cat]);
    std::printf ("\n%d presets, %.1f%% of real time per preset (one core)\n", (int) list.size(), 100.0 * cpu / audio);
    expect (names.size() == list.size(), "every preset name unique in its folder");
    expect (distinct.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset sounds, no overs", juce::String (bad) + " bad");
    expect (plain == 0, "every style preset: warp + 3 or more FX", juce::String (plain) + " plain");

    // FULL RANDOM
    {
        juce::Random rng (42);
        int rbad = 0, quiet = 0;
        std::set<std::map<juce::String, float>> seen;
        for (int i = 0; i < 150; ++i)
        {
            const auto p = randomPreset (rng);
            seen.insert (p.values);
            const auto r = render (valuesOf (p.values), { 48, 60, 67 }, 0.6, 1.0);
            if (! r.finite || r.peak > 1.0f) if (++rbad <= 4) std::printf ("  random %d peak %.2f finite %d\n", i, r.peak, (int) r.finite);
            if (r.rms < 0.002) ++quiet;
        }
        expect (rbad == 0, "150 FULL RANDOM sounds finite, no overs", juce::String (rbad) + " bad");
        expect (quiet <= 6, "FULL RANDOM almost never silent", juce::String (quiet) + " quiet of 150");
        expect (seen.size() == 150, "FULL RANDOM never repeats");
    }

    // 16 voices of a heavy patch stay real-time
    {
        auto v = defaults;
        for (const char* o : { "a", "b", "c" })
        {
            v[(size_t) pid (std::string ("o") + o + "on")] = 1; v[(size_t) pid (std::string ("o") + o + "uni")] = 8;
            v[(size_t) pid (std::string ("o") + o + "w1t")] = WSync; v[(size_t) pid (std::string ("o") + o + "w1")] = 50;
        }
        v[(size_t) pid ("poly")] = 16; v[(size_t) pid ("f2on")] = 1;
        for (const char* fx : { "hyon", "dson", "moon", "oton", "dlon", "rvon" }) v[(size_t) pid (fx)] = 1;
        std::vector<int> chord; for (int i = 0; i < 16; ++i) chord.push_back (40 + i * 3);
        const auto r = render (v, chord, 2.0, 2.0);
        expect (r.seconds < 2.0 * 0.5, "16 voices x 3 osc x 8 unison + FX under 50% CPU", juce::String (100.0 * r.seconds / 2.0, 1) + "%");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
