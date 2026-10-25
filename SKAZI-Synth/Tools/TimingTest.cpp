// Timing checks for SKAZI: is the sound where the DAW expects it?
//   1. reported plugin latency matches the real delay at every oversampling factor
//   2. notes start at their exact sample offset inside a block
//   3. host blocks bigger than the prepared size don't cut notes or shift timing
//   4. synced LFOs sit on the beat grid (host transport and RUN)
//   5. the step sequencer starts on the grid, and a key that lands just after a step boundary
//      still plays that step instead of waiting a whole 16th
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <vector>
#include <thread>
#include <algorithm>

namespace
{
    constexpr double sr = 48000.0;

    struct Host : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 145.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setIsPlaying (playing);
            p.setPpqPosition (ppq);
            return p;
        }
    };

    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail)
    {
        if (! ok) ++failures;
        std::printf ("%-62s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    // A bare, instant test tone: phase-locked saw, fastest attack, no filter, no warmth, no FX
    void cleanPatch (AcidanceProcessor& p)
    {
        p.resetAllParams();
        p.setParam ("osc1_wave", 1.f); p.setParam ("osc1_lock", 1.f); p.setParam ("osc1_rphase", 0.f);
        p.setParam ("osc1_level", 0.8f);
        p.setParam ("flt1_type", 0.f); p.setParam ("flt2_type", 0.f); p.setParam ("warmth", 0.f);
        p.setParam ("env1_a", 0.001f); p.setParam ("env1_d", 1.f); p.setParam ("env1_s", 1.f); p.setParam ("env1_r", 0.005f);
        p.setParam ("volume", 0.5f);
        p.setParam ("drift", 0.f);   // no analog drift: the same note every time
        p.setParam ("seq_on", 0.f);
    }

    // Renders `total` samples in blocks of `block`; events are (absolute sample, message).
    // The result is shifted by the reported latency, like a DAW with delay compensation plays it.
    std::vector<float> render (AcidanceProcessor& p, int total, int block,
                               const std::vector<std::pair<int, juce::MidiMessage>>& events, Host* host = nullptr)
    {
        std::vector<float> out ((size_t) total, 0.f);
        juce::AudioBuffer<float> buf (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& [t, m] : events)
                if (t >= pos && t < pos + n) midi.addEvent (m, t - pos);
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = buf.getSample (0, i);
            if (host != nullptr) host->ppq += n / sr * host->bpm / 60.0;
        }
        const int lat = p.getLatencySamples();
        out.erase (out.begin(), out.begin() + std::min (lat, total));
        out.resize ((size_t) total, 0.f);
        return out;
    }

    // low threshold: the fastest attack (0.5 ms) crosses it on the first sample
    int onset (const std::vector<float>& x, int from = 0, float thr = 0.002f)
    {
        for (int i = from; i < (int) x.size(); ++i)
            if (std::abs (x[(size_t) i]) > thr) return i;
        return -1;
    }

    // lag (samples) that best lines b up with a, by cross-correlation over [from, to)
    int lagOf (const std::vector<float>& a, const std::vector<float>& b, int from, int to)
    {
        int best = 0;
        double bestDot = -1.0e30;
        for (int lag = -12; lag <= 12; ++lag)
        {
            double dot = 0.0;
            for (int i = from; i < to; ++i) dot += (double) a[(size_t) i] * b[(size_t) (i + lag)];
            if (dot > bestDot) { bestDot = dot; best = lag; }
        }
        return best;
    }

    // short-window RMS
    float rmsAt (const std::vector<float>& x, int at, int len)
    {
        double s = 0.0;
        int n = 0;
        for (int i = std::max (0, at); i < std::min ((int) x.size(), at + len); ++i, ++n) s += (double) x[(size_t) i] * x[(size_t) i];
        return n > 0 ? (float) std::sqrt (s / n) : 0.f;
    }

    void prepare (AcidanceProcessor& p, int block)
    {
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const int noteAt = 3000;

    // 1 + 2: latency and in-block note offsets ------------------------------------------
    // Each factor is compared with plain 1x by waveform cross-correlation after the host's delay
    // compensation: any lag left is a delay the DAW doesn't know about.
    std::vector<float> ref;
    for (int os = 0; os < 4; ++os)
    {
        AcidanceProcessor p;
        cleanPatch (p);
        p.setParam ("oversample", (float) os);
        prepare (p, 512);
        render (p, 4096, 512, {});   // let the oversampling filters settle
        const int reported = p.getLatencySamples();
        auto x = render (p, 12000, 512, { { noteAt + 300, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) } });
        if (os == 0)
        {
            ref = x;
            const int on = onset (x);
            expect (std::abs (on - (noteAt + 300)) <= 1, "note-on at sample 300 of a block starts at sample 300",
                    "starts " + juce::String (on - (noteAt + 300)) + " smp after its offset (latency " + juce::String (reported) + " compensated)");
            continue;
        }
        const int lag = lagOf (ref, x, noteAt + 600, noteAt + 6000);
        expect (std::abs (lag) <= 1, "oversampling " + juce::String (1 << os) + "x: no delay beyond the reported latency",
                "reported " + juce::String (reported) + " smp, still late by " + juce::String (lag) + " smp");
    }

    // 3: blocks bigger than prepared -----------------------------------------------------
    {
        AcidanceProcessor p;
        cleanPatch (p);
        p.setParam ("oversample", 2.f);   // 4x
        prepare (p, 256);
        auto a = render (p, 4096, 256, { { 1000, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) } });
        // the host now sends 2048-sample blocks (offline export, FL Studio, ...): the held note must keep going
        auto b = render (p, 8192, 2048, {});
        const float before = rmsAt (a, 3000, 1000), after = rmsAt (b, 6000, 2000);
        expect (after > before * 0.7f, "bigger host block than prepared: held note keeps playing",
                "rms before " + juce::String (before, 3) + ", after " + juce::String (after, 3));
        // and a note in such a big block lands on time
        AcidanceProcessor q;
        cleanPatch (q);
        q.setParam ("oversample", 2.f);
        prepare (q, 256);
        render (q, 4096, 256, {});
        q.reset();
        auto c = render (q, 8192, 2048, { { 2048 + 700, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) } });
        AcidanceProcessor r0;
        cleanPatch (r0);
        r0.setParam ("oversample", 0.f);
        prepare (r0, 256);
        auto d = render (r0, 8192, 2048, { { 2048 + 700, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) } });
        const int off = onset (c) - onset (d);
        expect (std::abs (off) <= 1, "note inside a 2048 block (prepared 256) lands on time",
                "off by " + juce::String (off) + " smp after latency compensation");
    }

    // 4: synced LFO on the beat grid -----------------------------------------------------
    for (int useRun = 0; useRun < 2; ++useRun)
    {
        AcidanceProcessor p;
        cleanPatch (p);
        p.setParam ("oversample", 0.f);
        p.setParam ("lfo1_shape", 4.f);            // square
        p.setParam ("lfo1_sync", 1.f); p.setParam ("lfo1_div", 8.f);   // 1/4
        p.setParam ("mod1_src", (float) S_Lfo1); p.setParam ("mod1_dst", (float) D_Amp); p.setParam ("mod1_amt", 1.f);
        Host host;
        host.bpm = 120.0;                           // 1 beat = 24000 samples
        if (useRun) { host.playing = false; p.setParam ("run", 1.f); p.setParam ("bpm", 120.f); }
        else host.ppq = 7.37;                       // song already running, odd position
        p.setPlayHead (&host);
        prepare (p, 512);
        auto x = render (p, 96000, 512, { { 0, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) } }, &host);
        // the gate opens (square goes high) on every beat: find the first rising edge after 0.25 s
        int edge = -1;
        for (int i = 12000; i < 90000; ++i)
            if (rmsAt (x, i - 200, 200) < 0.005f && std::abs (x[(size_t) i]) > 0.02f) { edge = i; break; }
        const double startPpq = useRun ? 0.0 : 7.37;
        const double beatPos = startPpq + edge / 24000.0;
        const double err = (beatPos - std::round (beatPos)) * 24000.0;
        expect (edge > 0 && std::abs (err) <= 48.0,
                juce::String (useRun ? "RUN clock" : "host transport") + ": synced 1/4 LFO gate opens on the beat",
                edge > 0 ? "off by " + juce::String (err, 1) + " smp" : juce::String ("no edge found"));
        p.setPlayHead (nullptr);
    }

    // 5: step sequencer on the grid --------------------------------------------------------
    auto seqPatch = [] (AcidanceProcessor& p)
    {
        cleanPatch (p);
        p.setParam ("oversample", 0.f);
        p.setParam ("seq_on", 1.f);
        p.setParam ("seq_gate", 0.5f);
        p.setParam ("mode", 2.f);
        for (int i = 0; i < kNumSteps; ++i)
        {
            auto& s = p.seq.steps[0][i];
            s.gate = 1; s.note = 0; s.octave = 1; s.accent = 0; s.slide = 0;
        }
    };
    {
        // 145 bpm: a 16th = 4965.5 samples. Song at ppq 3.9, so step 16 (ppq 4.0) starts 1986 samples in.
        const double bpm = 145.0, startPpq = 3.9;
        const double samplesPer16 = sr * 60.0 / bpm / 4.0;
        const int boundary = (int) std::ceil ((4.0 - startPpq) * 4.0 * samplesPer16);
        struct Case { const char* what; int keyAt; int expectAt; int tol; };
        const Case cases[] = {
            { "seq: key 20 smp before a step boundary plays on the boundary", boundary - 20, boundary, 2 },
            { "seq: key 10 smp after a step boundary plays at once, not 1/16 late", boundary + 10, boundary + 10, 2 },
            { "seq: key 30% into a step plays at once", boundary + (int) (samplesPer16 * 0.3), boundary + (int) (samplesPer16 * 0.3), 2 },
        };
        for (auto& c : cases)
        {
            AcidanceProcessor p;
            seqPatch (p);
            Host host;
            host.bpm = bpm; host.ppq = startPpq;
            p.setPlayHead (&host);
            prepare (p, 512);
            auto x = render (p, 30000, 512, { { c.keyAt, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } }, &host);
            const int on = onset (x);
            expect (on >= 0 && std::abs (on - c.expectAt) <= c.tol, c.what,
                    on >= 0 ? "sounds " + juce::String (on - c.expectAt) + " smp from where it should ("
                                  + juce::String ((on - c.expectAt) / sr * 1000.0, 1) + " ms)"
                            : juce::String ("silent"));
                        p.setPlayHead (nullptr);
        }

        // steady pattern: every gated step starts on its 16th
        AcidanceProcessor p;
        seqPatch (p);
        for (int i = 0; i < kNumSteps; ++i) p.seq.steps[0][i].gate = (int8_t) (i % 2 == 0);
        p.setParam ("seq_gate", 0.4f);
        Host host;
        host.bpm = bpm; host.ppq = 0.0;
        p.setPlayHead (&host);
        prepare (p, 480);
        auto x = render (p, 100000, 480, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } }, &host);
        double worst = 0.0;
        int found = 0;
        for (int step = 2; step < 18; step += 2)
        {
            const int expectAt = (int) std::ceil (step * samplesPer16);
            const int on = onset (x, expectAt - 600);
            if (on < 0) break;
            ++found;
            worst = std::max (worst, (double) std::abs (on - expectAt));
        }
        expect (found == 8 && worst <= 2.0, "seq: steady pattern, every step on its 16th",
                juce::String (found) + " steps, worst " + juce::String (worst, 0) + " smp");
        p.setPlayHead (nullptr);
    }

    // 6: arpeggiator on the grid ---------------------------------------------------------
    {
        const double bpm = 145.0, startPpq = 3.9;
        const double samplesPer16 = sr * 60.0 / bpm / 4.0;
        const int boundary = (int) std::ceil ((4.0 - startPpq) * 4.0 * samplesPer16);
        auto arpPatch = [] (AcidanceProcessor& p)
        {
            cleanPatch (p);
            p.setParam ("oversample", 0.f);
            p.setParam ("arp_on", 1.f); p.setParam ("arp_mode", (float) ArpUp);
            p.setParam ("arp_div", 2.f); p.setParam ("arp_gate", 0.5f); p.setParam ("arp_oct", 2.f);
        };
        auto chord = [] (int at)
        {
            return std::vector<std::pair<int, juce::MidiMessage>> {
                { at, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) },
                { at, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100) },
                { at, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100) } };
        };
        auto check = [] (const std::vector<float>& x, int first, double stepLen, const juce::String& what)
        {
            const int on = onset (x);
            double worst = std::abs (on - first);
            int found = on >= 0 ? 1 : 0;
            for (int k = 1; k <= 8 && on >= 0; ++k)
            {
                const int expectAt = (int) std::ceil (first + k * stepLen);
                const int o = onset (x, expectAt - 600);
                if (o < 0) break;
                ++found;
                worst = std::max (worst, (double) std::abs (o - expectAt));
            }
            expect (found == 9 && worst <= 2.0, what, juce::String (found) + " notes, worst " + juce::String (worst, 0) + " smp");
        };
        {
            AcidanceProcessor p;
            arpPatch (p);
            Host host;
            host.bpm = bpm; host.ppq = startPpq;
            p.setPlayHead (&host);
            prepare (p, 512);
            auto x = render (p, 60000, 512, chord (boundary - 30), &host);
            check (x, boundary, samplesPer16, "arp: chord before a 16th plays on the grid");
            p.setPlayHead (nullptr);
        }
        {
            AcidanceProcessor p;
            arpPatch (p);
            Host host;
            host.bpm = bpm; host.ppq = startPpq;
            p.setPlayHead (&host);
            prepare (p, 512);
            auto x = render (p, 60000, 512, chord (boundary + 12), &host);
            const int on = onset (x);
            expect (std::abs (on - (boundary + 12)) <= 2, "arp: chord 12 smp late still plays this step at once",
                    "sounds " + juce::String (on - (boundary + 12)) + " smp from the key");
            const int next = (int) std::ceil (boundary + samplesPer16);
            const int on2 = onset (x, next - 600);
            expect (std::abs (on2 - next) <= 2, "arp: ...and the next note is back on the grid",
                    "off by " + juce::String (on2 - next) + " smp");
            p.setPlayHead (nullptr);
        }
        {
            AcidanceProcessor p;
            arpPatch (p);
            p.setParam ("bpm", (float) bpm);
            Host host;
            host.playing = false;
            p.setPlayHead (&host);
            prepare (p, 512);
            auto x = render (p, 60000, 512, chord (1000));
            check (x, 1000, samplesPer16, "arp: song stopped, free clock starts on the key");
            p.setPlayHead (nullptr);
        }
    }

    // 7: a program change from the host's audio thread must not load the preset there ----------
    {
        AcidanceProcessor p;
        prepare (p, 512);
        // median of 20 calls, so one scheduler hiccup can't decide it
        std::vector<double> times;
        std::thread t ([&]
        {
            for (int k = 0; k < 20; ++k)
            {
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                p.setCurrentProgram (1200 + k % 2);
                times.push_back (juce::Time::getMillisecondCounterHiRes() - t0);
            }
            p.setCurrentProgram (1200);
        });
        t.join();
        std::sort (times.begin(), times.end());
        const double ms = times[times.size() / 2];
        expect (ms < 2.0 && p.getCurrentProgram() == 1200, "program change from the audio thread returns at once",
                "median " + juce::String (ms, 3) + " ms, worst " + juce::String (times.back(), 2) + " ms (the preset loads on the message thread)");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
