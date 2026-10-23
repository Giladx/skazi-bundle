// Plays a bassline through the SUBICH engine: voice modes, glide, filters, and every preset.
#include "../Source/SynthEngine.h"
#include "../Source/Presets.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace subich;

namespace
{
    int failures = 0;
    void expect (bool ok, const juce::String& what, const juce::String& detail = {})
    {
        if (! ok) ++failures;
        std::printf ("%-48s %s  %s\n", what.toRawUTF8(), ok ? "OK  " : "FAIL", detail.toRawUTF8());
    }

    struct Result { float peak = 0; double rms = 0; bool finite = true; double dc = 0; };

    // notes: (startSample, lengthSamples, note)
    struct Note { int start, len, note; };

    Result play (const Settings& s, const std::vector<Note>& notes, int totalSamples, double sr = 48000.0, Engine* keep = nullptr)
    {
        Engine local;
        Engine& e = keep != nullptr ? *keep : local;
        e.prepare (sr);
        juce::AudioBuffer<float> buf (2, 256);
        Result r;
        double sum = 0, dcSum = 0;
        for (int pos = 0; pos < totalSamples; pos += 256)
        {
            juce::MidiBuffer midi;
            for (auto& n : notes)
            {
                if (n.start >= pos && n.start < pos + 256) midi.addEvent (juce::MidiMessage::noteOn (1, n.note, 0.9f), n.start - pos);
                const int off = n.start + n.len;
                if (off >= pos && off < pos + 256) midi.addEvent (juce::MidiMessage::noteOff (1, n.note), off - pos);
            }
            e.render (buf, midi, s, nullptr);
            for (int i = 0; i < 256; ++i)
            {
                const float v = buf.getSample (0, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                sum += (double) v * v; dcSum += v;
            }
        }
        r.rms = std::sqrt (sum / totalSamples);
        r.dc = dcSum / totalSamples;
        return r;
    }

    // rolling 1/16 psy line at 145 bpm: off-beat bass notes
    std::vector<Note> bassline (double sr = 48000.0)
    {
        std::vector<Note> n;
        const int step = (int) (sr * 60.0 / 145.0 / 4.0);
        const int line[] = { 30, 30, 30, 42, 30, 30, 33, 30 };
        for (int i = 0; i < 32; ++i)
            if (i % 4 != 0) n.push_back ({ i * step, step * 3 / 4, line[i % 8] });
        return n;
    }
}

int main()
{
    const double sr = 48000.0;
    Settings s = Settings::fromValues ([] (const char* id) { return defaultValues().at (id); });

    auto r = play (s, bassline(), (int) (sr * 3.5));
    expect (r.finite && r.peak <= 1.0f && r.rms > 0.02, "default sound plays a bassline", "peak " + juce::String (r.peak, 2) + "  rms " + juce::String (r.rms, 3));
    expect (std::abs (r.dc) < 0.02, "no DC offset", juce::String (r.dc, 4));

    r = play (s, {}, (int) sr);
    expect (r.peak == 0.0f, "silent with no notes");

    // a held note is released: tail decays to silence
    {
        Engine e;
        r = play (s, { { 0, 4800, 33 } }, (int) (sr * 1.0), sr, &e);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer none;
        e.render (buf, none, s, nullptr);
        expect (buf.getMagnitude (0, 0, 256) < 1.0e-3f, "note off releases to silence");
    }

    // every wave on both oscillators, every filter type
    for (int w = 0; w < NumWaves; ++w)
    {
        auto t = s; t.o1wave = w; t.o2wave = w; t.o2level = 0.5f; t.o1shape = 0.6f;
        r = play (t, bassline(), (int) sr * 2);
        expect (r.finite && r.peak <= 1.0f && r.rms > 0.01, juce::String ("osc wave ") + juce::String (w), "rms " + juce::String (r.rms, 3));
    }
    for (int f = 0; f <= Acid; ++f)
    {
        auto t = s; t.f1type = f; t.f1res = 0.95f; t.f1env = 1.0f; t.f2type = f; t.parallel = f % 2 == 1;
        r = play (t, bassline(), (int) sr * 2);
        expect (r.finite && r.peak <= 1.0f, juce::String ("filter type ") + juce::String (f) + " at max reso", "peak " + juce::String (r.peak, 2));
    }

    // sync, FM, sub, noise, LFOs to every target
    {
        auto t = s; t.o2sync = true; t.o2fm = true; t.o2level = 1.0f; t.noise = 1.0f; t.sublevel = 1.0f;
        t.l1depth = 1.0f; t.l2depth = 1.0f; t.l1shape = LSH; t.l2sync = false; t.l2hz = 20.0f;
        bool ok = true;
        for (int d1 = 0; d1 <= DReso1; ++d1)
        {
            t.l1dest = d1; t.l2dest = (d1 + 3) % 7;
            r = play (t, bassline(), (int) sr);
            ok = ok && r.finite && r.peak <= 1.0f && r.rms > 0.005;
        }
        expect (ok, "sync + FM + LFOs on every target");
    }

    // poly plays a chord louder than one note; mono plays one voice
    {
        auto t = s; t.mode = Poly; t.as = 1.0f; t.f1env = 0;
        const auto one = play (t, { { 0, 48000, 36 } }, 48000).rms;
        const auto three = play (t, { { 0, 48000, 36 }, { 0, 48000, 40 }, { 0, 48000, 43 } }, 48000).rms;
        t.mode = Mono;
        const auto mono3 = play (t, { { 0, 48000, 36 }, { 0, 48000, 40 }, { 0, 48000, 43 } }, 48000).rms;
        expect (three > one * 1.3 && mono3 < one * 1.3, "POLY stacks notes, MONO plays one",
                "one " + juce::String (one, 3) + "  poly3 " + juce::String (three, 3) + "  mono3 " + juce::String (mono3, 3));
    }

    // glide: pitch moves gradually between overlapping notes
    {
        auto t = s; t.mode = Legato; t.glideMs = 200.0f;
        Engine e;
        play (t, { { 0, 30000, 33 }, { 12000, 30000, 45 } }, 12000 + 2400, sr, &e);
        const float mid = e.uiFreq.load();
        const float a = (float) juce::MidiMessage::getMidiNoteInHertz (33), b = (float) juce::MidiMessage::getMidiNoteInHertz (45);
        expect (mid > a * 1.05f && mid < b * 0.95f, "LEGATO glide slides between notes", juce::String (mid, 1) + " Hz between " + juce::String (a, 1) + " and " + juce::String (b, 1));
    }

    // pitch wheel: full up bends by the BEND range (2 semitones)
    {
        Engine e; e.prepare (sr);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.9f), 0);
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        e.render (buf, midi, s, nullptr);
        juce::MidiBuffer none;
        e.render (buf, none, s, nullptr);
        const float expected = (float) juce::MidiMessage::getMidiNoteInHertz (47);
        expect (std::abs (e.uiFreq.load() - expected) < expected * 0.01f && e.uiBend.load() > 0.99f, "PITCH wheel bends 2 semitones up",
                juce::String (e.uiFreq.load(), 1) + " Hz vs " + juce::String (expected, 1));
    }
    // mod wheel: brings LFO 1 in even with its DEPTH knob at 0
    {
        auto t = s; t.l1depth = 0.0f; t.l1dest = DVolume; t.l1shape = LSquare; t.l1sync = false; t.l1hz = 8.0f; t.as = 1.0f; t.f1env = 0;
        auto wobble = [&] (int cc)
        {
            Engine e; e.prepare (sr);
            juce::AudioBuffer<float> buf (2, 256);
            // peak level over 1/32-second windows (longer than one bass cycle), so only the LFO moves it
            float lo = 1e9f, hi = 0, win = 0;
            for (int b = 0; b < 600; ++b)
            {
                juce::MidiBuffer midi;
                if (b == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.9f), 0); midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, cc), 0); }
                e.render (buf, midi, t, nullptr);
                win = std::max (win, buf.getMagnitude (0, 0, 256));
                if (b % 6 == 5) { if (b > 60) { lo = std::min (lo, win); hi = std::max (hi, win); } win = 0; }
            }
            return lo / hi;
        };
        const float still = wobble (0), moving = wobble (127);
        expect (still > 0.8f && moving < 0.3f, "MOD wheel fades LFO 1 in", "level dips to " + juce::String (moving, 2) + " (wheel down: " + juce::String (still, 2) + ")");
    }
    // panic: a held note goes silent at once
    {
        Engine e; e.prepare (sr);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.9f), 0);
        e.render (buf, midi, s, nullptr);
        e.panic();
        juce::MidiBuffer none;
        e.render (buf, none, s, nullptr);
        expect (buf.getMagnitude (0, 0, 256) == 0.0f, "PANIC silences held notes");
    }

    // presets
    const auto& list = presets();
    std::set<juce::String> names;
    std::set<std::map<juce::String, float>> values;
    std::map<juce::String, int> perCat;
    int bad = 0, missing = 0;
    const auto defs = defaultValues();
    for (auto& p : list)
    {
        names.insert (p.name);
        values.insert (p.values);
        perCat[p.category]++;
        for (auto& [id, val] : p.values) if (defs.count (id) == 0) ++missing;
        const auto ps = Settings::fromValues ([&p] (const char* id) { return p.values.at (id); });
        const auto rr = play (ps, bassline(), (int) (sr * 1.2));
        if (! (rr.finite && rr.peak <= 1.0f && rr.rms > 0.01))
        {
            if (++bad <= 5) std::printf ("  bad preset %s / %s  peak %.2f rms %.4f\n", p.category.toRawUTF8(), p.name.toRawUTF8(), rr.peak, rr.rms);
        }
    }
    std::printf ("\n%d presets:", (int) list.size());
    for (auto& [c, n] : perCat) std::printf (" %s %d,", c.toRawUTF8(), n);
    std::printf ("\n");
    expect (list.size() == 1000, "1000 presets");
    expect (names.size() == list.size(), "every preset name unique");
    expect (values.size() == list.size(), "every preset's settings different");
    expect (missing == 0, "presets only use real parameters");
    expect (bad == 0, "every preset plays the bassline cleanly", juce::String (bad) + " bad");

    juce::Random rnd (5);
    int badRandom = 0;
    for (int i = 0; i < 100; ++i)
    {
        const auto p = randomPreset (rnd);
        const auto rr = play (Settings::fromValues ([&p] (const char* id) { return p.values.at (id); }), bassline(), (int) sr);
        if (! (rr.finite && rr.peak <= 1.0f && rr.rms > 0.01)) ++badRandom;
    }
    expect (badRandom == 0, "100 RANDOM rolls play cleanly", juce::String (badRandom) + " bad");

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
