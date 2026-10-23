#include "Presets.h"

namespace presets
{
namespace
{
    constexpr int kPerCategory = 12;

    // Rates: 0 4/1, 1 2/1, 2 1/1, 3 1/2, 4 1/4, 5 1/8, 6 1/16, 7 1/32, 8 1/2T, 9 1/4T, 10 1/8T,
    //        11 1/16T, 12 1/2D, 13 1/4D, 14 1/8D, 15 1/16D
    // Shapes: 0 Pump, 1 Hard Cut, 2 Smooth, 3 Expo, 4 Log, 5 Saw Down, 6 Triangle, 7 Gate, 8 Bounce
    struct Profile
    {
        const char* name;
        std::vector<int> rates, shapes;
        float depthLo, depthHi, curveLo, curveHi, attackLo, attackHi, releaseLo, releaseHi, smoothLo, smoothHi, swingLo, swingHi;
    };

    const std::vector<Profile>& profiles()
    {
        static const std::vector<Profile> p = {
            { "PSY",     { 4, 4, 6 },        { 0, 1, 3 },       70, 100, 40, 70, 0, 5,  50, 90,  1, 4,   50, 50 },
            { "HOUSE",   { 4 },              { 2, 0, 4 },       50, 85,  35, 60, 0, 8,  70, 100, 3, 8,   50, 56 },
            { "TRANCE",  { 5, 6, 4 },        { 0, 7, 2 },       60, 95,  40, 65, 0, 4,  60, 100, 1, 4,   50, 54 },
            { "TECHNO",  { 4, 5 },           { 1, 0, 3 },       75, 100, 45, 75, 0, 10, 40, 80,  1, 3,   50, 52 },
            { "TRAP",    { 9, 10, 11, 5 },   { 7, 0, 8 },       60, 95,  40, 70, 0, 6,  50, 100, 1, 4,   50, 60 },
            { "DUBSTEP", { 3, 12, 13, 8 },   { 3, 4, 2, 8 },    70, 100, 30, 65, 0, 15, 60, 100, 2, 10,  50, 50 },
            { "CHOPS",   { 6, 7, 11, 15 },   { 7, 1, 6 },       80, 100, 45, 75, 0, 2,  50, 100, 0.5f, 2, 50, 62 },
            { "SWELLS",  { 2, 1, 3, 0 },     { 5, 6, 4, 3, 2 }, 40, 90,  30, 70, 0, 30, 80, 100, 5, 20,  50, 50 },
        };
        return p;
    }

    const char* const kAdjectives[] = { "Rolling", "Deep", "Hyper", "Fat", "Liquid", "Golden", "Wild", "Midnight",
                                         "Crispy", "Heavy", "Smooth", "Desert", "Electric", "Spicy", "Velvet", "Toxic",
                                         "Solar", "Shadow", "Neon", "Cosmic", "Rusty", "Atomic", "Frozen", "Burning", "Chrome" };
    const char* const kNouns[] = { "Kick", "Pita", "Bounce", "Wave", "Engine", "Pulse", "Breath", "Hummus", "Ride",
                                   "Tahini", "Groove", "Falafel", "Bass", "Machine", "Heart", "Pump", "Swing", "Lung",
                                   "Motor", "Tide", "Piston", "Drum", "Ritual", "Rocket", "Spice" };

    std::map<juce::String, float> defaults()
    {
        return { { "trigger", 0 }, { "rate", 4 }, { "snap", 3 }, { "phase", 0 }, { "swing", 50 }, { "thresh", -24 },
                 { "shape", 0 }, { "depth", 85 }, { "curve", 50 }, { "attack", 0 }, { "release", 100 }, { "smooth", 2 },
                 { "pregain", 0 }, { "auto", 0 }, { "mix", 100 }, { "gain", 0 } };
    }

    float range (juce::Random& r, float lo, float hi) { return std::round ((lo + r.nextFloat() * (hi - lo)) * 10.0f) / 10.0f; }
    int pick (juce::Random& r, const std::vector<int>& v) { return v[(size_t) r.nextInt ((int) v.size())]; }

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
            factory ("Classic Pump", { { "shape", 0 }, { "depth", 90 } }),
            factory ("Hard Duck", { { "shape", 1 }, { "depth", 100 }, { "smooth", 1 } }),
            factory ("Smooth House", { { "shape", 2 }, { "depth", 70 }, { "smooth", 5 } }),
            factory ("Psy Gallop", { { "shape", 3 }, { "rate", 6 }, { "depth", 80 }, { "release", 70 } }),
            factory ("Trance Gate", { { "shape", 7 }, { "rate", 5 }, { "depth", 100 }, { "smooth", 1 } }),
            factory ("Kick Follower", { { "trigger", 2 }, { "shape", 0 }, { "depth", 90 }, { "thresh", -20 } }),
            factory ("MIDI Ducker", { { "trigger", 1 }, { "shape", 3 }, { "depth", 90 } }),
            factory ("Quiet Fixer", { { "pregain", 6 }, { "auto", 1 }, { "depth", 85 } }),
        };

        int g = 0;
        for (auto& pr : profiles())
            for (int i = 0; i < kPerCategory; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 6151 + 4099);
                // (g % 25, (g / 25 + g) % 25) never repeats for g < 625, so every name is unique
                Preset p { juce::String (kAdjectives[g % 25]) + " " + kNouns[(g / 25 + g) % 25], pr.name, defaults() };
                auto& v = p.values;
                v["rate"]    = (float) pick (r, pr.rates);
                v["shape"]   = (float) pick (r, pr.shapes);
                v["depth"]   = range (r, pr.depthLo, pr.depthHi);
                v["curve"]   = range (r, pr.curveLo, pr.curveHi);
                v["attack"]  = range (r, pr.attackLo, pr.attackHi);
                v["release"] = range (r, pr.releaseLo, pr.releaseHi);
                v["smooth"]  = range (r, pr.smoothLo, pr.smoothHi);
                v["swing"]   = range (r, pr.swingLo, pr.swingHi);
                v["mix"]     = r.nextFloat() < 0.2f ? range (r, 60, 90) : 100.0f;
                list.push_back (std::move (p));
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
    v["rate"]    = (float) pick (r, { 3, 4, 4, 4, 5, 5, 6, 9, 10, 13, 14 });
    v["shape"]   = (float) r.nextInt (9);
    v["depth"]   = range (r, 50, 100);
    v["curve"]   = range (r, 30, 75);
    v["attack"]  = r.nextFloat() < 0.6f ? 0.0f : range (r, 1, 15);
    v["release"] = range (r, 40, 100);
    v["smooth"]  = range (r, 0.5f, 8);
    v["swing"]   = r.nextFloat() < 0.6f ? 50.0f : range (r, 52, 66);
    return p;
}
} // namespace presets
