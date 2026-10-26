// Engine checks: presets only use real parameters, the crossover sums back flat (also at 50% mix),
// MONO BASS keeps the lows centred, and hard drive stays finite and in range.
#include <juce_dsp/juce_dsp.h>
#include "../Source/QuaziEngine.h"
#include "../Source/Presets.h"

using namespace quazi;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static Settings flat()
{
    auto defs = skazi::defaultValues (paramDefs());
    for (int b = 0; b < NumBands; ++b) defs[bandId (b, "Drive")] = 0;
    return Settings::fromValues ([&] (const char* id) { return defs.at (id); });
}

// Gain in dB of a sine through the engine, measured after it settles
static float sineGain (const Settings& s, double freq, float amp, int channelMask = 3, float* sideOut = nullptr)
{
    const double sr = 48000.0; const int bs = 256;
    Engine e; e.prepare (sr, bs);
    juce::AudioBuffer<float> buf (2, bs);
    double inSq = 0, outSq = 0, side = 0; int n = 0;
    for (int blk = 0; blk < 400; ++blk)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float x = amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (blk * bs + i) / sr);
            buf.setSample (0, i, (channelMask & 1) ? x : 0.0f);
            buf.setSample (1, i, (channelMask & 2) ? x : 0.0f);
        }
        e.process (buf, s);
        if (blk >= 200)
            for (int i = 0; i < bs; ++i)
            {
                const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                outSq += l * l; inSq += (double) amp * amp * 0.5; side += (l - r) * (l - r); ++n;
            }
    }
    if (sideOut) *sideOut = (float) std::sqrt (side / juce::jmax (1e-12, outSq));
    return (float) (10.0 * std::log10 (outSq / inSq));
}

int main()
{
    // 1. presets
    {
        const auto defs = skazi::defaultValues (paramDefs());
        bool ok = true;
        for (auto& p : presets())
            for (auto& [id, v] : p.values)
                if (defs.find (id) == defs.end()) { ok = false; std::printf ("  preset %s uses unknown %s\n", p.name.toRawUTF8(), id.toRawUTF8()); }
        check (ok, "all " + juce::String ((int) presets().size()) + " presets use real parameters");
    }

    // 2. flat sum
    {
        auto s = flat();
        float worst = 0;
        for (double f : { 40.0, 100.0, 150.0, 300.0, 1200.0, 2500.0, 5000.0, 9000.0, 15000.0 })
            worst = juce::jmax (worst, std::abs (sineGain (s, f, 0.03f)));
        check (worst < 0.3f, "crossover sums flat, worst " + juce::String (worst, 3) + " dB");

        s.mix = 0.5f;
        worst = 0;
        for (double f : { 40.0, 150.0, 1200.0, 5000.0, 9000.0, 15000.0 })
        {
            const float gdb = sineGain (s, f, 0.03f);
            if (std::abs (gdb) > 0.2f) std::printf ("  %.0f Hz: %.2f dB\n", f, gdb);
            worst = juce::jmax (worst, std::abs (gdb));
        }
        check (worst < 1.0f, "50% mix stays flat (each band blends with its clean self), worst " + juce::String (worst, 3) + " dB");
    }

    // 3. mono bass
    {
        auto s = flat();
        float side = 0;
        sineGain (s, 50.0, 0.3f, 1, &side);
        check (side < 0.1f, "MONO BASS centres a left-only 50 Hz (side/total " + juce::String (side, 3) + ")");
        s.monoBass = false;
        sineGain (s, 50.0, 0.3f, 1, &side);
        check (side > 0.9f, "without MONO BASS it stays left (side/total " + juce::String (side, 3) + ")");
    }

    // 4. hard drive
    {
        auto s = flat();
        for (int b = 0; b < NumBands; ++b) { s.driveDb[(size_t) b] = 48; s.mode[(size_t) b] = b % NumModes; s.level[(size_t) b] = 4; }
        s.width = 2; s.lowDb = s.midDb = s.highDb = 15; s.outGain = 4;
        Engine e; e.prepare (44100.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::Random r (3);
        bool finite = true; float peak = 0;
        for (int blk = 0; blk < 100; ++blk)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, r.nextFloat() * 2 - 1);
            e.process (buf, s);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i)
            {
                const float y = buf.getSample (c, i);
                finite = finite && std::isfinite (y); peak = juce::jmax (peak, std::abs (y));
            }
        }
        check (finite && peak <= 1.0f, "48 dB drive into white noise stays finite and under 0 dBFS (peak " + juce::String (peak, 3) + ")");
        check (e.latency() > 0, "reports " + juce::String (e.latency()) + " samples latency");
    }

    std::printf (failures == 0 ? "\nall passed\n" : "\n%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
