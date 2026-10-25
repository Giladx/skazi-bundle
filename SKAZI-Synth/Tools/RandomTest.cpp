// RANDOM and distortion checks for SKAZI:
//   - every RANDOM press changes every section (oscillators, filter, amp envelope, LFOs, effects)
//   - no RANDOM sound is silent, none clips, all come out at a similar level
//   - every distortion type stays finite, DC-free and inside the limiter at full drive
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <map>

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    int failures = 0;

    void expect (bool ok, const juce::String& what, const juce::String& detail)
    {
        if (! ok) ++failures;
        std::printf ("%-58s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    float value (AcidanceProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    // hot = share of samples in the output limiter's knee (> 0.8), full = share pinned near full scale
    struct Level { double rms = 0.0; float peak = 0.f; double hot = 0.0, full = 0.0; double mean = 0.0; bool finite = true; };

    // Plays the current sound like a user would: a chord (poly / arp), a low note (mono), or C3 for the sequencer
    Level play (AcidanceProcessor& p, double seconds)
    {
        const bool seq = value (p, "seq_on") > 0.5f, arp = value (p, "arp_on") > 0.5f, poly = value (p, "mode") < 0.5f;
        p.setParam ("run", seq ? 1.f : 0.f);
        const int total = (int) (sr * seconds), offAt = (int) (sr * seconds * 0.8);
        juce::AudioBuffer<float> buf (2, block);
        Level r;
        double sum = 0.0, sumX = 0.0;
        long hot = 0, full = 0, n = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            std::vector<int> notes = seq ? std::vector<int> { 60 } : (poly || arp) ? std::vector<int> { 57, 60, 64 } : std::vector<int> { 48 };
            for (int nn : notes)
            {
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
                if (offAt >= pos && offAt < pos + block) midi.addEvent (juce::MidiMessage::noteOff (1, nn), offAt - pos);
            }
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.peak = std::max (r.peak, std::abs (v));
                    sum += (double) v * v; sumX += v; ++n;
                    if (std::abs (v) > 0.8f) ++hot;
                    if (std::abs (v) > 0.98f) ++full;
                }
        }
        p.setParam ("run", 0.f);
        r.rms = std::sqrt (sum / (double) n);
        r.mean = sumX / (double) n;
        r.hot = (double) hot / (double) n;
        r.full = (double) full / (double) n;
        return r;
    }

    // a fingerprint per section, so we can tell whether a press changed it
    juce::String section (AcidanceProcessor& p, std::initializer_list<const char*> ids)
    {
        juce::String s;
        for (auto* id : ids) s << juce::String (value (p, id), 4) << ",";
        return s;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const int presses = argc > 1 ? juce::String (argv[1]).getIntValue() : 300;

    // --- RANDOM ------------------------------------------------------------------------------
    AcidanceProcessor p;
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    const char* sections[] = { "oscillators", "filter", "amp envelope", "LFOs", "effects" };
    auto prints = [&p]
    {
        juce::String lfo;
        for (int l = 1; l <= kNumLfos; ++l)
            for (auto* k : { "_shape", "_rate", "_sync", "_div", "_trig" })
                lfo << juce::String (value (p, ("lfo" + juce::String (l) + k).toRawUTF8()), 4) << ",";
        return std::array<juce::String, 5> {
            section (p, { "osc1_wave", "osc1_wtpos", "osc1_uni", "osc1_detune", "osc1_warp", "osc2_on", "osc2_wave", "osc2_oct", "osc2_fine", "sub_level", "noise_level" }),
            section (p, { "flt1_type", "flt1_cutoff", "flt1_reso", "flt1_env", "flt1_drive", "flt2_type", "env2_d" }),
            section (p, { "env1_a", "env1_d", "env1_s", "env1_r" }),
            lfo,
            section (p, { "dist_on", "dist_type", "dist_drive", "mfx_on", "mfx_type", "dly_on", "dly_div", "rev_on", "rev_mix",
                          "comp_on", "fxf_on", "fxf_type", "wid_on", "eq_on" }) };
    };
    int changed[5] {};
    std::vector<double> levels, pressMs;
    int silent = 0, clipping = 0, broken = 0, arps = 0, seqs = 0;
    float worstPeak = 0.f;
    std::map<int, int> distUse;
    auto last = prints();
    for (int i = 0; i < presses; ++i)
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        p.randomizeSound();
        p.fader.flush();   // no message loop here: apply now
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        pressMs.push_back (ms);
        auto now = prints();
        for (int k = 0; k < 5; ++k) if (now[(size_t) k] != last[(size_t) k]) ++changed[k];
        last = now;
        if (value (p, "arp_on") > 0.5f) ++arps;
        if (value (p, "seq_on") > 0.5f) ++seqs;
        if (value (p, "dist_on") > 0.5f) ++distUse[(int) value (p, "dist_type")];
        p.panicRequest = true;   // start each sound from silence
        const auto r = play (p, 2.5);
        if (! r.finite) ++broken;
        if (r.rms < 0.01) { ++silent; std::printf ("  quiet: %s rms %.4f\n", p.currentPresetName.toRawUTF8(), r.rms); }
        worstPeak = std::max (worstPeak, r.peak);
        if (r.full > 0.001 || r.hot > 0.01) { ++clipping; std::printf ("  hot: %s peak %.3f, %.1f%% over 0.8\n", p.currentPresetName.toRawUTF8(), r.peak, r.hot * 100.0); }
        levels.push_back (20.0 * std::log10 (std::max (r.rms, 1.0e-9)));
    }
    for (int k = 0; k < 5; ++k)
        expect (changed[k] >= presses * 95 / 100, juce::String ("RANDOM changes the ") + sections[k] + " on every press",
                juce::String (changed[k]) + " of " + juce::String (presses));
    std::sort (levels.begin(), levels.end());
    std::sort (pressMs.begin(), pressMs.end());
    expect (pressMs.back() < 800.0, "RANDOM: a press never blocks the UI for long",
            "median " + juce::String (pressMs[pressMs.size() / 2], 0) + " ms, worst " + juce::String (pressMs.back(), 0) + " ms");
    expect (silent == 0, "RANDOM: no silent sounds (all above -40 dBFS RMS)", juce::String (silent) + " quiet");
    expect (clipping == 0 && broken == 0, "RANDOM: nothing clips or breaks",
            juce::String (clipping) + " hot, " + juce::String (broken) + " NaN, loudest single peak " + juce::String (worstPeak, 3));
    const double lo = levels[(size_t) (levels.size() / 20)], hi = levels[(size_t) (levels.size() * 19 / 20)];
    expect (hi - lo < 12.0, "RANDOM: 90% of sounds within 12 dB of each other",
            "5%..95%: " + juce::String (lo, 1) + " .. " + juce::String (hi, 1) + " dBFS, median " + juce::String (levels[levels.size() / 2], 1));
    juce::String du;
    for (auto& [t, n] : distUse) du << Lists::distTypes[t] << " " << n << "  ";
    std::printf ("  arp on in %d, sequencer in %d of %d presses\n  distortion types used: %s\n", arps, seqs, presses, du.toRawUTF8());

    // --- distortion types -----------------------------------------------------------------------
    std::printf ("\n  distortion level vs clean (saw C2, 4x oversampling)\n");
    for (int t = 0; t < Lists::distTypes.size(); ++t)
    {
        juce::String row;
        bool ok = true;
        for (float drive : { 0.3f, 0.7f, 1.f })
        {
            AcidanceProcessor q;
            q.resetAllParams();
            q.setParam ("osc1_wave", 1.f); q.setParam ("flt1_type", 0.f); q.setParam ("oversample", 2.f);
            q.setParam ("env1_s", 1.f); q.setParam ("volume", 0.4f);   // headroom: this measures the distortion, not the master limiter
            q.setPlayConfigDetails (0, 2, sr, block);
            q.prepareToPlay (sr, block);
            const auto clean = play (q, 1.0);
            q.setParam ("dist_on", 1.f); q.setParam ("dist_type", (float) t); q.setParam ("dist_drive", drive); q.setParam ("dist_mix", 1.f);
            q.panicRequest = true;
            const auto d = play (q, 1.0);
            const double db = 20.0 * std::log10 (d.rms / clean.rms);
            row << juce::String (db, 1).paddedLeft (' ', 6) << " dB";
            if (! d.finite || d.full > 0.001 || std::abs (d.mean) > 0.01 || d.rms < 0.01) ok = false;
        }
        expect (ok, "  " + Lists::distTypes[t].paddedRight (' ', 10) + " finite, no DC, under the limiter", "drive .3/.7/1: " + row);
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
