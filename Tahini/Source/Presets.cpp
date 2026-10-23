#include "Presets.h"
#include "TahiniEngine.h"

namespace tahini
{
namespace
{
    // Delay times (index into the note list): 0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16, 5 1/32, 6 1/2T, 7 1/4T,
    // 8 1/8T, 9 1/16T, 10 1/2D, 11 1/4D, 12 1/8D, 13 1/16D, 14 2/1, 15 4/1
    struct Category { const char* name; std::function<void (skazi::PresetGen&)> style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "PSY SPACE", [] (skazi::PresetGen& g) {
                  g.set ("dmode", PingPong); g.set ("dtime", (float) g.pick ({ 12, 3, 8, 13 })); g.range ("fb", 40, 65);
                  g.range ("locut", 200, 500); g.range ("hicut", 4000, 9000); g.range ("wow", 0, 20);
                  g.range ("size", 60, 90); g.range ("decay", 3, 8); g.range ("damp", 30, 60);
                  g.range ("dmix", 25, 45); g.range ("rmix", 25, 45); g.range ("duck", 40, 80); } },
            { "GOA CAVE", [] (skazi::PresetGen& g) {
                  g.set ("dmode", PingPong); g.set ("dtime", (float) g.pick ({ 13, 8, 12 })); g.range ("fb", 50, 75);
                  g.range ("locut", 150, 400); g.range ("hicut", 3000, 7000);
                  g.range ("size", 80, 100); g.range ("decay", 8, 16); g.range ("damp", 20, 40); g.range ("predelay", 10, 60);
                  g.range ("dmix", 30, 45); g.range ("rmix", 30, 50); g.range ("duck", 30, 70); } },
            { "DUB ECHO", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 11, 12, 2 })); g.range ("fb", 60, 85);
                  g.range ("locut", 100, 300); g.range ("hicut", 2500, 5000); g.range ("wow", 30, 60); g.range ("ddrive", 20, 50);
                  g.range ("size", 40, 70); g.range ("decay", 1.5f, 4); g.range ("dmix", 40, 60); g.range ("rmix", 10, 25); g.range ("duck", 20, 50); } },
            { "TAPE ECHO", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 3, 12, 8 })); g.range ("fb", 35, 60);
                  g.range ("locut", 150, 400); g.range ("hicut", 3000, 6000); g.range ("wow", 20, 50); g.range ("ddrive", 10, 40);
                  g.range ("size", 30, 60); g.range ("decay", 1, 3); g.range ("dmix", 30, 50); g.range ("rmix", 5, 20); g.range ("duck", 20, 60); } },
            { "VOCAL THROW", [] (skazi::PresetGen& g) {
                  g.set ("dmode", (float) g.pick ({ PingPong, Stereo })); g.set ("dtime", (float) g.pick ({ 2, 12, 11 })); g.range ("fb", 30, 50);
                  g.range ("locut", 300, 600); g.range ("hicut", 5000, 8000); g.range ("wow", 0, 15);
                  g.range ("size", 50, 80); g.range ("decay", 2, 5); g.range ("dmix", 30, 50); g.range ("rmix", 15, 30); g.range ("duck", 60, 90); } },
            { "BIG HALL", [] (skazi::PresetGen& g) {
                  g.range ("dmix", 0, 15); g.range ("fb", 20, 40); g.range ("size", 70, 100); g.range ("decay", 4, 12);
                  g.range ("predelay", 20, 80); g.range ("damp", 30, 60); g.range ("rmix", 30, 60); g.range ("duck", 0, 40); g.range ("width", 80, 100); } },
            { "ROOMS", [] (skazi::PresetGen& g) {
                  g.range ("dmix", 0, 10); g.range ("size", 10, 40); g.range ("decay", 0.4f, 1.5f); g.range ("predelay", 0, 20);
                  g.range ("damp", 30, 70); g.range ("rmix", 15, 35); g.range ("duck", 0, 30); g.range ("width", 60, 100); } },
            { "AMBIENT WASH", [] (skazi::PresetGen& g) {
                  g.set ("dmode", Tape); g.set ("dtime", (float) g.pick ({ 1, 10, 2 })); g.range ("fb", 50, 70); g.range ("wow", 20, 50);
                  g.range ("hicut", 3000, 6000); g.range ("size", 90, 100); g.range ("decay", 10, 20); g.range ("damp", 40, 70);
                  g.range ("dmix", 20, 40); g.range ("rmix", 50, 80); g.range ("duck", 20, 50); } },
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
        factory ("Goa Cave", { { "dtime", 13 }, { "fb", 65 }, { "size", 95 }, { "decay", 12 }, { "rmix", 40 } });
        factory ("Dub Siren Echo", { { "dmode", Tape }, { "dtime", 11 }, { "fb", 80 }, { "wow", 45 }, { "hicut", 3500 } });
        factory ("Clean Ping-Pong", { { "rmix", 0 }, { "fb", 45 }, { "duck", 0 } });
        factory ("Big Hall", { { "dmix", 0 }, { "size", 90 }, { "decay", 7 }, { "rmix", 45 } });
        factory ("Send Only (no dry)", { { "dry", 0 }, { "dmix", 100 }, { "rmix", 100 }, { "duck", 0 } });

        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < 14; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 3571 + 77);
                skazi::Preset p { skazi::presetName (g + 400), cat.name, defs };
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
    const auto& cats = categories();
    const auto& cat = cats[(size_t) rng.nextInt ((int) cats.size())];
    skazi::Preset p { "Random", cat.name, skazi::defaultValues (paramDefs()) };
    juce::Random r (rng.nextInt64());
    skazi::PresetGen gen { r, p.values };
    cat.style (gen);
    return p;
}
} // namespace tahini
