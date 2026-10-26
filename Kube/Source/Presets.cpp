#include "Presets.h"
#include "KubeEngine.h"
#include "PresetTrims.h"

namespace kube
{
namespace
{
    // The bank's order (see tableNames)
    enum Table { Growl, Yoy, Robot, Talkbox, Choir, MetalCube, Glass, SpectralShift, ReeseComb, HollowComb, PsySquelch, AcidSquare,
                 LaserChirp, PsyRes, PhaseDist, FoldSine, DigitalGrit, Stairs, WarmBrass, OrganDraw, SawStack, SpectralNoise, FmCube, BellCluster };

    // Rolling psy bass: one short filter pluck per 1/16, gone before the next kick
    void bass (skazi::PresetGen& g)
    {
        g.set ("mode", Mono); g.set ("aRand", 0); g.set ("bRand", 0); // the same punch on every note
        switch (g.pick ({ 0, 0, 1, 2 }))
        {
            case 0:  g.set ("aTable", SawStack); g.range ("aPos", 0, 25); break;
            case 1:  g.set ("aTable", PhaseDist); g.range ("aPos", 65, 100); break;
            default: g.set ("aTable", AcidSquare); g.range ("aPos", 0, 15); break;
        }
        g.set ("aOct", -1);
        if (g.chance (0.4f)) { g.set ("aWarp1", (float) g.pick ({ WarpBendPlus, WarpAsymPlus, WarpMirror })); g.range ("aWarp1Amt", 10, 45); }
        g.set ("bOn", (float) g.pick ({ 0, 1 })); g.set ("bTable", (float) g.pick ({ SawStack, PhaseDist })); g.range ("bPos", 0, 60); g.set ("bOct", -1);
        g.range ("bFine", 4, 12); g.range ("bLevel", 40, 70); g.range ("sub", 0, 40);
        g.set ("fltType", LP24); g.range ("cutoff", 120, 500); g.range ("reso", 15, 45); g.range ("fltEnv", 35, 65);
        g.set ("fltA", 0); g.range ("fltD", 70, 180); g.set ("fltS", 0); g.range ("fltR", 40, 90); g.range ("keyTrk", 30, 70);
        g.set ("ampA", 0.5f); g.range ("ampD", 140, 230); g.set ("ampS", 0); g.range ("ampR", 25, 60);
        g.range ("drive", 0, 35); g.set ("vel", 10); g.range ("out", -6, -3);
    }

    // 303-style: resonant and gliding
    void acid (skazi::PresetGen& g)
    {
        g.set ("mode", Legato); g.range ("glide", 40, 110); g.set ("aRand", 0);
        g.set ("aTable", (float) g.pick ({ AcidSquare, PsySquelch, SawStack })); g.range ("aPos", 0, 20); g.set ("aOct", -1);
        if (g.chance (0.35f)) { g.set ("aWarp1", WarpPwm); g.range ("aWarp1Amt", 10, 50); }
        g.set ("fltType", LP24); g.range ("cutoff", 250, 900); g.range ("reso", 70, 92); g.range ("fltEnv", 40, 80);
        g.set ("fltA", 0); g.range ("fltD", 120, 400); g.set ("fltS", 0); g.range ("drive", 20, 60);
        g.set ("ampA", 1); g.range ("ampD", 300, 800); g.range ("ampS", 50, 90); g.range ("ampR", 30, 80);
        if (g.chance (0.5f)) { g.set ("lfoSync", 1); g.set ("lfoRate", (float) g.pick ({ 0, 1, 11 })); g.range ("lfoCut", 10, 35); }
        if (g.chance (0.4f)) { g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 2, 6 })); g.range ("dlyMix", 10, 25); g.range ("dlyFb", 25, 50); }
        g.range ("out", -7, -4);
    }

    // Huge 9-16 voice stacks for the drop
    void lead (skazi::PresetGen& g)
    {
        g.set ("mode", (float) g.pick ({ Poly, Mono, Legato })); g.range ("glide", 0, 60);
        g.set ("aTable", (float) g.pick ({ SawStack, SawStack, ReeseComb, Growl, SpectralShift })); g.range ("aPos", 0, 50);
        g.set ("aUni", (float) g.pick ({ 9, 12, 16 })); g.range ("aDetune", 25, 55); g.range ("aWidth", 70, 100); g.range ("aBlend", 60, 90);
        if (g.chance (0.5f)) { g.set ("aWarp1", (float) g.pick ({ WarpSync, WarpBendMinus, WarpMirror })); g.range ("aWarp1Amt", 10, 40); }
        g.set ("bOn", 1); g.set ("bTable", (float) g.pick ({ WarmBrass, HollowComb, PhaseDist, OrganDraw })); g.range ("bPos", 30, 90);
        g.set ("bOct", (float) g.pick ({ 0, 1, -1 })); g.range ("bLevel", 35, 65); g.set ("bUni", (float) g.pick ({ 1, 3, 5 }));
        g.set ("fltType", (float) g.pick ({ LP24, LP12 })); g.range ("cutoff", 2500, 9000); g.range ("reso", 10, 35); g.range ("fltEnv", 0, 25);
        g.set ("ampA", 2); g.range ("ampD", 400, 1200); g.range ("ampS", 70, 95); g.range ("ampR", 150, 500);
        g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 2, 0, 5 })); g.range ("dlyMix", 15, 30); g.range ("dlyFb", 30, 50);
        g.set ("revOn", 1); g.range ("revSize", 50, 80); g.range ("revMix", 12, 25); g.range ("out", -10, -7);
    }

    void pluck (skazi::PresetGen& g)
    {
        g.set ("mode", Poly);
        g.set ("aTable", (float) g.pick ({ Glass, BellCluster, FmCube, MetalCube, OrganDraw })); g.range ("aPos", 10, 80);
        g.set ("aUni", (float) g.pick ({ 1, 3, 5 })); g.range ("aDetune", 10, 30);
        if (g.chance (0.4f)) { g.set ("aWarp1", (float) g.pick ({ WarpFlip, WarpAsymMinus, WarpSync })); g.range ("aWarp1Amt", 10, 40); }
        g.set ("fltType", LP24); g.range ("cutoff", 700, 2200); g.range ("reso", 20, 55); g.range ("fltEnv", 45, 75);
        g.set ("fltA", 0); g.range ("fltD", 90, 260); g.set ("fltS", 0);
        g.set ("ampA", 0.5f); g.range ("ampD", 250, 600); g.set ("ampS", 0); g.range ("ampR", 150, 350);
        g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 2, 1, 6 })); g.range ("dlyMix", 20, 35); g.range ("dlyFb", 35, 60);
        g.set ("revOn", (float) g.pick ({ 0, 1 })); g.range ("revMix", 10, 25); g.range ("out", -8, -5);
    }

    // Laser zaps: fast pitch drop, the chirp and resonance tables swept by the envelope
    void zap (skazi::PresetGen& g)
    {
        g.set ("mode", (float) g.pick ({ Mono, Poly })); g.set ("aRand", 0);
        g.set ("aTable", (float) g.pick ({ LaserChirp, PsyRes, LaserChirp, FmCube })); g.range ("aPos", 0, 30); g.range ("envPos", 50, 100);
        if (g.chance (0.5f)) { g.set ("aWarp1", WarpSync); g.range ("aWarp1Amt", 10, 50); }
        g.range ("pitchAmt", 18, 48); g.range ("pitchDec", 25, 160);
        g.set ("fltType", (float) g.pick ({ LP12, LP12, LP24 })); g.range ("cutoff", 1500, 6000); g.range ("reso", 40, 75); g.range ("fltEnv", 20, 60);
        g.set ("fltA", 0); g.range ("fltD", 80, 300); g.set ("fltS", 0);
        g.set ("ampA", 0); g.range ("ampD", 120, 400); g.set ("ampS", 0); g.range ("ampR", 60, 200);
        g.range ("fold", 0, 40);
        g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 2, 3, 6 })); g.range ("dlyMix", 20, 40); g.range ("dlyFb", 40, 65);
        g.set ("revOn", 1); g.range ("revMix", 15, 30); g.range ("out", -8, -5);
    }

    void pad (skazi::PresetGen& g)
    {
        g.set ("mode", Poly);
        g.set ("aTable", (float) g.pick ({ Choir, WarmBrass, HollowComb, SpectralShift })); g.range ("aPos", 10, 70);
        g.set ("aUni", (float) g.pick ({ 9, 12, 16 })); g.range ("aDetune", 25, 50); g.range ("aWidth", 80, 100); g.range ("aBlend", 70, 100);
        g.set ("bOn", 1); g.set ("bTable", (float) g.pick ({ Choir, OrganDraw, Glass })); g.range ("bPos", 20, 80); g.set ("bOct", (float) g.pick ({ 0, 1 }));
        g.range ("bLevel", 30, 60); g.set ("bUni", 5); g.range ("bDetune", 15, 35);
        g.set ("fltType", LP12); g.range ("cutoff", 1200, 5000); g.range ("reso", 5, 25);
        g.range ("ampA", 400, 1800); g.range ("ampD", 800, 2000); g.range ("ampS", 60, 90); g.range ("ampR", 1200, 3500);
        g.set ("lfoSync", 1); g.set ("lfoRate", (float) g.pick ({ 0, 11, 12 })); g.range ("lfoPos", 20, 50); g.range ("lfoCut", 0, 20);
        g.set ("chOn", 1); g.range ("chMix", 30, 60); g.set ("revOn", 1); g.range ("revSize", 70, 95); g.range ("revMix", 25, 45);
        g.range ("out", -12, -8);
    }

    // Twisted goa lines: talking tables, cross-mod from osc B and a synced filter LFO
    void goa (skazi::PresetGen& g)
    {
        g.set ("mode", (float) g.pick ({ Mono, Legato, Poly })); g.range ("glide", 0, 80);
        g.set ("aTable", (float) g.pick ({ Growl, Yoy, Talkbox, Robot, FmCube })); g.range ("aPos", 20, 80);
        if (g.chance (0.5f))
        {
            g.set ("bOn", 1); g.set ("bTable", (float) g.pick ({ FoldSine, PhaseDist })); g.range ("bPos", 0, 60); g.set ("bLevel", (float) g.pick ({ 0, 20, 40 }));
            g.set ("aWarp1", (float) g.pick ({ WarpFm, WarpRm, WarpAm })); g.range ("aWarp1Amt", 15, 60);
        }
        g.range ("fold", 10, 45); g.range ("lfoFold", 0, 40);
        g.set ("fltType", (float) g.pick ({ LP24, LP12, LP12, Notch })); g.range ("cutoff", 600, 3000); g.range ("reso", 40, 80);
        g.set ("lfoSync", 1); g.set ("lfoShape", (float) g.pick ({ Sine, Triangle, Saw, SampleHold })); g.set ("lfoRate", (float) g.pick ({ 3, 4, 7, 10 }));
        g.range ("lfoCut", 25, 60); g.range ("lfoPos", 0, 50);
        g.set ("ampA", 2); g.range ("ampD", 300, 900); g.range ("ampS", 60, 90); g.range ("ampR", 80, 300);
        g.set ("dlyOn", 1); g.set ("dlyTime", (float) g.pick ({ 2, 0, 6 })); g.range ("dlyMix", 15, 35); g.range ("dlyFb", 35, 60);
        g.set ("revOn", (float) g.pick ({ 0, 1 })); g.range ("revMix", 10, 25); g.range ("out", -9, -6);
    }

    void fx (skazi::PresetGen& g)
    {
        g.set ("mode", Poly);
        g.set ("aTable", (float) g.pick ({ SpectralNoise, DigitalGrit, Stairs, MetalCube, LaserChirp })); g.range ("aPos", 0, 100);
        g.set ("aWarp1", (float) g.pick ({ WarpQuantize, WarpFlip, WarpSync })); g.range ("aWarp1Amt", 20, 70);
        g.range ("noise", 0, 40); g.range ("fold", 0, 60);
        g.set ("fltType", (float) g.pick ({ BandPass, HighPass, LP12, Notch })); g.range ("cutoff", 500, 5000); g.range ("reso", 50, 85);
        g.set ("lfoSync", 1); g.set ("lfoShape", (float) g.pick ({ SampleHold, Saw, Square })); g.set ("lfoRate", (float) g.pick ({ 4, 5, 8 }));
        g.range ("lfoCut", -60, 60); g.range ("lfoPos", -50, 50);
        g.range ("pitchAmt", -24, 24); g.range ("pitchDec", 200, 1500);
        g.range ("ampA", 5, 400); g.range ("ampD", 500, 2000); g.range ("ampS", 30, 80); g.range ("ampR", 800, 3000);
        g.set ("dlyOn", 1); g.range ("dlyMix", 25, 45); g.range ("dlyFb", 50, 75);
        g.set ("revOn", 1); g.range ("revSize", 70, 98); g.range ("revMix", 30, 50); g.range ("out", -12, -8);
    }

    struct Category { const char* name; void (*style) (skazi::PresetGen&); };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "BASS", bass }, { "ACID", acid }, { "LEAD", lead }, { "PLUCK", pluck },
            { "ZAP", zap }, { "PAD", pad }, { "GOA", goa }, { "FX", fx },
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
        factory ("Rolling Kube", { { "mode", Mono }, { "aRand", 0 }, { "aOct", -1 }, { "aTable", PhaseDist }, { "aPos", 85 }, { "cutoff", 260 },
                                   { "reso", 30 }, { "fltEnv", 55 }, { "fltA", 0 }, { "fltD", 120 }, { "fltS", 0 }, { "ampA", 0.5f },
                                   { "ampD", 190 }, { "ampS", 0 }, { "ampR", 40 }, { "sub", 25 }, { "drive", 20 }, { "vel", 10 }, { "out", -4 } });
        factory ("Hyper Stack", { { "aUni", 16 }, { "aDetune", 45 }, { "aWidth", 100 }, { "aBlend", 80 }, { "bOn", 1 }, { "bTable", WarmBrass },
                                  { "bPos", 60 }, { "bOct", 1 }, { "bLevel", 40 }, { "cutoff", 7000 }, { "ampR", 300 }, { "dlyOn", 1 },
                                  { "dlyMix", 20 }, { "revOn", 1 }, { "revMix", 18 }, { "out", -9 } });
        factory ("Goa Acid", { { "mode", Legato }, { "glide", 70 }, { "aRand", 0 }, { "aOct", -1 }, { "aTable", AcidSquare }, { "aPos", 10 },
                               { "cutoff", 450 }, { "reso", 85 }, { "fltEnv", 60 }, { "fltA", 0 }, { "fltD", 220 }, { "fltS", 0 }, { "drive", 45 },
                               { "ampS", 80 }, { "out", -6 } });
        factory ("Laser Zap", { { "aTable", LaserChirp }, { "aRand", 0 }, { "aPos", 10 }, { "envPos", 90 }, { "pitchAmt", 36 }, { "pitchDec", 70 },
                                { "fltEnv", 40 }, { "fltD", 160 }, { "fltS", 0 }, { "ampA", 0 }, { "ampD", 250 }, { "ampS", 0 }, { "ampR", 100 },
                                { "dlyOn", 1 }, { "dlyMix", 30 }, { "dlyFb", 55 }, { "revOn", 1 }, { "revMix", 20 }, { "out", -7 } });
        factory ("Growl Machine", { { "aTable", Growl }, { "aPos", 30 }, { "lfoPos", 70 }, { "lfoRate", 3 }, { "aWarp1", WarpBendPlus },
                                    { "aWarp1Amt", 30 }, { "cutoff", 4000 }, { "reso", 30 }, { "mode", Mono }, { "glide", 30 }, { "out", -8 } });
        factory ("Yoy Talker", { { "aTable", Yoy }, { "lfoPos", 90 }, { "lfoRate", 2 }, { "lfoShape", Triangle }, { "cutoff", 5000 },
                                 { "mode", Mono }, { "glide", 40 }, { "out", -7 } });
        factory ("FM Monster", { { "aTable", Robot }, { "aPos", 40 }, { "bOn", 1 }, { "bTable", FoldSine }, { "bPos", 30 }, { "bLevel", 0 },
                                 { "bOct", 1 }, { "aWarp1", WarpFm }, { "aWarp1Amt", 45 }, { "lfoFold", 30 }, { "fold", 20 }, { "cutoff", 3500 },
                                 { "mode", Legato }, { "glide", 50 }, { "out", -8 } });
        factory ("Glass Arp", { { "aTable", Glass }, { "aPos", 40 }, { "fltEnv", 50 }, { "cutoff", 900 }, { "fltD", 200 }, { "fltS", 0 },
                                { "ampD", 400 }, { "ampS", 0 }, { "ampR", 250 }, { "dlyOn", 1 }, { "dlyMix", 30 }, { "dlyFb", 50 }, { "out", -7 } });
        factory ("Choir Cube", { { "aTable", Choir }, { "aPos", 40 }, { "aUni", 12 }, { "aDetune", 40 }, { "aWidth", 100 }, { "fltType", LP12 },
                                 { "cutoff", 3500 }, { "ampA", 1200 }, { "ampR", 3000 }, { "lfoPos", 40 }, { "lfoRate", 11 }, { "chOn", 1 },
                                 { "revOn", 1 }, { "revSize", 90 }, { "revMix", 40 }, { "out", -10 } });

        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < 12; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 7919 + 313);
                skazi::Preset p { skazi::presetName (g + 1100), cat.name, defs };
                skazi::PresetGen gen { r, p.values };
                cat.style (gen);
                list.push_back (std::move (p));
            }

        for (size_t i = 0; i < list.size() && i < std::size (kPresetTrims); ++i)
            list[i].values["out"] = juce::jlimit (-36.0f, 6.0f, list[i].values["out"] + kPresetTrims[i]);
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
    const auto& cats = categories();
    const auto& cat = cats[(size_t) rng.nextInt ((int) cats.size())];
    skazi::Preset p { "Random", cat.name, skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    cat.style (gen);
    return p;
}
} // namespace kube
