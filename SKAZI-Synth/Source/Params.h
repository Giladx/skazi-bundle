#pragma once
#include <juce_core/juce_core.h>

// Choice lists and modulation enums shared by the processor, presets and editor.
namespace Lists
{
    inline const juce::StringArray oscWaves   { "Sine", "Saw", "Square", "Triangle", "WT Basic", "WT PWM", "WT Vocal", "WT Digital", "WT User" };
    inline const juce::StringArray oscWavesShort { "SIN", "SAW", "SQR", "TRI", "BASIC", "PWM", "VOCAL", "DIGI", "USER" };
    inline const juce::StringArray warpModes  { "Off", "FM", "AM", "Ring" };
    inline const juce::StringArray subWaves   { "Sine", "Triangle", "Square" };
    inline const juce::StringArray subOcts    { "-1 Oct", "-2 Oct" };
    inline const juce::StringArray noiseTypes { "White", "Pink" };
    inline const juce::StringArray filterTypes { "Off", "Acid 24", "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "Notch", "Comb" };
    inline const juce::StringArray routings   { "Serial", "Parallel", "Split" };
    inline const juce::StringArray lfoShapes  { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Custom" };
    inline const juce::StringArray lfoShapesShort { "SIN", "TRI", "UP", "DOWN", "SQR", "S&H", "CUST" };
    inline const juce::StringArray divisions  { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 Bar", "2 Bars", "4 Bars" };
    inline const float divisionBeats[] = { 0.125f, 1.f / 6.f, 0.25f, 0.375f, 1.f / 3.f, 0.5f, 0.75f, 2.f / 3.f, 1.f, 1.5f, 2.f, 4.f, 8.f, 16.f };
    inline const juce::StringArray lfoTrig    { "Global", "Retrigger" };
    inline const juce::StringArray voiceModes { "Poly", "Mono", "Legato" };
    // new entries only ever go at the end, so saved projects keep their choices
    inline const juce::StringArray distTypes  { "Tube", "Soft Clip", "Hard Clip", "Bitcrush", "Fuzz", "Fold", "Rectify", "Diode", "Scream", "Destroy" };
    inline const juce::StringArray distTypesShort { "TUBE", "SOFT", "HARD", "CRUSH", "FUZZ", "FOLD", "RECT", "DIODE", "SCREAM", "DSTRY" };
    inline const juce::StringArray modFxTypes { "Chorus", "Phaser", "Flanger", "Ring Mod", "Tremolo", "Auto Pan" };
    inline const juce::StringArray fxFilterTypes { "LP 24", "HP 24", "BP", "Notch", "Acid" };
    inline const juce::StringArray oscShapes  { "Off", "Fold", "PWM", "Asym" };
    inline const juce::StringArray uniModes   { "Classic", "Power", "Minor", "Major" };
    inline const juce::StringArray arpModes   { "Up", "Down", "Up/Down", "Down/Up", "Random", "Played", "Chord", "Converge" };
    inline const juce::StringArray arpModesShort { "UP", "DOWN", "UP/DN", "DN/UP", "RAND", "PLAYED", "CHORD", "CONV" };
    inline const juce::StringArray compModes  { "Normal", "OTT" };

    inline const juce::StringArray modSources { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8",
                                                "Env 2 (Filter)", "Env 3", "Env 4", "Mod Wheel", "Velocity", "Aftertouch", "Keytrack", "Random",
                                                "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    inline const juce::StringArray modDests   { "None", "Osc 1 Pitch", "Osc 2 Pitch", "Osc 1 WT Pos", "Osc 2 WT Pos",
                                                "Osc 1 Level", "Osc 2 Level", "Osc 1 Warp", "Osc 2 Warp", "Unison Detune",
                                                "Sub Level", "Noise Level", "Filter 1 Cutoff", "Filter 2 Cutoff",
                                                "Filter 1 Reso", "Filter 2 Reso", "Amp", "Pan",
                                                "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate", "LFO 5 Rate", "LFO 6 Rate", "LFO 7 Rate", "LFO 8 Rate",
                                                "Dist Drive", "Mod FX Mix", "Delay Mix", "Reverb Mix", "FX Filter Cutoff", "Dist Mix", "Osc 1 Shape", "Osc 2 Shape" };
}

enum ModSource { S_None, S_Lfo1, S_Lfo2, S_Lfo3, S_Lfo4, S_Lfo5, S_Lfo6, S_Lfo7, S_Lfo8,
                 S_Env2, S_Env3, S_Env4, S_ModWheel, S_Velocity, S_Aftertouch, S_Keytrack, S_Random,
                 S_Macro1, S_Macro2, S_Macro3, S_Macro4, S_Count };

enum ModDest
{
    D_None, D_Osc1Pitch, D_Osc2Pitch, D_Osc1Wt, D_Osc2Wt, D_Osc1Level, D_Osc2Level, D_Osc1Warp, D_Osc2Warp,
    D_Detune, D_Sub, D_Noise, D_Cut1, D_Cut2, D_Res1, D_Res2, D_Amp, D_Pan,
    D_Lfo1Rate, D_Lfo2Rate, D_Lfo3Rate, D_Lfo4Rate, D_Lfo5Rate, D_Lfo6Rate, D_Lfo7Rate, D_Lfo8Rate,
    D_Dist, D_ModFx, D_Delay, D_Reverb, D_FxCut, D_DistMix, D_Shape1, D_Shape2, D_Count
};

constexpr int kNumModSlots = 16;
constexpr int kNumLfos = 8;
constexpr int kNumEnvs = 4;
constexpr int kLfoSteps = 16;
constexpr int kNumMacros = 4;
constexpr int kNumLocks = 4;   // step-lock lanes: cutoff, resonance, decay, drive

enum DistType { DTube, DSoft, DHard, DCrush, DFuzz, DFold, DRectify, DDiode, DScream, DDestroy };
enum ArpMode { ArpUp, ArpDown, ArpUpDown, ArpDownUp, ArpRandom, ArpPlayed, ArpChord, ArpConverge };
