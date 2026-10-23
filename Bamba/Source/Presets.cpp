#include "Presets.h"
#include "BambaEngine.h"

namespace bamba
{
namespace
{
    constexpr int kPerType = 14;
    const char* const kFolders[] = { "UPLIFT", "DOWNLIFT", "NOISE SWEEP", "IMPACT", "SNARE ROLL", "REVERSE CRASH", "LASER", "SIREN", "SUB DROP", "RANDOM FX" };

    // bars choice: 0 1 bar, 1 2 bars, 2 4 bars, 3 8 bars, 4 16 bars
    void style (skazi::PresetGen& g, int type)
    {
        g.set ("type", (float) type);
        g.range ("curve", 35, 80);
        g.range ("width", 40, 100);
        g.range ("reverb", 20, 80);
        g.range ("delay", 0, 50);
        g.range ("phaser", 0, 50);
        g.range ("color", 20, 80);
        switch (type)
        {
            case Uplifter:
                g.set ("bars", (float) g.pick ({ 2, 3, 3, 4 })); g.range ("noise", 40, 90); g.range ("tone", 20, 70);
                g.range ("pfrom", -24, -6); g.range ("pto", 12, 36); g.range ("sweep", 60, 100); g.range ("reso", 20, 70);
                g.set ("roll", g.chance (0.4f) ? g.rr (20, 70) : 0.0f); break;
            case Downlifter:
                g.set ("bars", (float) g.pick ({ 1, 2, 2, 3 })); g.range ("noise", 50, 90); g.range ("tone", 10, 50);
                g.range ("pfrom", -24, -12); g.range ("pto", 0, 24); g.range ("sweep", 60, 100); g.range ("reso", 10, 50); g.set ("roll", 0); break;
            case NoiseSweep:
                g.set ("bars", (float) g.pick ({ 1, 2, 3 })); g.range ("noise", 70, 100); g.range ("tone", 0, 20);
                g.range ("sweep", 70, 100); g.range ("reso", 30, 85); g.set ("roll", g.chance (0.3f) ? g.rr (20, 50) : 0.0f); break;
            case Impact:
                g.set ("bars", (float) g.pick ({ 0, 1 })); g.range ("noise", 40, 80); g.range ("tone", 70, 100);
                g.range ("sweep", 30, 80); g.range ("reso", 0, 30); g.range ("reverb", 50, 90); g.set ("roll", 0); break;
            case SnareRoll:
                g.set ("bars", (float) g.pick ({ 1, 2, 2, 3 })); g.range ("noise", 60, 100); g.range ("tone", 30, 70);
                g.range ("pfrom", -12, 0); g.range ("pto", 0, 24); g.range ("sweep", 40, 90); g.range ("reso", 10, 40); break;
            case ReverseCrash:
                g.set ("bars", (float) g.pick ({ 0, 1, 2 })); g.range ("noise", 80, 100); g.range ("tone", 0, 20);
                g.range ("color", 0, 40); g.range ("sweep", 0, 30); g.set ("roll", 0); break;
            case Laser:
                g.set ("bars", (float) g.pick ({ 0, 1, 2 })); g.range ("noise", 0, 20); g.range ("tone", 60, 100);
                g.range ("sweep", 20, 70); g.range ("reso", 30, 80); g.set ("roll", g.chance (0.4f) ? g.rr (20, 60) : 0.0f); break;
            case Siren:
                g.set ("bars", (float) g.pick ({ 1, 2, 3 })); g.range ("noise", 0, 30); g.range ("tone", 60, 100);
                g.range ("pfrom", -12, 0); g.range ("pto", 12, 36); g.range ("sweep", 30, 80); g.range ("reso", 20, 60); break;
            case SubDrop:
                g.set ("bars", (float) g.pick ({ 0, 1 })); g.range ("noise", 0, 10); g.range ("tone", 80, 100);
                g.range ("pto", 12, 36); g.range ("sweep", 0, 20); g.range ("reverb", 0, 40); g.range ("delay", 0, 20); g.set ("roll", 0); break;
            default: // random fx
                g.set ("bars", (float) g.pick ({ 1, 2, 3 })); g.range ("noise", 30, 90); g.range ("tone", 30, 90); break;
        }
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
        factory ("8 Bar Rocket", { { "type", Uplifter }, { "bars", 3 }, { "roll", 40 }, { "pto", 36 } });
        factory ("Drop Impact", { { "type", Impact }, { "bars", 1 }, { "reverb", 75 } });
        factory ("Snare Build", { { "type", SnareRoll }, { "bars", 2 }, { "tone", 50 } });
        factory ("Wash Down", { { "type", Downlifter }, { "bars", 2 }, { "reverb", 70 } });
        factory ("Laser Rain", { { "type", Laser }, { "bars", 1 }, { "delay", 50 } });
        factory ("Reverse Wash", { { "type", ReverseCrash }, { "bars", 1 } });
        factory ("Sub Bomb", { { "type", SubDrop }, { "bars", 1 }, { "pto", 24 } });

        int g = 0;
        for (int type = 0; type < NumTypes; ++type)
            for (int i = 0; i < kPerType; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 4513 + 911);
                skazi::Preset p { skazi::presetName (g + 200), kFolders[type], defs };
                skazi::PresetGen gen { r, p.values };
                style (gen, type);
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
    skazi::Preset p { "Random", "RANDOM", skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    style (gen, r.nextInt (NumTypes));
    return p;
}
} // namespace bamba
