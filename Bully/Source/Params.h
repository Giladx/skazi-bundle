#pragma once

// BULLY parameter table, modulation sources/destinations and fast parameter indices.
// The table builds both the plugin parameters and the preset defaults.
#include "../../Common/SkaziCore.h"
#include <deque>
#include <string>
#include <unordered_map>

namespace bully
{
constexpr int kOsc = 3, kEnv = 4, kLfo = 10, kChaos = 2, kMacro = 4, kSlots = 16, kTables = 12;

enum Warp { WNone, WSync, WBendP, WBendM, WPwm, WAsym, WMirror, WQuant, WFm, WRing, NumWarps };
enum NoiseType { NWhite, NPink, NBrown, NCrackle, NVinyl, NDigital };
// New types go at the end, so saved projects and presets keep pointing at the same filter
enum FilterType { FLp12, FLp24, FHp12, FHp24, FBp12, FNotch, FPeak, FCombP, FCombM, FFormant, FPhaser, FRing, FDirty, FSharp24, NumFilterTypes };
enum LfoShape { LSine, LTri, LSawUp, LSawDown, LSquare, LPulse, LSH, LSmooth, LStairs, LHump, NumLfoShapes };
enum LfoMode { LFree, LTrig, LEnv };
enum VoiceMode { VPoly, VMono, VLegato };
enum ArpMode { AUp, ADown, AUpDown, ARandom, AAsPlayed, AChord };
enum DistType { DTube, DSoft, DHard, DDiode, DFold, DCrush, DDownsample, DRectify };
enum ModFxType { MChorus, MFlanger, MPhaser };

// Modulation sources
enum Src { SOff, SEnv1, SLfo1 = SEnv1 + kEnv, SChaos1 = SLfo1 + kLfo, SMacro1 = SChaos1 + kChaos,
           SVel = SMacro1 + kMacro, SNote, SModWheel, SBend, SAfter, SRand, NumSrcs };
inline bool srcIsPerVoice (int s) { return (s >= SEnv1 && s < SLfo1) || s == SVel || s == SNote || s == SRand; }

// Modulation destinations. Per-osc block: POS, WARP 1, WARP 2, PITCH, DETUNE, LEVEL, PAN
enum Dest { DOff,
            DOsc0 = 1, kOscDests = 7,
            DNTone = DOsc0 + kOscDests * kOsc, DNLvl,
            DF1Cut, DF1Res, DF1Drv, DF1Var, DF1Mix,
            DF2Cut, DF2Res, DF2Drv, DF2Var, DF2Mix,
            DFirstGlobal,
            DHyMix = DFirstGlobal, DDsDrv, DDsMix, DMoRate, DMoMix, DFsFrq, DFsMix, DOtDep, DDlFb, DDlMix, DRvSize, DRvMix,
            NumDests };
enum OscDest { OPos, OW1, OW2, OPitch, ODet, OLvl, OPan };
constexpr float kPitchModRange = 24.0f; // semitones at 100% (matches the SEMI knob's span)

inline char oscLetter (int o) { return (char) ('a' + o); }

inline const juce::StringArray& tableNames()
{
    static const juce::StringArray n { "BASIC SHAPES", "BULLY SAW", "GROWL FM", "SUB SINE", "SQUARE PWM", "VOCAL",
                                       "METALLIC", "DIGITAL SYNC", "ACID SWEEP", "HOLLOW ORGAN", "SPECTRAL NOISE", "FOLD TEAR" };
    return n;
}

inline const juce::StringArray& srcNames()
{
    static const juce::StringArray n = []
    {
        juce::StringArray s { "OFF" };
        for (int i = 1; i <= kEnv; ++i) s.add ("ENV " + juce::String (i));
        for (int i = 1; i <= kLfo; ++i) s.add ("LFO " + juce::String (i));
        for (int i = 1; i <= kChaos; ++i) s.add ("CHAOS " + juce::String (i));
        for (int i = 1; i <= kMacro; ++i) s.add ("MACRO " + juce::String (i));
        s.addArray ({ "VELOCITY", "NOTE", "MOD WHEEL", "PITCH BEND", "AFTERTOUCH", "RANDOM" });
        return s;
    }();
    return n;
}

// Name and parameter ID of every destination (the parameter is what a knob drop maps to)
inline const std::vector<std::pair<juce::String, std::string>>& destInfo()
{
    static const std::vector<std::pair<juce::String, std::string>> d = []
    {
        std::vector<std::pair<juce::String, std::string>> v { { "OFF", "" } };
        const char* on[] = { "WT POS", "WARP 1", "WARP 2", "PITCH", "DETUNE", "LEVEL", "PAN" };
        const char* op[] = { "pos", "w1", "w2", "semi", "det", "lvl", "pan" };
        for (int o = 0; o < kOsc; ++o)
            for (int k = 0; k < kOscDests; ++k)
                v.push_back ({ juce::String::charToString ((juce::juce_wchar) ('A' + o)) + " " + on[k], std::string ("o") + oscLetter (o) + op[k] });
        v.push_back ({ "NOISE TONE", "ntone" });
        v.push_back ({ "NOISE LEVEL", "nlvl" });
        for (int f = 1; f <= 2; ++f)
        {
            const std::string p = "f" + std::to_string (f);
            const juce::String n = "F" + juce::String (f) + " ";
            v.push_back ({ n + "CUTOFF", p + "cut" }); v.push_back ({ n + "RES", p + "res" }); v.push_back ({ n + "DRIVE", p + "drv" });
            v.push_back ({ n + "VAR", p + "var" });    v.push_back ({ n + "MIX", p + "mix" });
        }
        v.push_back ({ "HYPER MIX", "hymix" });   v.push_back ({ "DIST DRIVE", "dsdrv" }); v.push_back ({ "DIST MIX", "dsmix" });
        v.push_back ({ "MODFX RATE", "morate" }); v.push_back ({ "MODFX MIX", "momix" });  v.push_back ({ "FSHIFT FREQ", "fsfrq" });
        v.push_back ({ "FSHIFT MIX", "fsmix" });  v.push_back ({ "OTT DEPTH", "otdep" });  v.push_back ({ "DELAY FB", "dlfb" });
        v.push_back ({ "DELAY MIX", "dlmix" });   v.push_back ({ "REVERB SIZE", "rvsize" }); v.push_back ({ "REVERB MIX", "rvmix" });
        jassert ((int) v.size() == NumDests);
        return v;
    }();
    return d;
}

inline juce::StringArray destNames()
{
    juce::StringArray s;
    for (auto& d : destInfo()) s.add (d.first);
    return s;
}

inline int destForParam (const juce::String& paramId)
{
    auto& d = destInfo();
    for (int i = 1; i < (int) d.size(); ++i)
        if (paramId == juce::String (d[(size_t) i].second)) return i;
    if (paramId.length() >= 4 && paramId[0] == 'o' && (paramId.substring (2) == "oct" || paramId.substring (2) == "fine"))
        return DOsc0 + (paramId[1] - 'a') * kOscDests + OPitch;
    return -1;
}

inline const std::vector<skazi::ParamDef>& paramDefs()
{
    static std::deque<std::string> ids, names; // stable storage for the generated strings
    static const std::vector<skazi::ParamDef> defs = []
    {
        using P = skazi::ParamDef;
        std::vector<P> d;
        auto id = [] (const std::string& s) { ids.push_back (s); return ids.back().c_str(); };
        auto nm = [] (const std::string& s) { names.push_back (s); return names.back().c_str(); };
        auto F = [&] (const std::string& i, const std::string& n, float lo, float hi, float def, const char* lab, float step = 0.1f, float skew = 1.0f)
        { d.push_back ({ id (i), nm (n), P::Float, lo, hi, def, lab, step, skew }); };
        auto I = [&] (const std::string& i, const std::string& n, int lo, int hi, int def) { d.push_back ({ id (i), nm (n), P::Int, (float) lo, (float) hi, (float) def }); };
        auto B = [&] (const std::string& i, const std::string& n, bool def) { d.push_back ({ id (i), nm (n), P::Bool, 0, 1, def ? 1.0f : 0.0f }); };
        auto C = [&] (const std::string& i, const std::string& n, const juce::StringArray& ch, int def)
        { d.push_back ({ id (i), nm (n), P::Choice, 0, (float) (ch.size() - 1), (float) def, "", 1, 1, ch }); };

        const juce::StringArray warps { "NONE", "SYNC", "BEND +", "BEND -", "PWM", "ASYM", "MIRROR", "QUANTIZE", "FM", "RING" };
        const juce::StringArray filters { "MG LOW 12", "MG LOW 24", "HIGH 12", "HIGH 24", "BAND 12", "NOTCH", "PEAK",
                                          "COMB +", "COMB -", "FORMANT", "PHASER", "RING MOD", "DIRTY LOW", "SHARP 24" };
        const juce::StringArray lfoShapes { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "PULSE", "S&H", "SMOOTH RND", "STAIRS", "HUMP" };
        const auto rates = skazi::noteLengthNames();

        for (int o = 0; o < kOsc; ++o)
        {
            const std::string p = std::string ("o") + oscLetter (o), N = std::string ("Osc ") + (char) ('A' + o) + " ";
            B (p + "on", N + "On", o == 0);
            C (p + "tab", N + "Wavetable", tableNames(), o == 0 ? 1 : (o == 1 ? 2 : 3));
            F (p + "pos", N + "WT Pos", 0, 100, 0, "%");
            C (p + "w1t", N + "Warp 1", warps, 0);
            F (p + "w1", N + "Warp 1 Amount", 0, 100, 0, "%");
            C (p + "w2t", N + "Warp 2", warps, 0);
            F (p + "w2", N + "Warp 2 Amount", 0, 100, 0, "%");
            I (p + "oct", N + "Octave", -4, 4, o == 2 ? -1 : 0);
            I (p + "semi", N + "Semi", -12, 12, 0);
            F (p + "fine", N + "Fine", -100, 100, 0, "ct", 1);
            I (p + "uni", N + "Unison", 1, 8, 1);
            F (p + "det", N + "Detune", 0, 100, 25, "%");
            F (p + "bld", N + "Blend", 0, 100, 75, "%");
            F (p + "phs", N + "Phase", 0, 100, 0, "%");
            F (p + "rnd", N + "Phase Random", 0, 100, 100, "%");
            F (p + "pan", N + "Pan", -100, 100, 0, "");
            F (p + "lvl", N + "Level", 0, 100, 75, "%");
            B (p + "f1", N + "To Filter 1", true);
            B (p + "f2", N + "To Filter 2", false);
        }
        B ("non", "Noise On", false);
        C ("ntype", "Noise Type", { "WHITE", "PINK", "BROWN", "CRACKLE", "VINYL", "DIGITAL" }, 0);
        F ("ntone", "Noise Tone", 0, 100, 60, "%");
        F ("nlvl", "Noise Level", 0, 100, 25, "%");
        F ("npan", "Noise Pan", -100, 100, 0, "");
        B ("nf1", "Noise To Filter 1", true);
        B ("nf2", "Noise To Filter 2", false);

        for (int f = 1; f <= 2; ++f)
        {
            const std::string p = "f" + std::to_string (f), N = "Filter " + std::to_string (f) + " ";
            B (p + "on", N + "On", f == 1);
            C (p + "type", N + "Type", filters, f == 1 ? FLp24 : FHp12);
            F (p + "cut", N + "Cutoff", 20, 20000, f == 1 ? 6000.0f : 80.0f, "Hz", 1, 0.25f);
            F (p + "res", N + "Resonance", 0, 100, 10, "%");
            F (p + "drv", N + "Drive", 0, 100, 0, "%");
            F (p + "var", N + "Var", 0, 100, 0, "%");
            F (p + "mix", N + "Mix", 0, 100, 100, "%");
            F (p + "key", N + "Key Track", 0, 100, 0, "%");
        }
        C ("froute", "Filter Routing", { "SERIAL", "PARALLEL" }, 0);

        for (int e = 1; e <= kEnv; ++e)
        {
            const std::string p = "e" + std::to_string (e), N = "Env " + std::to_string (e) + " ";
            F (p + "a", N + "Attack", 0, 10000, e == 1 ? 1.0f : 5.0f, "ms", 0.1f, 0.25f);
            F (p + "d", N + "Decay", 1, 10000, e == 1 ? 600.0f : 400.0f, "ms", 0.1f, 0.25f);
            F (p + "s", N + "Sustain", 0, 100, e == 1 ? 85.0f : 0.0f, "%");
            F (p + "r", N + "Release", 1, 15000, e == 1 ? 180.0f : 300.0f, "ms", 0.1f, 0.25f);
        }
        for (int l = 1; l <= kLfo; ++l)
        {
            const std::string p = "l" + std::to_string (l), N = "LFO " + std::to_string (l) + " ";
            C (p + "shp", N + "Shape", lfoShapes, (l - 1) % NumLfoShapes);
            B (p + "sync", N + "Sync", true);
            C (p + "rate", N + "Rate (sync)", rates, 2 + (l % 4));
            F (p + "hz", N + "Rate (Hz)", 0.01f, 30, 1, "Hz", 0.01f, 0.3f);
            C (p + "mode", N + "Mode", { "FREE", "TRIG", "ENV" }, 1);
            F (p + "smo", N + "Smooth", 0, 100, 0, "%");
            F (p + "phs", N + "Phase", 0, 100, 0, "%");
        }
        for (int c = 1; c <= kChaos; ++c)
        {
            const std::string p = "c" + std::to_string (c), N = "Chaos " + std::to_string (c) + " ";
            C (p + "type", N + "Type", { "LORENZ", "ROSSLER", "HENON" }, c - 1);
            F (p + "rate", N + "Rate", 0, 100, 30, "%");
            F (p + "smo", N + "Smooth", 0, 100, 20, "%");
        }
        for (int m = 1; m <= kMacro; ++m)
            F ("m" + std::to_string (m), "Macro " + std::to_string (m), 0, 100, 0, "%");
        for (int s = 1; s <= kSlots; ++s)
        {
            const std::string p = "x" + std::to_string (s), N = "Mod " + std::to_string (s) + " ";
            C (p + "src", N + "Source", srcNames(), 0);
            C (p + "dst", N + "Destination", destNames(), 0);
            F (p + "amt", N + "Amount", -100, 100, 0, "%");
        }

        // FX rack (signal order: HYPER, DIST, MODFX, FSHIFT, OTT, EQ, DELAY, REVERB)
        B ("hyon", "Hyper On", false);
        I ("hyuni", "Hyper Voices", 2, 7, 4);
        F ("hydet", "Hyper Detune", 0, 100, 30, "%");
        F ("hyrate", "Hyper Rate", 0, 100, 30, "%");
        F ("hymix", "Hyper Mix", 0, 100, 50, "%");
        B ("dson", "Dist On", false);
        C ("dstype", "Dist Type", { "TUBE", "SOFT", "HARD", "DIODE", "FOLD", "BITCRUSH", "DOWNSAMPLE", "RECTIFY" }, 0);
        F ("dsdrv", "Dist Drive", 0, 100, 30, "%");
        F ("dstone", "Dist Tone", 200, 20000, 14000, "Hz", 1, 0.3f);
        F ("dsmix", "Dist Mix", 0, 100, 100, "%");
        B ("moon", "ModFX On", false);
        C ("motype", "ModFX Type", { "CHORUS", "FLANGER", "PHASER" }, 0);
        F ("morate", "ModFX Rate", 0.02f, 10, 0.5f, "Hz", 0.01f, 0.4f);
        F ("modep", "ModFX Depth", 0, 100, 40, "%");
        F ("mofb", "ModFX Feedback", 0, 95, 20, "%");
        F ("momix", "ModFX Mix", 0, 100, 50, "%");
        B ("fson", "Freq Shift On", false);
        F ("fsfrq", "Freq Shift", -1000, 1000, 0, "Hz", 0.1f);
        F ("fsmix", "Freq Shift Mix", 0, 100, 50, "%");
        B ("oton", "OTT On", false);
        F ("otdep", "OTT Depth", 0, 100, 50, "%");
        F ("ottime", "OTT Time", 0, 100, 50, "%");
        F ("otup", "OTT Upward", 0, 100, 60, "%");
        F ("otdn", "OTT Downward", 0, 100, 60, "%");
        B ("eqon", "EQ On", false);
        F ("eqlo", "EQ Low", -18, 18, 0, "dB");
        F ("eqmidf", "EQ Mid Freq", 100, 10000, 1000, "Hz", 1, 0.3f);
        F ("eqmid", "EQ Mid", -18, 18, 0, "dB");
        F ("eqhi", "EQ High", -18, 18, 0, "dB");
        B ("dlon", "Delay On", false);
        B ("dlsync", "Delay Sync", true);
        C ("dlnote", "Delay Time (sync)", rates, 12);
        F ("dlms", "Delay Time (ms)", 1, 2000, 300, "ms", 0.1f, 0.4f);
        F ("dlfb", "Delay Feedback", 0, 100, 35, "%");
        B ("dlpp", "Delay Ping-Pong", true);
        F ("dltone", "Delay Tone", 0, 100, 70, "%");
        F ("dlmix", "Delay Mix", 0, 100, 25, "%");
        B ("rvon", "Reverb On", false);
        F ("rvsize", "Reverb Size", 0, 100, 60, "%");
        F ("rvdec", "Reverb Decay", 0, 100, 50, "%");
        F ("rvdamp", "Reverb Damp", 0, 100, 40, "%");
        F ("rvpre", "Reverb Pre-delay", 0, 250, 10, "ms");
        F ("rvwid", "Reverb Width", 0, 100, 100, "%");
        F ("rvmix", "Reverb Mix", 0, 100, 25, "%");
        // 3-band splitter: LOW band forced mono, DELAY + REVERB only on the HIGH band
        B ("spon", "Splitter On", false);
        F ("splo", "Split Low/Mid", 50, 1000, 300, "Hz", 1, 0.4f);
        F ("sphi", "Split Mid/High", 500, 10000, 2500, "Hz", 1, 0.4f);

        B ("arpon", "Arp On", false);
        C ("arpmode", "Arp Mode", { "UP", "DOWN", "UP/DOWN", "RANDOM", "AS PLAYED", "CHORD" }, 0);
        C ("arprate", "Arp Rate", rates, 4);
        I ("arpoct", "Arp Octaves", 1, 4, 1);
        F ("arpgate", "Arp Gate", 5, 100, 60, "%");
        F ("arpswing", "Arp Swing", 0, 75, 0, "%");

        I ("poly", "Voices", 1, 16, 8);
        C ("vmode", "Voice Mode", { "POLY", "MONO", "LEGATO" }, 0);
        F ("glide", "Glide", 0, 2000, 0, "ms", 0.1f, 0.35f);
        I ("bend", "Bend Range", 0, 24, 2);
        F ("velo", "Velocity", 0, 100, 60, "%");
        F ("vol", "Volume", -36, 12, -4, "dB");
        // analog character: what sets BULLY apart from the other SKAZI synths
        F ("drift", "Analog Drift", 0, 100, 35, "%");
        F ("sat", "Saturation", 0, 100, 30, "%");
        F ("boost", "Analog Boost", 0, 100, 25, "%");
        F ("spread", "Voice Spread", 0, 100, 40, "%");
        return d;
    }();
    return defs;
}

// The sound BULLY opens with: a Virus-style hypersaw over a Nord-style sync oscillator,
// through the saturated ladder, with chorus, delay and reverb. (Presets build from the plain defaults.)
inline const std::map<juce::String, float>& initPatch()
{
    static const std::map<juce::String, float> p = {
        { "oatab", 1 }, { "oapos", 100 }, { "oauni", 7 }, { "oadet", 32 }, { "oabld", 100 }, { "oalvl", 68 },
        { "obon", 1 }, { "obtab", 0 }, { "obpos", 100 }, { "obw1t", 1 }, { "obw1", 18 }, { "oblvl", 52 }, { "obuni", 2 }, { "obdet", 12 },
        { "f1type", 1 }, { "f1cut", 3800 }, { "f1res", 24 }, { "f1drv", 18 },
        { "e2a", 0 }, { "e2d", 650 }, { "e2s", 0 }, { "e2r", 300 },
        { "x1src", 2 }, { "x1dst", (float) (1 + 7 + 1) }, { "x1amt", 35 },   // ENV 2 -> B WARP 1 (sync sweep)
        { "x2src", 2 }, { "x2dst", (float) (1 + 7 * 3 + 2) }, { "x2amt", 18 }, // ENV 2 -> F1 CUTOFF
        { "x3src", 6 }, { "x3dst", 1 }, { "x3amt", -12 },                      // LFO 2 (slow) -> A WT POS
        { "l2shp", 0 }, { "l2rate", 14 }, { "l2mode", 0 },
        { "moon", 1 }, { "motype", 0 }, { "morate", 0.35f }, { "modep", 45 }, { "momix", 35 },
        { "dlon", 1 }, { "dlnote", 12 }, { "dlfb", 35 }, { "dlmix", 20 },
        { "rvon", 1 }, { "rvsize", 70 }, { "rvdec", 55 }, { "rvmix", 24 },
        { "vol", -9 },
    };
    return p;
}

inline int pid (const std::string& id)
{
    static const std::unordered_map<std::string, int> m = []
    {
        std::unordered_map<std::string, int> r;
        auto& d = paramDefs();
        for (int i = 0; i < (int) d.size(); ++i) r[d[(size_t) i].id] = i;
        return r;
    }();
    auto it = m.find (id);
    jassert (it != m.end());
    return it == m.end() ? 0 : it->second;
}

// Normalised <-> real value for a parameter, matching juce::NormalisableRange with skew
inline float toNorm (int idx, float x)
{
    auto& d = paramDefs()[(size_t) idx];
    const float t = juce::jlimit (0.0f, 1.0f, (x - d.lo) / (d.hi - d.lo));
    return d.skew == 1.0f ? t : std::pow (t, d.skew);
}
inline float fromNorm (int idx, float n)
{
    auto& d = paramDefs()[(size_t) idx];
    n = juce::jlimit (0.0f, 1.0f, n);
    return d.lo + (d.hi - d.lo) * (d.skew == 1.0f ? n : std::pow (n, 1.0f / d.skew));
}

// Parameter indices resolved once
struct Ix
{
    struct Osc { int on, tab, pos, w1t, w1, w2t, w2, oct, semi, fine, uni, det, bld, phs, rnd, pan, lvl, f1, f2; } osc[kOsc];
    int non, ntype, ntone, nlvl, npan, nf1, nf2;
    struct Filt { int on, type, cut, res, drv, var, mix, key; } filt[2];
    int froute;
    struct Env { int a, d, s, r; } env[kEnv];
    struct Lfo { int shp, sync, rate, hz, mode, smo, phs; } lfo[kLfo];
    struct Chaos { int type, rate, smo; } chaos[kChaos];
    int macro[kMacro];
    struct Slot { int src, dst, amt; } slot[kSlots];
    int hyon, hyuni, hydet, hyrate, hymix, dson, dstype, dsdrv, dstone, dsmix, moon, motype, morate, modep, mofb, momix,
        fson, fsfrq, fsmix, oton, otdep, ottime, otup, otdn, eqon, eqlo, eqmidf, eqmid, eqhi,
        dlon, dlsync, dlnote, dlms, dlfb, dlpp, dltone, dlmix, rvon, rvsize, rvdec, rvdamp, rvpre, rvwid, rvmix, spon, splo, sphi;
    int arpon, arpmode, arprate, arpoct, arpgate, arpswing, poly, vmode, glide, bend, velo, vol, drift, sat, boost, spread;
    int destParam[NumDests]; // parameter index behind each destination (-1 for OFF)

    static const Ix& get()
    {
        static const Ix ix = []
        {
            Ix x {};
            for (int o = 0; o < kOsc; ++o)
            {
                const std::string p = std::string ("o") + oscLetter (o);
                auto& s = x.osc[o];
                s = { pid (p + "on"), pid (p + "tab"), pid (p + "pos"), pid (p + "w1t"), pid (p + "w1"), pid (p + "w2t"), pid (p + "w2"),
                      pid (p + "oct"), pid (p + "semi"), pid (p + "fine"), pid (p + "uni"), pid (p + "det"), pid (p + "bld"), pid (p + "phs"),
                      pid (p + "rnd"), pid (p + "pan"), pid (p + "lvl"), pid (p + "f1"), pid (p + "f2") };
            }
            x.non = pid ("non"); x.ntype = pid ("ntype"); x.ntone = pid ("ntone"); x.nlvl = pid ("nlvl"); x.npan = pid ("npan");
            x.nf1 = pid ("nf1"); x.nf2 = pid ("nf2");
            for (int f = 0; f < 2; ++f)
            {
                const std::string p = "f" + std::to_string (f + 1);
                x.filt[f] = { pid (p + "on"), pid (p + "type"), pid (p + "cut"), pid (p + "res"), pid (p + "drv"), pid (p + "var"), pid (p + "mix"), pid (p + "key") };
            }
            x.froute = pid ("froute");
            for (int e = 0; e < kEnv; ++e)
            {
                const std::string p = "e" + std::to_string (e + 1);
                x.env[e] = { pid (p + "a"), pid (p + "d"), pid (p + "s"), pid (p + "r") };
            }
            for (int l = 0; l < kLfo; ++l)
            {
                const std::string p = "l" + std::to_string (l + 1);
                x.lfo[l] = { pid (p + "shp"), pid (p + "sync"), pid (p + "rate"), pid (p + "hz"), pid (p + "mode"), pid (p + "smo"), pid (p + "phs") };
            }
            for (int c = 0; c < kChaos; ++c)
            {
                const std::string p = "c" + std::to_string (c + 1);
                x.chaos[c] = { pid (p + "type"), pid (p + "rate"), pid (p + "smo") };
            }
            for (int m = 0; m < kMacro; ++m) x.macro[m] = pid ("m" + std::to_string (m + 1));
            for (int s = 0; s < kSlots; ++s)
            {
                const std::string p = "x" + std::to_string (s + 1);
                x.slot[s] = { pid (p + "src"), pid (p + "dst"), pid (p + "amt") };
            }
#define BX(n) x.n = pid (#n);
            BX (hyon) BX (hyuni) BX (hydet) BX (hyrate) BX (hymix) BX (dson) BX (dstype) BX (dsdrv) BX (dstone) BX (dsmix)
            BX (moon) BX (motype) BX (morate) BX (modep) BX (mofb) BX (momix) BX (fson) BX (fsfrq) BX (fsmix)
            BX (oton) BX (otdep) BX (ottime) BX (otup) BX (otdn) BX (eqon) BX (eqlo) BX (eqmidf) BX (eqmid) BX (eqhi)
            BX (dlon) BX (dlsync) BX (dlnote) BX (dlms) BX (dlfb) BX (dlpp) BX (dltone) BX (dlmix)
            BX (rvon) BX (rvsize) BX (rvdec) BX (rvdamp) BX (rvpre) BX (rvwid) BX (rvmix) BX (spon) BX (splo) BX (sphi)
            BX (arpon) BX (arpmode) BX (arprate) BX (arpoct) BX (arpgate) BX (arpswing)
            BX (poly) BX (vmode) BX (glide) BX (bend) BX (velo) BX (vol) BX (drift) BX (sat) BX (boost) BX (spread)
#undef BX
            auto& di = destInfo();
            for (int i = 0; i < NumDests; ++i) x.destParam[i] = di[(size_t) i].second.empty() ? -1 : pid (di[(size_t) i].second);
            return x;
        }();
        return ix;
    }
};
} // namespace bully
