#pragma once

// SUBICH parameter table: used to build the plugin parameters and the preset defaults,
// so the two can never drift apart.
#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace subich
{
enum Wave { Sine, Tri, Saw, Square, Pulse, FM, Noise, NumWaves };
enum FilterType { LP24, LP12, HP12, BP, Notch, Acid };
enum LfoShape { LSine, LTri, LSaw, LSquare, LSH };
enum LfoDest { DCut1, DCut2, DCutBoth, DPitch, DShape, DVolume, DReso1 };
enum VoiceMode { Mono, Legato, Poly };

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

inline const std::vector<ParamDef>& paramDefs()
{
    const juce::StringArray waves { "SINE", "TRI", "SAW", "SQUARE", "PULSE", "FM", "NOISE" };
    const juce::StringArray filters { "LP 24", "LP 12", "HP 12", "BP", "NOTCH", "ACID" };
    const juce::StringArray lfoShapes { "SINE", "TRI", "SAW", "SQUARE", "S&H" };
    const juce::StringArray dests { "CUTOFF 1", "CUTOFF 2", "BOTH CUTOFFS", "PITCH", "OSC SHAPE", "VOLUME", "RESO 1" };
    const juce::StringArray rates { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2T", "1/4T", "1/8T", "1/16T",
                                    "1/2D", "1/4D", "1/8D", "1/16D", "2/1", "4/1" };
    using P = ParamDef;
    static const std::vector<ParamDef> defs = {
        { "o1wave", "Osc 1 Wave", P::Choice, 0, 6, 2, "", 1, 1, waves },
        { "o1oct", "Osc 1 Octave", P::Int, -3, 2, -1 },
        { "o1semi", "Osc 1 Semi", P::Int, -12, 12, 0 },
        { "o1fine", "Osc 1 Fine", P::Float, -100, 100, 0, "ct", 1 },
        { "o1shape", "Osc 1 Shape", P::Float, 0, 100, 0, "%", 0.1f },
        { "o1level", "Osc 1 Level", P::Float, 0, 100, 80, "%", 0.1f },

        { "o2wave", "Osc 2 Wave", P::Choice, 0, 6, 3, "", 1, 1, waves },
        { "o2oct", "Osc 2 Octave", P::Int, -3, 2, -1 },
        { "o2semi", "Osc 2 Semi", P::Int, -12, 12, 0 },
        { "o2detune", "Osc 2 Detune", P::Float, -100, 100, 7, "ct", 1 },
        { "o2shape", "Osc 2 Shape", P::Float, 0, 100, 50, "%", 0.1f },
        { "o2level", "Osc 2 Level (FM depth when FM is on)", P::Float, 0, 100, 0, "%", 0.1f },
        { "o2sync", "Osc 2 Hard Sync", P::Bool, 0, 1, 0 },
        { "o2fm", "Osc 2 FMs Osc 1", P::Bool, 0, 1, 0 },

        { "subwave", "Sub Wave", P::Choice, 0, 1, 0, "", 1, 1, { "SINE", "TRI" } },
        { "suboct", "Sub Octave", P::Choice, 0, 1, 0, "", 1, 1, { "-1", "-2" } },
        { "sublevel", "Sub Level", P::Float, 0, 100, 60, "%", 0.1f },
        { "noise", "Noise", P::Float, 0, 100, 0, "%", 0.1f },

        { "f1type", "Filter 1 Type", P::Choice, 0, 5, 0, "", 1, 1, filters },
        { "f1cut", "Filter 1 Cutoff", P::Float, 20, 18000, 420, "Hz", 1, 0.25f },
        { "f1res", "Filter 1 Reso", P::Float, 0, 100, 35, "%", 0.1f },
        { "f1env", "Filter 1 Env", P::Float, -100, 100, 60, "%", 0.1f },
        { "f1drive", "Filter 1 Drive", P::Float, 0, 100, 20, "%", 0.1f },
        { "f1key", "Filter 1 Key Track", P::Float, 0, 100, 50, "%", 0.1f },

        { "f2type", "Filter 2 Type", P::Choice, 0, 5, 2, "", 1, 1, filters },
        { "f2cut", "Filter 2 Cutoff", P::Float, 20, 18000, 35, "Hz", 1, 0.25f },
        { "f2res", "Filter 2 Reso", P::Float, 0, 100, 20, "%", 0.1f },
        { "f2env", "Filter 2 Env", P::Float, -100, 100, 0, "%", 0.1f },
        { "f2drive", "Filter 2 Drive", P::Float, 0, 100, 0, "%", 0.1f },
        { "f2key", "Filter 2 Key Track", P::Float, 0, 100, 0, "%", 0.1f },
        { "routing", "Filter Routing", P::Choice, 0, 1, 0, "", 1, 1, { "SER", "PAR" } },

        { "aa", "Amp Attack", P::Float, 0, 2000, 1, "ms", 0.1f, 0.3f },
        { "ad", "Amp Decay", P::Float, 1, 3000, 180, "ms", 0.1f, 0.3f },
        { "as", "Amp Sustain", P::Float, 0, 100, 80, "%", 0.1f },
        { "ar", "Amp Release", P::Float, 1, 3000, 60, "ms", 0.1f, 0.3f },
        { "fa", "Filter Attack", P::Float, 0, 2000, 0, "ms", 0.1f, 0.3f },
        { "fd", "Filter Decay", P::Float, 1, 3000, 120, "ms", 0.1f, 0.3f },
        { "fs", "Filter Sustain", P::Float, 0, 100, 10, "%", 0.1f },
        { "fr", "Filter Release", P::Float, 1, 3000, 40, "ms", 0.1f, 0.3f },

        { "l1shape", "LFO 1 Shape", P::Choice, 0, 4, 0, "", 1, 1, lfoShapes },
        { "l1sync", "LFO 1 Sync", P::Bool, 0, 1, 1 },
        { "l1rate", "LFO 1 Rate (sync)", P::Choice, 0, 15, 3, "", 1, 1, rates },
        { "l1hz", "LFO 1 Rate (Hz)", P::Float, 0.05f, 20, 2, "Hz", 0.01f, 0.35f },
        { "l1depth", "LFO 1 Depth", P::Float, 0, 100, 0, "%", 0.1f },
        { "l1dest", "LFO 1 Target", P::Choice, 0, 6, 0, "", 1, 1, dests },

        { "l2shape", "LFO 2 Shape", P::Choice, 0, 4, 4, "", 1, 1, lfoShapes },
        { "l2sync", "LFO 2 Sync", P::Bool, 0, 1, 0 },
        { "l2rate", "LFO 2 Rate (sync)", P::Choice, 0, 15, 4, "", 1, 1, rates },
        { "l2hz", "LFO 2 Rate (Hz)", P::Float, 0.05f, 20, 0.8f, "Hz", 0.01f, 0.35f },
        { "l2depth", "LFO 2 Depth", P::Float, 0, 100, 0, "%", 0.1f },
        { "l2dest", "LFO 2 Target", P::Choice, 0, 6, 3, "", 1, 1, dests },

        { "mode", "Voice Mode", P::Choice, 0, 2, 0, "", 1, 1, { "MONO", "LEGATO", "POLY" } },
        { "glide", "Glide", P::Float, 0, 1000, 0, "ms", 0.1f, 0.4f },
        { "velo", "Velocity", P::Float, 0, 100, 50, "%", 0.1f },
        { "bend", "Bend Range", P::Int, 0, 12, 2 },
        { "drive", "Drive", P::Float, 0, 100, 25, "%", 0.1f },
        { "vol", "Volume", P::Float, -24, 6, -3, "dB", 0.1f },
    };
    return defs;
}

inline std::map<juce::String, float> defaultValues()
{
    std::map<juce::String, float> v;
    for (auto& d : paramDefs())
        v[d.id] = d.def;
    return v;
}
} // namespace subich
