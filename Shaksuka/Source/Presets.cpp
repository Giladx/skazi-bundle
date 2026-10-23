#include "Presets.h"

namespace presets
{
namespace
{
    constexpr int kNumFx = 11; // Repeat Stutter Reverse Scramble Tape Pitch Crush Filter Gate Ring Pan
    constexpr float kDefaultWeights[kNumFx] = { 80, 70, 60, 40, 30, 40, 40, 60, 50, 20, 40 };
    constexpr int kPerCategory = 50;

    // Grid indices: 0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16, 5 1/32, 6 1/64, 7 1/2T, 8 1/4T,
    //               9 1/8T, 10 1/16T, 11 1/32T, 12 1/2D, 13 1/4D, 14 1/8D, 15 1/16D
    // Grooves: 0 Straight, 1 Swing, 2 Shuffle, 3 Drunk, 4 Push
    struct Profile
    {
        const char* name;
        std::vector<int> grids, grooves;
        float chaosLo, chaosHi;
        int stackLo, stackHi;
        float evolveLo, evolveHi, lenLo, lenHi, swingLo, swingHi, humLo, humHi;
        float w[kNumFx]; // 0..1 how likely each effect is in this style
    };

    const std::vector<Profile>& profiles()
    {
        static const std::vector<Profile> p = {
            { "PSY STUTTER", { 4, 5, 4, 10 }, { 0, 0, 4 }, 55, 85, 1, 3, 10, 40, 10, 35, 52, 58, 0, 5,
              { .7f, 1, .3f, .2f, .1f, .3f, .2f, .7f, .9f, .15f, .4f } },
            { "GLITCH", { 5, 6, 11, 10, 4 }, { 0, 3, 4 }, 75, 100, 3, 5, 30, 80, 20, 50, 52, 64, 0, 10,
              { .7f, .9f, .6f, .7f, .4f, .6f, .7f, .6f, .7f, .5f, .5f } },
            { "TAPE & PITCH", { 3, 2, 14, 13 }, { 0, 1 }, 45, 75, 1, 2, 10, 30, 30, 60, 54, 62, 0, 10,
              { .3f, .1f, .6f, .3f, 1, .8f, .2f, .5f, .1f, .05f, .3f } },
            { "DUB & SPACE", { 3, 14, 2, 15 }, { 1, 2 }, 40, 70, 1, 3, 10, 35, 20, 50, 55, 66, 0, 10,
              { .3f, .1f, .8f, .7f, .3f, .3f, .05f, .9f, .2f, 0, .8f } },
            { "LO-FI", { 4, 3, 15 }, { 1, 2, 3 }, 50, 80, 2, 4, 10, 40, 10, 40, 54, 64, 5, 20,
              { .5f, .3f, .3f, .3f, .2f, .4f, 1, .5f, .5f, .4f, .1f } },
            { "BREAKS", { 4, 3, 9 }, { 1, 2 }, 50, 80, 1, 3, 15, 45, 15, 45, 56, 70, 0, 15,
              { .9f, .5f, .7f, .9f, .2f, .3f, .2f, .4f, .3f, .1f, .3f } },
            { "TECHNO GATE", { 4, 5, 3 }, { 0, 1 }, 50, 85, 1, 3, 10, 35, 0, 30, 50, 56, 0, 5,
              { .5f, .4f, .2f, .2f, .1f, .1f, .2f, .9f, 1, .3f, .6f } },
            { "TRAP ROLLS", { 10, 11, 9, 5 }, { 0, 2 }, 50, 85, 1, 3, 15, 45, 10, 40, 50, 60, 0, 10,
              { .6f, 1, .3f, .2f, .4f, .8f, .3f, .4f, .6f, .1f, .3f } },
            { "DRUNK GROOVE", { 4, 3, 9, 15 }, { 3 }, 45, 75, 1, 3, 15, 45, 15, 45, 60, 78, 15, 45,
              { .7f, .6f, .6f, .5f, .3f, .4f, .3f, .5f, .4f, .2f, .5f } },
            { "EXTREME", { 5, 6, 11, 4, 10 }, { 0, 3, 4, 2 }, 85, 100, 4, 5, 50, 100, 30, 70, 50, 70, 0, 25,
              { .9f, .9f, .9f, .9f, .8f, .9f, .9f, .9f, .9f, .8f, .9f } },
        };
        return p;
    }

    const char* const kAdjectives[] = { "Neon", "Acid", "Cosmic", "Rusty", "Liquid", "Broken", "Hyper", "Dusty",
                                         "Atomic", "Frozen", "Twisted", "Electric", "Midnight", "Savage", "Golden",
                                         "Toxic", "Velvet", "Chrome", "Wild", "Desert", "Shadow", "Solar",
                                         "Crystal", "Burning", "Spicy" };
    const char* const kNouns[] = { "Hammer", "Ritual", "Pulse", "Engine", "Mirage", "Riot", "Spiral", "Voltage",
                                   "Machine", "Prophet", "Circuit", "Tiger", "Comet", "Jungle", "Ghost", "Vortex",
                                   "Rocket", "Temple", "Serpent", "Signal", "Thunder", "Pepper", "Sunrise",
                                   "Bazooka", "Skillet" };

    std::map<juce::String, float> defaults()
    {
        std::map<juce::String, float> v {
            { "free", 0 }, { "grid", 4 }, { "freems", 125 }, { "groove", 0 }, { "swing", 58 }, { "humanize", 0 },
            { "bars", 0 }, { "lenvar", 30 }, { "chaos", 60 }, { "stack", 2 }, { "evolve", 20 }, { "seed", 427 },
            { "mix", 100 }, { "gain", 0 },
        };
        for (int i = 0; i < kNumFx; ++i)
        {
            v["on" + juce::String (i)] = 1;
            v["w" + juce::String (i)] = kDefaultWeights[i];
        }
        return v;
    }

    float range (juce::Random& r, float lo, float hi) { return std::round (lo + r.nextFloat() * (hi - lo)); }

    template <typename T>
    T pick (juce::Random& r, const std::vector<T>& v) { return v[(size_t) r.nextInt ((int) v.size())]; }

    // Turns some effects off (keeping at least three on) so presets differ in character, not just amounts
    void setEffects (juce::Random& r, std::map<juce::String, float>& v, const float* style, float offChance)
    {
        int on = 0;
        for (int i = 0; i < kNumFx; ++i)
        {
            const float w = juce::jlimit (0.0f, 100.0f, std::round (100.0f * style[i] * (0.5f + r.nextFloat()) + r.nextFloat() * 10.0f));
            v["w" + juce::String (i)] = w;
            const bool enabled = w > 0.0f && r.nextFloat() >= offChance;
            v["on" + juce::String (i)] = enabled ? 1.0f : 0.0f;
            on += enabled ? 1 : 0;
        }
        for (int i = 0; on < 3 && i < kNumFx; ++i)
        {
            const int fx = r.nextInt (kNumFx);
            if (v["on" + juce::String (fx)] < 0.5f)
            {
                v["on" + juce::String (fx)] = 1;
                v["w" + juce::String (fx)] = juce::jmax (v["w" + juce::String (fx)], 40.0f);
                ++on;
            }
        }
    }

    Preset factory (const char* name, std::initializer_list<std::pair<const char*, float>> changes)
    {
        Preset p { name, "FACTORY", defaults() };
        for (auto& [id, value] : changes)
            p.values[id] = value;
        return p;
    }

    std::vector<Preset> build()
    {
        std::vector<Preset> list = {
            factory ("Init", {}),
            factory ("Psy Stutter", { { "grid", 4 }, { "chaos", 70 }, { "stack", 2 },
                                      { "w0", 60 }, { "w1", 100 }, { "w2", 30 }, { "w3", 10 }, { "w4", 10 }, { "w5", 20 },
                                      { "w6", 20 }, { "w7", 60 }, { "w8", 80 }, { "w9", 10 }, { "w10", 30 } }),
            factory ("Tape Madness", { { "grid", 3 }, { "chaos", 65 }, { "lenvar", 50 },
                                       { "w0", 20 }, { "w1", 10 }, { "w2", 50 }, { "w3", 20 }, { "w4", 100 }, { "w5", 70 },
                                       { "w6", 20 }, { "w7", 40 }, { "w8", 10 }, { "w9", 0 }, { "w10", 20 } }),
            factory ("Glitch Storm", { { "grid", 5 }, { "chaos", 90 }, { "stack", 4 }, { "evolve", 60 }, { "lenvar", 40 },
                                       { "w0", 70 }, { "w1", 70 }, { "w2", 70 }, { "w3", 70 }, { "w4", 70 }, { "w5", 70 },
                                       { "w6", 70 }, { "w7", 70 }, { "w8", 70 }, { "w9", 70 }, { "w10", 70 } }),
            factory ("Dub Chop", { { "grid", 3 }, { "groove", 1 }, { "swing", 62 }, { "chaos", 50 },
                                   { "w0", 30 }, { "w1", 10 }, { "w2", 70 }, { "w3", 70 }, { "w4", 20 }, { "w5", 30 },
                                   { "w6", 0 }, { "w7", 80 }, { "w8", 20 }, { "w9", 0 }, { "w10", 70 } }),
            factory ("Lo-Fi Crush", { { "grid", 4 }, { "chaos", 60 }, { "stack", 3 },
                                      { "w0", 50 }, { "w1", 30 }, { "w2", 20 }, { "w3", 20 }, { "w4", 10 }, { "w5", 20 },
                                      { "w6", 100 }, { "w7", 30 }, { "w8", 50 }, { "w9", 40 }, { "w10", 10 } }),
            factory ("Chaos Grid", { { "grid", 5 }, { "humanize", 10 }, { "chaos", 75 }, { "evolve", 50 } }),
            factory ("Drunk Groove", { { "grid", 4 }, { "groove", 3 }, { "swing", 70 }, { "chaos", 55 },
                                       { "w0", 80 }, { "w1", 70 }, { "w2", 60 }, { "w3", 30 }, { "w4", 0 }, { "w5", 10 } }),
            factory ("Shuffle Slice", { { "grid", 4 }, { "groove", 2 }, { "chaos", 50 }, { "lenvar", 20 } }),
        };

        int g = 0; // global index across all generated presets
        for (size_t c = 0; c < profiles().size(); ++c)
        {
            const auto& pr = profiles()[c];
            for (int i = 0; i < kPerCategory; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 7919 + 104729);
                Preset p;
                // (g % 25, (g / 25 + g) % 25) never repeats for g < 625, so every name is unique
                p.name = juce::String (kAdjectives[g % 25]) + " " + kNouns[(g / 25 + g) % 25];
                p.category = pr.name;
                p.values = defaults();
                auto& v = p.values;
                v["grid"]     = (float) pick (r, pr.grids);
                v["groove"]   = (float) pick (r, pr.grooves);
                v["swing"]    = range (r, pr.swingLo, pr.swingHi);
                v["humanize"] = range (r, pr.humLo, pr.humHi);
                v["bars"]     = (float) pick (r, std::vector<int> { 0, 0, 1, 2 });
                v["lenvar"]   = range (r, pr.lenLo, pr.lenHi);
                v["chaos"]    = range (r, pr.chaosLo, pr.chaosHi);
                v["stack"]    = (float) (pr.stackLo + r.nextInt (pr.stackHi - pr.stackLo + 1));
                v["evolve"]   = range (r, pr.evolveLo, pr.evolveHi);
                v["seed"]     = (float) (1 + (g * 37) % 9999); // 37 is coprime with 9999, so seeds never repeat
                v["mix"]      = r.nextFloat() < 0.15f ? range (r, 70, 90) : 100.0f;
                setEffects (r, v, pr.w, 0.2f);
                list.push_back (std::move (p));
            }
        }
        return list;
    }
} // namespace

const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = build();
    return list;
}

juce::StringArray categories()
{
    juce::StringArray s { "FACTORY" };
    for (auto& p : profiles())
        s.add (p.name);
    return s;
}

Preset makeRandom (juce::Random& r)
{
    Preset p { "Random", "RANDOM", defaults() };
    auto& v = p.values;
    v["free"]     = 0; // RANDOM always stays on the grid
    v["grid"]     = (float) pick (r, std::vector<int> { 2, 3, 3, 4, 4, 4, 5, 5, 6, 9, 10, 11, 14, 15 });
    v["groove"]   = (float) r.nextInt (5);
    v["swing"]    = range (r, 52, 72);
    v["humanize"] = r.nextFloat() < 0.6f ? 0.0f : range (r, 5, 30);
    v["bars"]     = (float) r.nextInt (3);
    v["lenvar"]   = range (r, 0, 60);
    v["chaos"]    = range (r, 40, 100);
    v["stack"]    = (float) (1 + r.nextInt (5));
    v["evolve"]   = range (r, 0, 70);
    v["seed"]     = (float) (1 + r.nextInt (9999));
    float style[kNumFx];
    for (auto& s : style)
        s = r.nextFloat();
    setEffects (r, v, style, 0.25f);
    return p;
}
} // namespace presets
