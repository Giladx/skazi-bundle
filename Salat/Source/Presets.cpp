#include "Presets.h"
#include "SalatEngine.h"

namespace salat
{
namespace
{
    // MAKE A SALAT: every effect has its own chance of landing in the bowl, and each one is thrown in
    // within musical limits, so the result is wild but never just noise.
    void salad (skazi::PresetGen& g)
    {
        // the chopper is always in, at a random place in the chain
        g.set ("chopOn", 1);
        g.set ("chopPos", (float) g.pick ({ First, Middle, Middle, Last }));
        g.set ("chopStep", (float) g.pick ({ 1, 2, 2, 3 }));
        g.set ("chopRep", (float) g.pick ({ 1, 2, 2, 3, 4, 5 }));
        g.range ("chance", 30, 80);
        g.range ("wStut", 50, 100);
        g.set ("wRev", g.chance (0.5f) ? g.rr (50, 100) : g.rr (0, 30));
        g.range ("wGate", 0, 70);
        g.range ("wTape", 0, 50);

        // crush depth spikes 80% of the time
        if (g.chance (0.8f)) { g.set ("crOn", 1); g.range ("bits", 3, 10); g.range ("down", 1, 16); }

        // the filter goes in 90% of the time, close to self-oscillation
        if (g.chance (0.9f))
        {
            g.set ("fltOn", 1);
            g.set ("fltType", (float) g.pick ({ LowPass, LowPass, BandPass, HighPass, Notch }));
            g.range ("cutoff", 300, 6000);
            g.range ("reso", 55, 92);
            if (g.chance (0.7f))
            {
                g.set ("lfoRate", (float) g.pick ({ 1, 2, 3, 4, 5, 6 }));
                g.set ("lfoShape", (float) g.pick ({ Sine, Triangle, Saw, Square, SampleHold }));
                g.range ("lfoDepth", 0.5f, 3.0f);
            }
        }
        else g.set ("fltOn", 0);

        if (g.chance (0.7f)) { g.set ("drvOn", 1); g.set ("drvMode", (float) g.pick ({ Tube, Fuzz, Fold, Rectify })); g.range ("drive", 6, 24); }
        else g.set ("drvOn", 0);

        if (g.chance (0.3f)) { g.set ("ringOn", 1); g.range ("ringFreq", 60, 1800); g.range ("ringMix", 20, 70); }
        if (g.chance (0.4f))
        {
            g.set ("modOn", 1); g.set ("modType", (float) g.pick ({ Flanger, Chorus }));
            g.range ("modRate", 0.1f, 2.0f); g.range ("modDepth", 30, 90); g.range ("modFb", 20, 85); g.range ("modMix", 30, 80);
        }
        if (g.chance (0.5f))
        {
            g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 1, 2, 4, 5, 6, 7 }));
            g.range ("dlyFb", 20, 65); g.range ("dlyTone", 20, 80); g.range ("dlyMix", 15, 45);
        }
        if (g.chance (0.45f)) { g.set ("revOn", 1); g.range ("revSize", 40, 90); g.range ("revDamp", 20, 70); g.range ("revMix", 10, 40); }
        g.range ("out", -3, 0);
    }

    struct Category { const char* name; std::function<void (skazi::PresetGen&)> style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "GLITCH", [] (skazi::PresetGen& g) {
                  g.set ("chopStep", (float) g.pick ({ 2, 3 })); g.set ("chopRep", (float) g.pick ({ 2, 3, 4, 5 }));
                  g.range ("chance", 40, 75); g.range ("wStut", 70, 100); g.range ("wRev", 20, 60); g.range ("wGate", 10, 50); g.range ("wTape", 0, 30);
                  g.range ("drive", 3, 12); g.range ("cutoff", 6000, 16000); g.range ("reso", 10, 30); } },
            { "STUTTER", [] (skazi::PresetGen& g) {
                  g.set ("chopStep", (float) g.pick ({ 1, 2 })); g.set ("chopRep", (float) g.pick ({ 2, 4, 5, 6 }));
                  g.range ("chance", 50, 90); g.set ("wStut", 100); g.range ("wRev", 0, 20); g.set ("wGate", 0); g.set ("wTape", 0);
                  g.set ("dlyOn", (float) g.pick ({ 0, 1 })); g.range ("dlyMix", 10, 30); g.range ("dlyFb", 20, 50); } },
            { "TAPE & REVERSE", [] (skazi::PresetGen& g) {
                  g.set ("chopStep", (float) g.pick ({ 0, 1 })); g.range ("chance", 25, 55); g.range ("wStut", 0, 30);
                  g.range ("wRev", 60, 100); g.range ("wTape", 50, 100); g.set ("wGate", 0);
                  g.set ("revOn", 1); g.range ("revSize", 50, 85); g.range ("revMix", 15, 35); g.set ("drvMode", Tube); g.range ("drive", 2, 9); } },
            { "LO-FI", [] (skazi::PresetGen& g) {
                  g.set ("crOn", 1); g.range ("bits", 4, 9); g.range ("down", 2, 12); g.range ("chance", 15, 40);
                  g.range ("cutoff", 1800, 6000); g.range ("reso", 10, 40); g.set ("drvMode", (float) g.pick ({ Tube, Fuzz })); g.range ("drive", 6, 16); } },
            { "FILTER FX", [] (skazi::PresetGen& g) {
                  g.set ("fltType", (float) g.pick ({ LowPass, BandPass, HighPass, Notch })); g.range ("cutoff", 400, 4000); g.range ("reso", 50, 88);
                  g.set ("lfoRate", (float) g.pick ({ 1, 2, 3, 4, 5 })); g.set ("lfoShape", (float) g.pick ({ Sine, Triangle, Saw, Square, SampleHold }));
                  g.range ("lfoDepth", 1.0f, 3.5f); g.range ("chance", 0, 30); g.range ("drive", 3, 12); } },
            { "DUB SPACE", [] (skazi::PresetGen& g) {
                  g.range ("chance", 10, 35); g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 1, 2, 5 })); g.range ("dlyFb", 45, 80);
                  g.range ("dlyTone", 10, 45); g.range ("dlyMix", 25, 50); g.set ("revOn", 1); g.range ("revSize", 60, 95); g.range ("revMix", 15, 40);
                  g.range ("cutoff", 1500, 7000); g.range ("reso", 20, 50); } },
            { "METAL MOD", [] (skazi::PresetGen& g) {
                  g.set ("ringOn", 1); g.range ("ringFreq", 80, 2400); g.range ("ringMix", 25, 70);
                  g.set ("modOn", 1); g.set ("modType", (float) g.pick ({ Flanger, Chorus })); g.range ("modRate", 0.1f, 1.5f);
                  g.range ("modDepth", 40, 100); g.range ("modFb", 40, 90); g.range ("modMix", 40, 90); g.range ("chance", 20, 50); } },
            { "SALAT", salad },
        };
        return c;
    }

    std::vector<skazi::Preset> build()
    {
        const auto defs = skazi::defaultValues (paramDefs());
        std::vector<skazi::Preset> list;
        auto factory = [&] (const char* name, std::initializer_list<std::pair<const char*, float>> changes)
        {
            skazi::Preset p { name, "FACTORY", defs };
            for (auto& [id, v] : changes) p.values[id] = v;
            list.push_back (std::move (p));
        };
        factory ("Init", {});
        factory ("Clean Chop", { { "drvOn", 0 }, { "fltOn", 0 }, { "chance", 50 } });
        factory ("Stutter Machine", { { "chance", 70 }, { "chopRep", 4 }, { "wRev", 10 }, { "wGate", 0 }, { "wTape", 0 } });
        factory ("Backspin", { { "chopStep", 1 }, { "chance", 40 }, { "wStut", 20 }, { "wRev", 100 }, { "wTape", 60 }, { "wGate", 0 } });
        factory ("Crunchy Salat", { { "crOn", 1 }, { "bits", 6 }, { "down", 6 }, { "drvMode", Fuzz }, { "drive", 15 },
                                    { "reso", 70 }, { "cutoff", 2500 }, { "lfoDepth", 2 }, { "lfoRate", 4 } });
        factory ("Robot Kitchen", { { "ringOn", 1 }, { "ringFreq", 320 }, { "ringMix", 60 }, { "modOn", 1 }, { "modFb", 80 }, { "chance", 45 } });
        factory ("Dub Bowl", { { "dlyOn", 1 }, { "dlyFb", 65 }, { "dlyMix", 40 }, { "revOn", 1 }, { "revMix", 30 }, { "cutoff", 3000 }, { "chance", 25 } });
        factory ("Tape Stop Drop", { { "chopStep", 0 }, { "chance", 50 }, { "wStut", 0 }, { "wRev", 0 }, { "wGate", 0 }, { "wTape", 100 } });

        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < 12; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 4127 + 91);
                skazi::Preset p { skazi::presetName (g + 900), cat.name, defs };
                skazi::PresetGen gen { r, p.values };
                cat.style (gen);
                list.push_back (std::move (p));
            }
        return list;
    }
}

const std::vector<skazi::Preset>& presets()
{
    static const std::vector<skazi::Preset> list = build();
    return list;
}

skazi::Preset randomPreset (juce::Random& rng)
{
    skazi::Preset p { "Salat", "SALAT", skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    salad (gen);
    return p;
}
} // namespace salat
