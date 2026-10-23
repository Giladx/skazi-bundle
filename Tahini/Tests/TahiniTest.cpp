// Checks TAHINI's echo timing, feedback stability, reverb decay, freeze, duck and presets.
#include "../Source/TahiniEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace tahini;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-46s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    const double sr = 48000.0;

    // input(i) gives the dry sample; returns the processed L and R
    std::pair<std::vector<float>, std::vector<float>> run (const Settings& s, int total, const std::function<float (int)>& input)
    {
        Engine e;
        e.prepare (sr, 2);
        juce::AudioBuffer<float> buf (2, 256);
        std::vector<float> L, R;
        for (int pos = 0; pos < total; pos += 256)
        {
            for (int i = 0; i < 256; ++i) { const float x = input (pos + i); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            e.process (buf, s, nullptr);
            for (int i = 0; i < 256; ++i) { L.push_back (buf.getSample (0, i)); R.push_back (buf.getSample (1, i)); }
        }
        return { L, R };
    }

    double rms (const std::vector<float>& v, int from, int to)
    {
        double s = 0; to = std::min (to, (int) v.size());
        for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
        return std::sqrt (s / std::max (1, to - from));
    }

    bool clean (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x) || std::abs (x) > 1.0f) return false; return true; }
}

int main()
{
    Settings base = Settings::fromValues ([] (const char* id) { return skazi::defaultValues (paramDefs()).at (id); });
    const auto click = [] (int i) { return i == 0 ? 0.8f : 0.0f; };
    const int T = (int) (0.75 * 60.0 / 120.0 * sr); // 1/8 dotted at 120 bpm

    // echo timing: STEREO mode, delay only
    {
        auto s = base; s.mode = Stereo; s.reverbMix = 0; s.duck = 0; s.dry = false; s.wow = 0; s.feedback = 0.5f; s.delayMix = 1.0f;
        const auto [L, R] = run (s, T * 3, click);
        int peakAt = 0; float best = 0;
        for (int i = T / 2; i < T * 3 / 2; ++i) if (std::abs (L[(size_t) i]) > best) { best = std::abs (L[(size_t) i]); peakAt = i; }
        expect (std::abs (peakAt - T) < 48, "first echo lands on 1/8 dotted", juce::String (peakAt) + " vs " + juce::String (T) + " samples");
    }
    // ping-pong: first echo left, second echo right
    {
        auto s = base; s.mode = PingPong; s.reverbMix = 0; s.duck = 0; s.dry = false; s.wow = 0; s.feedback = 0.6f; s.delayMix = 1.0f;
        const auto [L, R] = run (s, T * 3, click);
        const double l1 = rms (L, T - 200, T + 200), r1 = rms (R, T - 200, T + 200);
        const double l2 = rms (L, 2 * T - 200, 2 * T + 200), r2 = rms (R, 2 * T - 200, 2 * T + 200);
        expect (l1 > r1 * 10 && r2 > l2 * 10, "PING-PONG bounces left then right");
    }
    // stability: max feedback for a long time
    {
        auto s = base; s.feedback = 0.95f; s.drive = 1.0f; s.duck = 0;
        juce::Random rnd (3);
        const auto [L, R] = run (s, (int) (sr * 20), [&rnd] (int i) { return i < 48000 ? (rnd.nextFloat() - 0.5f) : 0.0f; });
        expect (clean (L) && clean (R), "95% feedback stays stable for 20 s");
    }
    // reverb decay time: about -60 dB after DECAY seconds
    {
        auto s = base; s.delayMix = 0; s.duck = 0; s.dry = false; s.decay = 2.0f; s.predelayMs = 0; s.damp = 0;
        const auto [L, R] = run (s, (int) (sr * 2.5), click);
        const double early = rms (L, (int) (0.1 * sr), (int) (0.2 * sr)), late = rms (L, (int) (2.0 * sr), (int) (2.1 * sr));
        const double drop = 20.0 * std::log10 (late / early);
        expect (drop < -45.0 && drop > -75.0, "reverb falls ~60 dB over DECAY (2 s)", juce::String (drop, 1) + " dB");
    }
    // freeze holds the tail
    {
        auto s = base; s.delayMix = 0; s.duck = 0; s.dry = false; s.decay = 1.0f;
        juce::Random rnd (4);
        Engine e; e.prepare (sr, 2);
        juce::AudioBuffer<float> buf (2, 256);
        double before = 0, after = 0;
        for (int pos = 0; pos < (int) (sr * 4); pos += 256)
        {
            for (int i = 0; i < 256; ++i) { const float x = pos < 24000 ? rnd.nextFloat() - 0.5f : 0.0f; buf.setSample (0, i, x); buf.setSample (1, i, x); }
            s.freeze = pos > 26000;
            e.process (buf, s, nullptr);
            const double m = buf.getRMSLevel (0, 0, 256);
            if (pos > 30000 && pos < 40000) before = std::max (before, m);
            if (pos > (int) (sr * 3.5)) after = std::max (after, m);
        }
        expect (after > before * 0.3, "FREEZE keeps the tail ringing", juce::String (before, 4) + " -> " + juce::String (after, 4));
    }
    // duck: wet is pushed down while the dry signal plays
    {
        auto s = base; s.dry = false; s.mode = Stereo;
        const auto tone = [] (int i) { return 0.5f * (float) std::sin (i * 0.05); };
        s.duck = 0.0f;
        const double open = rms (run (s, (int) sr * 2, tone).first, (int) sr, (int) sr * 2);
        s.duck = 1.0f;
        const double ducked = rms (run (s, (int) sr * 2, tone).first, (int) sr, (int) sr * 2);
        expect (ducked < open * 0.3, "DUCK pushes the wet down under the signal", juce::String (open, 3) + " -> " + juce::String (ducked, 3));
    }
    // dry off: no direct sound
    {
        auto s = base; s.dry = false;
        const auto [L, R] = run (s, 512, click);
        expect (std::abs (L[0]) < 1.0e-4f, "DRY off removes the direct sound");
    }

    // presets with a drum-ish input
    const auto& list = presets();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> values;
    int bad = 0;
    for (auto& p : list)
    {
        names.insert (p.name);
        values.insert (p.values);
        const auto s = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
        const auto [L, R] = run (s, (int) (sr * 3), [] (int i) { const int ph = i % 24000; return ph < 2400 ? 0.6f * (float) std::sin (ph * 0.02) * (1.0f - ph / 2400.0f) : 0.0f; });
        if (! (clean (L) && clean (R) && rms (L, 0, (int) L.size()) > 0.005)) ++bad;
    }
    std::printf ("\n%d presets\n", (int) list.size());
    expect (names.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (bad == 0, "every preset processes cleanly", juce::String (bad) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
