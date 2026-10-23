#pragma once

// Shared non-UI helpers for SKAZI plugins: presets, parameter helpers, note lengths, host tempo.
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>

namespace skazi
{
// Values in parameter units keyed by parameter ID
struct Preset
{
    juce::String name, category;
    std::map<juce::String, float> values;
};

// Unique for g < 1600: (g % 40, (g / 40 + g) % 40) never repeats in that range
inline juce::String presetName (int g)
{
    static const char* const adj[40] = { "Rolling", "Deep", "Hyper", "Fat", "Liquid", "Golden", "Wild", "Midnight", "Crispy", "Heavy",
                                         "Smooth", "Desert", "Electric", "Spicy", "Velvet", "Toxic", "Solar", "Shadow", "Neon", "Cosmic",
                                         "Rusty", "Atomic", "Frozen", "Burning", "Chrome", "Dirty", "Funky", "Dark", "Twisted", "Bouncy",
                                         "Rubber", "Steel", "Acid", "Lunar", "Savage", "Hollow", "Gritty", "Silky", "Sharp", "Lazy" };
    static const char* const noun[40] = { "Rider", "Pulse", "Engine", "Wave", "Groove", "Machine", "Motor", "Snake", "Tiger", "Ritual",
                                          "Comet", "Thunder", "Signal", "Rocket", "Temple", "Spiral", "Voltage", "Circuit", "Ghost", "Vortex",
                                          "Falafel", "Hummus", "Pepper", "Bazooka", "Prophet", "Jungle", "Mirage", "Riot", "Piston", "Drum",
                                          "Lung", "Heart", "Tide", "Swing", "Bounce", "Hammer", "Skillet", "Tahini", "Sunrise", "Spice" };
    return juce::String (adj[g % 40]) + " " + noun[(g / 40 + g) % 40];
}


// Parameter helpers
inline std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* id, const char* name, float lo, float hi, float def,
                                                               const char* label, float step = 0.01f, float skew = 1.0f)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step, skew),
                                                        def, juce::AudioParameterFloatAttributes().withLabel (label));
}

inline std::unique_ptr<juce::AudioParameterChoice> choiceParam (const char* id, const char* name, const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}

inline std::unique_ptr<juce::AudioParameterBool> boolParam (const char* id, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

inline std::unique_ptr<juce::AudioParameterInt> intParam (const char* id, const char* name, int lo, int hi, int def)
{
    return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def);
}

// Musical note lengths shared by the synced controls
struct NoteLength { const char* name; double beats; };
inline const std::vector<NoteLength>& noteLengths()
{
    static const std::vector<NoteLength> n = {
        { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
        { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 },
        { "1/2D", 3.0 }, { "1/4D", 1.5 }, { "1/8D", 0.75 }, { "1/16D", 0.375 }, { "2/1", 8.0 }, { "4/1", 16.0 },
    };
    return n;
}

inline juce::StringArray noteLengthNames()
{
    juce::StringArray s;
    for (auto& n : noteLengths()) s.add (n.name);
    return s;
}

inline double noteBeats (int index) { return noteLengths()[(size_t) juce::jlimit (0, (int) noteLengths().size() - 1, index)].beats; }

// Tempo and position from the host (120 bpm and not playing when unavailable)
struct HostTime { double bpm = 120.0, ppq = 0.0, beatsPerBar = 4.0; bool playing = false; };
inline HostTime hostTime (juce::AudioPlayHead* ph)
{
    HostTime t;
    if (ph != nullptr)
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) t.bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; t.playing = pos->getIsPlaying(); }
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0) t.beatsPerBar = ts->numerator * 4.0 / ts->denominator;
        }
    return t;
}

// One table drives both the plugin parameters and the preset defaults, so they never drift apart
struct ParamDef
{
    enum Kind { Float, Int, Choice, Bool };
    const char* id;
    const char* name;
    Kind kind;
    float lo, hi, def;
    const char* label = "";
    float step = 0.01f, skew = 1.0f;
    juce::StringArray choices = {};
};

inline juce::AudioProcessorValueTreeState::ParameterLayout buildLayout (const std::vector<ParamDef>& defs)
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : defs)
    {
        switch (d.kind)
        {
            case ParamDef::Float:  l.add (floatParam (d.id, d.name, d.lo, d.hi, d.def, d.label, d.step, d.skew)); break;
            case ParamDef::Int:    l.add (intParam (d.id, d.name, (int) d.lo, (int) d.hi, (int) d.def)); break;
            case ParamDef::Choice: l.add (choiceParam (d.id, d.name, d.choices, (int) d.def)); break;
            case ParamDef::Bool:   l.add (boolParam (d.id, d.name, d.def > 0.5f)); break;
        }
    }
    return l;
}

inline std::map<juce::String, float> defaultValues (const std::vector<ParamDef>& defs)
{
    std::map<juce::String, float> v;
    for (auto& d : defs) v[d.id] = d.def;
    return v;
}

// Seeded helpers for generating preset values
struct PresetGen
{
    juce::Random& r;
    std::map<juce::String, float>& v;
    float rr (float lo, float hi) { return std::round ((lo + r.nextFloat() * (hi - lo)) * 10.0f) / 10.0f; }
    int pick (std::initializer_list<int> l) { return *(l.begin() + r.nextInt ((int) l.size())); }
    bool chance (float p) { return r.nextFloat() < p; }
    void set (const char* id, float x) { v[id] = x; }
    void range (const char* id, float lo, float hi) { v[id] = rr (lo, hi); }
};

inline float safeClip (float x)
{
    const float a = std::abs (x);
    if (a <= 0.95f) return x;
    return std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), x);
}
} // namespace skazi
