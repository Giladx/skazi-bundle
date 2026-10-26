#include "Presets.h"
#include "QuaziEngine.h"

namespace quazi
{
namespace
{
    void band (skazi::PresetGen& g, int b, int mode, float driveLo, float driveHi, float levelLo = -2, float levelHi = 1)
    {
        g.set (bandId (b, "Mode").toRawUTF8(), (float) mode);
        g.range (bandId (b, "Drive").toRawUTF8(), driveLo, driveHi);
        g.range (bandId (b, "Level").toRawUTF8(), levelLo, levelHi);
    }

    void xover (skazi::PresetGen& g, float lo0, float lo1, float mid0, float mid1, float hi0, float hi1)
    {
        g.set ("xLo", std::round (g.rr (lo0, lo1)));
        g.set ("xMid", std::round (g.rr (mid0, mid1)));
        g.set ("xHi", std::round (g.rr (hi0, hi1)));
    }

    // RANDOM: every band gets its own character, within limits that keep it musical
    void quad (skazi::PresetGen& g)
    {
        xover (g, 80, 300, 600, 2500, 3000, 9000);
        for (int b = 0; b < NumBands; ++b)
        {
            const int mode = g.pick ({ Soft, Soft, Hard, Fuzz, Tube, Tube });
            band (g, b, mode, b == Low ? 0 : 4, b == Low ? 18 : 34, -4, 2);
            g.set (bandId (b, "On").toRawUTF8(), g.chance (0.9f) ? 1.0f : 0.0f);
        }
        g.range ("width", 90, 160);
        g.set ("monoBass", 1);
        g.range ("eqLowDb", -3, 4); g.set ("eqLowF", std::round (g.rr (60, 160)));
        g.range ("eqMidDb", -6, 4); g.set ("eqMidF", std::round (g.rr (400, 3000))); g.range ("eqMidQ", 0.5f, 2.0f);
        g.range ("eqHighDb", -6, 3); g.set ("eqHighF", std::round (g.rr (5000, 12000)));
        g.range ("out", -4, 0);
    }

    struct Category { const char* name; std::function<void (skazi::PresetGen&)> style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "BASS", [] (skazi::PresetGen& g) {
                  // clean sub, dirty upper bass: the classic way to make a bass cut through
                  xover (g, 90, 180, 700, 1600, 3500, 7000);
                  band (g, Low, Soft, 0, 4); band (g, LowMid, g.pick ({ Tube, Fuzz, Soft }), 14, 30);
                  band (g, HighMid, g.pick ({ Hard, Fuzz }), 10, 26, -6, 0); band (g, High, Soft, 0, 12, -12, -4);
                  g.set ("width", 100); g.range ("eqMidDb", -3, 3); g.range ("eqHighDb", -6, -1); } },
            { "DRUMS", [] (skazi::PresetGen& g) {
                  xover (g, 100, 200, 1000, 2500, 5000, 9000);
                  band (g, Low, Tube, 6, 16); band (g, LowMid, Soft, 6, 18, -4, 0);
                  band (g, HighMid, g.pick ({ Hard, Soft }), 8, 22); band (g, High, Soft, 4, 16);
                  g.range ("width", 100, 140); g.range ("eqLowDb", 0, 4); g.range ("eqHighDb", 0, 3); g.range ("mix", 60, 100); } },
            { "GUITAR", [] (skazi::PresetGen& g) {
                  // four fuzz boxes, one per band
                  xover (g, 120, 250, 700, 1500, 2500, 5000);
                  for (int b = 0; b < NumBands; ++b) band (g, b, g.pick ({ Fuzz, Fuzz, Hard }), 20, 40, -3, 1);
                  g.range ("width", 100, 130);
                  g.range ("eqMidDb", 1, 5); g.set ("eqMidF", std::round (g.rr (700, 1600))); g.range ("eqHighDb", -9, -4); g.set ("eqHighF", 6000); } },
            { "SYNTH", [] (skazi::PresetGen& g) {
                  xover (g, 150, 300, 900, 2200, 4000, 8000);
                  band (g, Low, Soft, 2, 10); band (g, LowMid, Tube, 8, 24);
                  band (g, HighMid, Soft, 10, 28); band (g, High, g.pick ({ Soft, Hard }), 6, 20, -4, 0);
                  g.range ("width", 120, 180); g.range ("eqHighDb", -2, 3); } },
            { "PSY LEAD", [] (skazi::PresetGen& g) {
                  xover (g, 200, 400, 1200, 3000, 5000, 10000);
                  band (g, Low, Soft, 0, 6, -8, -2); band (g, LowMid, Fuzz, 16, 32);
                  band (g, HighMid, Hard, 18, 36); band (g, High, Fuzz, 10, 28, -6, 0);
                  g.range ("width", 130, 190); g.range ("eqMidDb", 2, 6); g.set ("eqMidF", std::round (g.rr (1500, 3500))); g.range ("eqMidQ", 1, 3); } },
            { "MASTER GLUE", [] (skazi::PresetGen& g) {
                  xover (g, 100, 200, 1000, 2500, 5000, 10000);
                  for (int b = 0; b < NumBands; ++b) band (g, b, g.pick ({ Soft, Tube }), 1, 7, -1, 0.5f);
                  g.range ("width", 100, 125); g.range ("mix", 40, 80);
                  g.range ("eqLowDb", 0, 2); g.range ("eqHighDb", 0, 2.5f); } },
            { "WIDE & WILD", [] (skazi::PresetGen& g) { quad (g); g.range ("width", 160, 200); } },
            { "QUAD", quad },
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
        factory ("Clean Split", { { "loDrive", 0 }, { "lmDrive", 0 }, { "hmDrive", 0 }, { "hiDrive", 0 } });
        factory ("Quad Fuzz", { { "loMode", Fuzz }, { "lmMode", Fuzz }, { "hmMode", Fuzz }, { "hiMode", Fuzz },
                                 { "loDrive", 24 }, { "lmDrive", 30 }, { "hmDrive", 30 }, { "hiDrive", 24 }, { "out", -3 } });
        factory ("Bass Grit", { { "xLo", 120 }, { "loDrive", 0 }, { "lmMode", Tube }, { "lmDrive", 24 }, { "hmMode", Hard }, { "hmDrive", 18 },
                                { "hiDrive", 6 }, { "hiLevel", -8 }, { "width", 100 } });
        factory ("Drum Crush", { { "loMode", Tube }, { "loDrive", 12 }, { "hmMode", Hard }, { "hmDrive", 20 }, { "hiDrive", 10 }, { "mix", 70 } });
        factory ("Wide Fuzz", { { "lmMode", Fuzz }, { "hmMode", Fuzz }, { "lmDrive", 26 }, { "hmDrive", 26 }, { "width", 180 } });
        factory ("Warm Glue", { { "loMode", Tube }, { "lmMode", Tube }, { "hmMode", Tube }, { "hiMode", Tube },
                                { "loDrive", 4 }, { "lmDrive", 4 }, { "hmDrive", 4 }, { "hiDrive", 4 }, { "mix", 60 }, { "eqHighDb", 1.5f } });
        factory ("Telephone Lead", { { "loOn", 0 }, { "loLevel", -24 }, { "hiLevel", -24 }, { "lmMode", Hard }, { "lmDrive", 30 },
                                     { "hmMode", Fuzz }, { "hmDrive", 30 }, { "eqMidDb", 5 }, { "eqMidF", 1800 }, { "eqMidQ", 1.5f } });

        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < 12; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 5381 + 17);
                skazi::Preset p { skazi::presetName (g + 1100), cat.name, defs };
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
    skazi::Preset p { "Quad", "QUAD", skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    quad (gen);
    return p;
}
} // namespace quazi
