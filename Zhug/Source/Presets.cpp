#include "Presets.h"
#include "ZhugEngine.h"

namespace zhug
{
namespace
{
    struct Category { const char* name; std::function<void (skazi::PresetGen&)> style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "HOT LEADS", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Tube, Transistor, Fuzz })); g.range ("drive", 10, 24); g.range ("tone", 0, 40);
                  g.range ("bias", 0, 40); g.range ("width", 110, 160); g.range ("haas", 0, 8); g.range ("mix", 70, 100); } },
            { "BASS GRIT", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Tube, Tape, Transistor })); g.set ("mb", 1); g.range ("drive", 6, 18);
                  g.range ("lowd", 20, 45); g.range ("midd", 60, 100); g.range ("highd", 40, 80); g.range ("xlow", 100, 200);
                  g.range ("xhigh", 1500, 4000); g.range ("width", 80, 110); g.range ("bassmono", 120, 250); g.range ("mix", 70, 100); } },
            { "DRUM CRUNCH", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Transistor, Digital, Tape })); g.range ("drive", 8, 20); g.range ("tone", -10, 30);
                  g.range ("bias", 0, 20); g.range ("mix", 40, 80); g.range ("width", 90, 120); } },
            { "MASTER WARMTH", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Tape, Tube })); g.range ("drive", 1, 6); g.range ("tone", -10, 10); g.range ("bias", 0, 15);
                  g.range ("width", 100, 120); g.range ("bassmono", 80, 150); g.range ("mix", 60, 100); } },
            { "LO-FI", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ BitCrush, BitCrush, Digital })); g.range ("drive", 14, 30); g.range ("tone", -60, -10);
                  g.range ("width", 60, 100); g.range ("mix", 50, 100); } },
            { "WIDE PADS", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Tape, Tube })); g.range ("drive", 2, 9); g.range ("tone", -10, 20);
                  g.range ("width", 150, 200); g.range ("bassmono", 150, 300); g.range ("haas", 5, 15); g.range ("mix", 80, 100); } },
            { "FUZZ & FOLD", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Fuzz, Fold, Rectify })); g.range ("drive", 12, 30); g.range ("bias", 10, 70);
                  g.range ("tone", -30, 30); g.range ("mix", 50, 100); g.range ("width", 90, 140); } },
            { "VOCAL HEAT", [] (skazi::PresetGen& g) {
                  g.set ("mode", (float) g.pick ({ Tube, Tape })); g.range ("drive", 3, 12); g.range ("tone", 5, 35); g.set ("mb", (float) g.pick ({ 0, 1 }));
                  g.range ("lowd", 10, 40); g.range ("midd", 50, 80); g.range ("highd", 30, 60); g.range ("mix", 50, 90); g.range ("width", 100, 130); } },
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
        factory ("Spicy Lead", { { "mode", Tube }, { "drive", 18 }, { "width", 140 }, { "tone", 20 } });
        factory ("Bass Grit", { { "mode", Transistor }, { "mb", 1 }, { "lowd", 30 }, { "midd", 85 }, { "bassmono", 180 } });
        factory ("Tape Glue", { { "mode", Tape }, { "drive", 4 }, { "mix", 80 } });
        factory ("Crushed", { { "mode", BitCrush }, { "drive", 24 }, { "tone", -30 } });
        factory ("Super Wide", { { "mode", Tape }, { "drive", 3 }, { "width", 190 }, { "haas", 10 }, { "bassmono", 200 } });

        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < 14; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 2909 + 55);
                skazi::Preset p { skazi::presetName (g + 600), cat.name, defs };
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
} // namespace zhug
