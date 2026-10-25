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

    // bars: 0 = 1 bar, 1 = 2, 2 = 4, 3 = 8, 4 = 16, 5 = 24.  sfit: 0 ORIGINAL, 1 STRETCH, 2 VARISPEED.  salign: 0 START, 1 END ON DROP.
    // spliceKind / spliceLength are not parameters: they tell BAMBA which kind of Splice sample to pick
    // (RiserLibrary::Kind, -1 any; RiserLibrary::Length) when the preset loads.
    constexpr float kRise = 0, kDown = 1, kSweep = 2, kImpact = 3, kReverse = 4, lAny = 0, lShort = 1, lLong = 2;

    using Changes = std::initializer_list<std::pair<const char*, float>>;
    struct Designed { const char* name; Changes changes; };

    void designed (const std::function<void (const char*, std::initializer_list<Designed>)>& bank)
    {
        // rrate: 0 FREE, 1 BUILD (1/4 > 1/8 > 1/16 > 1/32), 2 1/2, 3 1/4, 4 1/8, 5 1/16, 6 1/32, 7 1/64, 8 1/128
        bank ("SHORT RISES", {
            { "Quick Lift", { { "type", Uplifter }, { "bars", 0 }, { "curve", 70 }, { "noise", 80 }, { "tone", 30 }, { "pfrom", -12 }, { "pto", 12 }, { "sweep", 85 }, { "reverb", 35 } } },
            { "Snap Riser", { { "type", Uplifter }, { "bars", 1 }, { "curve", 75 }, { "noise", 60 }, { "tone", 60 }, { "pfrom", -7 }, { "pto", 19 }, { "sweep", 90 }, { "reso", 55 }, { "width", 80 } } },
            { "White Flick", { { "type", Uplifter }, { "bars", 0 }, { "curve", 60 }, { "noise", 95 }, { "tone", 5 }, { "color", 10 }, { "sweep", 100 }, { "reso", 30 }, { "reverb", 25 } } },
            { "Pre-Drop Whip", { { "type", Uplifter }, { "bars", 0 }, { "curve", 85 }, { "noise", 70 }, { "tone", 50 }, { "pto", 36 }, { "sweep", 100 }, { "reso", 70 }, { "reverb", 20 }, { "delay", 20 } } },
            { "Fill Riser", { { "type", Uplifter }, { "bars", 1 }, { "curve", 60 }, { "noise", 80 }, { "tone", 20 }, { "roll", 55 }, { "reverb", 40 } } },
            { "Laser Climb", { { "type", Laser }, { "bars", 1 }, { "tone", 90 }, { "noise", 10 }, { "roll", 30 }, { "reso", 60 }, { "delay", 45 } } },
            { "Half Bar Zip", { { "type", Uplifter }, { "bars", 0 }, { "curve", 95 }, { "noise", 50 }, { "tone", 70 }, { "pfrom", 0 }, { "pto", 24 }, { "reso", 45 }, { "reverb", 15 }, { "delay", 0 } } },
        });
        bank ("LONG RISES", {
            { "24 Bar Odyssey", { { "type", Uplifter }, { "bars", 5 }, { "curve", 70 }, { "noise", 65 }, { "tone", 45 }, { "pfrom", -24 }, { "pto", 36 }, { "sweep", 95 }, { "reso", 40 }, { "phaser", 55 }, { "reverb", 65 }, { "width", 95 } } },
            { "24 Bar Slow Tide", { { "type", Uplifter }, { "bars", 5 }, { "curve", 85 }, { "noise", 90 }, { "tone", 15 }, { "color", 55 }, { "sweep", 100 }, { "reso", 30 }, { "reverb", 75 } } },
            { "16 Bar Journey", { { "type", Uplifter }, { "bars", 4 }, { "curve", 65 }, { "noise", 60 }, { "tone", 40 }, { "pfrom", -24 }, { "pto", 24 }, { "sweep", 90 }, { "reso", 35 }, { "phaser", 50 }, { "reverb", 60 }, { "width", 90 } } },
            { "Endless Ascent", { { "type", Uplifter }, { "bars", 4 }, { "curve", 80 }, { "noise", 40 }, { "tone", 70 }, { "pfrom", -24 }, { "pto", 36 }, { "sweep", 70 }, { "reso", 45 }, { "roll", 30 } } },
            { "Slow Burn", { { "type", Uplifter }, { "bars", 3 }, { "curve", 85 }, { "noise", 75 }, { "tone", 25 }, { "pfrom", -12 }, { "pto", 12 }, { "sweep", 95 }, { "reso", 25 }, { "reverb", 70 } } },
            { "Stadium Lift", { { "type", Uplifter }, { "bars", 3 }, { "curve", 55 }, { "noise", 65 }, { "tone", 55 }, { "pto", 31 }, { "roll", 45 }, { "reverb", 75 }, { "delay", 35 } } },
            { "Long Siren Climb", { { "type", Siren }, { "bars", 3 }, { "tone", 85 }, { "noise", 15 }, { "pfrom", -12 }, { "pto", 24 }, { "reso", 40 }, { "delay", 40 } } },
            { "Breakdown Tide", { { "type", Uplifter }, { "bars", 4 }, { "curve", 50 }, { "noise", 85 }, { "tone", 15 }, { "color", 65 }, { "sweep", 80 }, { "phaser", 70 }, { "reverb", 80 } } },
        });
        bank ("QUANTIZED UPLIFTS", {
            { "24 Bar Gate Build", { { "type", Uplifter }, { "bars", 5 }, { "rrate", 1 }, { "roll", 65 }, { "rdecay", 50 }, { "curve", 70 }, { "noise", 65 }, { "tone", 55 }, { "pfrom", -24 }, { "pto", 24 }, { "reverb", 55 }, { "delay", 30 } } },
            { "Gated Uplift 1-16", { { "type", Uplifter }, { "bars", 2 }, { "rrate", 5 }, { "roll", 80 }, { "rdecay", 55 }, { "noise", 75 }, { "tone", 45 }, { "pfrom", -12 }, { "pto", 24 }, { "sweep", 90 } } },
            { "Stutter Riser 1-32", { { "type", Uplifter }, { "bars", 1 }, { "rrate", 6 }, { "roll", 85 }, { "rdecay", 60 }, { "noise", 80 }, { "tone", 40 }, { "pto", 24 }, { "reso", 55 } } },
            { "Build Gate Rise", { { "type", Uplifter }, { "bars", 3 }, { "rrate", 1 }, { "roll", 70 }, { "rdecay", 45 }, { "noise", 65 }, { "tone", 55 }, { "pfrom", -24 }, { "pto", 24 }, { "reverb", 55 } } },
            { "Half Bar Pump Rise", { { "type", Uplifter }, { "bars", 2 }, { "rrate", 2 }, { "roll", 60 }, { "rdecay", 30 }, { "noise", 70 }, { "tone", 40 }, { "sweep", 95 }, { "reverb", 60 } } },
            { "Quarter Pulse Uplift", { { "type", Uplifter }, { "bars", 3 }, { "rrate", 3 }, { "roll", 65 }, { "rdecay", 40 }, { "noise", 60 }, { "tone", 50 }, { "pto", 19 }, { "delay", 35 } } },
            { "Eighth Trance Gate", { { "type", Uplifter }, { "bars", 3 }, { "rrate", 4 }, { "roll", 75 }, { "rdecay", 50 }, { "noise", 55 }, { "tone", 70 }, { "width", 100 }, { "pfrom", -12 }, { "pto", 12 }, { "reverb", 55 } } },
            { "Psy Gate Launch 1-16", { { "type", Uplifter }, { "bars", 4 }, { "rrate", 5 }, { "roll", 70 }, { "rdecay", 55 }, { "curve", 72 }, { "noise", 60 }, { "tone", 60 }, { "pfrom", -24 }, { "pto", 36 }, { "reso", 55 }, { "phaser", 45 }, { "delay", 40 } } },
            { "Noise Chopper 1-64", { { "type", Uplifter }, { "bars", 1 }, { "rrate", 7 }, { "roll", 85 }, { "rdecay", 65 }, { "noise", 100 }, { "tone", 0 }, { "color", 15 }, { "sweep", 100 }, { "reso", 40 } } },
            { "Ring Riser 1-128", { { "type", Uplifter }, { "bars", 2 }, { "rrate", 8 }, { "roll", 60 }, { "rdecay", 40 }, { "noise", 40 }, { "tone", 80 }, { "pto", 24 }, { "reso", 50 } } },
            { "Siren Gate 1-16", { { "type", Siren }, { "bars", 2 }, { "rrate", 5 }, { "roll", 70 }, { "rdecay", 55 }, { "tone", 90 }, { "noise", 10 }, { "pto", 24 }, { "delay", 45 } } },
        });
        bank ("SNARE ROLLS", {
            { "24 Bar Snare Epic", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 5 }, { "rrate", 1 }, { "rdecay", 70 }, { "noise", 85 }, { "tone", 45 }, { "pfrom", -12 }, { "pto", 12 }, { "sweep", 90 }, { "reverb", 50 } } },
            { "Snare Build 1-4 To 1-32", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 3 }, { "rrate", 1 }, { "rdecay", 70 }, { "noise", 85 }, { "tone", 45 }, { "pfrom", 0 }, { "pto", 12 }, { "sweep", 80 }, { "reverb", 40 } } },
            { "Psy Snare 16ths", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 2 }, { "rrate", 5 }, { "rdecay", 75 }, { "noise", 90 }, { "tone", 40 }, { "pfrom", -5 }, { "pto", 12 }, { "reverb", 35 }, { "delay", 20 } } },
            { "Techno 8ths Pressure", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 2 }, { "rrate", 4 }, { "rdecay", 65 }, { "noise", 100 }, { "tone", 25 }, { "reso", 30 }, { "reverb", 45 } } },
            { "32nd Snare Rush", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 1 }, { "rrate", 6 }, { "rdecay", 80 }, { "noise", 90 }, { "tone", 50 }, { "pfrom", 0 }, { "pto", 19 }, { "reverb", 30 } } },
            { "Machine Gun 64", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 0 }, { "rrate", 7 }, { "rdecay", 85 }, { "noise", 95 }, { "tone", 40 }, { "pto", 24 }, { "reverb", 25 } } },
            { "Buzz Roll 128", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 0 }, { "rrate", 8 }, { "rdecay", 60 }, { "noise", 90 }, { "tone", 55 }, { "pfrom", -7 }, { "pto", 24 }, { "reverb", 20 } } },
            { "16 Bar Snare Journey", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 4 }, { "rrate", 1 }, { "rdecay", 65 }, { "noise", 80 }, { "tone", 50 }, { "pfrom", -12 }, { "pto", 12 }, { "sweep", 90 }, { "reverb", 55 }, { "delay", 25 } } },
            { "Free Snare Accel", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 2 }, { "rrate", 0 }, { "rdecay", 70 }, { "noise", 85 }, { "tone", 45 }, { "pto", 12 }, { "reverb", 40 } } },
            { "Quarter Snare Stomp", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 1 }, { "rrate", 3 }, { "rdecay", 80 }, { "noise", 90 }, { "tone", 60 }, { "reverb", 60 } } },
        });
        bank ("PITCHED UPLIFTS", {
            { "6 Octave Climb", { { "type", Uplifter }, { "bars", 3 }, { "curve", 55 }, { "pcurve", 35 }, { "noise", 25 }, { "tone", 90 }, { "pfrom", -12 }, { "pto", 72 }, { "sweep", 80 }, { "reso", 40 }, { "width", 90 }, { "reverb", 55 } } },
            { "Smooth Pitch Glide", { { "type", Uplifter }, { "bars", 3 }, { "curve", 60 }, { "pcurve", 15 }, { "noise", 30 }, { "tone", 85 }, { "pfrom", -24 }, { "pto", 48 }, { "sweep", 85 }, { "reso", 35 }, { "phaser", 40 }, { "reverb", 60 } } },
            { "Late Pitch Launch", { { "type", Uplifter }, { "bars", 4 }, { "curve", 55 }, { "pcurve", 85 }, { "noise", 45 }, { "tone", 80 }, { "pfrom", -12 }, { "pto", 60 }, { "sweep", 95 }, { "reso", 50 }, { "reverb", 50 }, { "delay", 30 } } },
            { "Octave Scream", { { "type", Uplifter }, { "bars", 2 }, { "noise", 20 }, { "tone", 90 }, { "pfrom", -12 }, { "pto", 24 }, { "sweep", 70 }, { "reso", 50 }, { "width", 90 } } },
            { "Two Octave Rocket", { { "type", Uplifter }, { "bars", 3 }, { "noise", 30 }, { "tone", 85 }, { "pfrom", -24 }, { "pto", 24 }, { "curve", 70 } } },
            { "Supersaw Lift", { { "type", Uplifter }, { "bars", 2 }, { "noise", 25 }, { "tone", 95 }, { "width", 100 }, { "pfrom", 0 }, { "pto", 12 }, { "reso", 20 }, { "sweep", 80 }, { "phaser", 30 } } },
            { "Fifth Climb", { { "type", Uplifter }, { "bars", 1 }, { "tone", 85 }, { "noise", 15 }, { "pfrom", -5 }, { "pto", 19 }, { "reso", 35 } } },
            { "Hyper Pitch", { { "type", Uplifter }, { "bars", 2 }, { "tone", 90 }, { "noise", 30 }, { "pfrom", 0 }, { "pto", 48 }, { "curve", 75 }, { "reso", 60 } } },
        });
        bank ("FILTERED BUILDS", {
            { "Filter Opener", { { "type", Uplifter }, { "bars", 3 }, { "noise", 50 }, { "tone", 60 }, { "pfrom", 0 }, { "pto", 0 }, { "sweep", 100 }, { "reso", 65 }, { "curve", 60 } } },
            { "Acid Squelch Build", { { "type", Uplifter }, { "bars", 2 }, { "noise", 20 }, { "tone", 90 }, { "pfrom", -12 }, { "pto", -12 }, { "sweep", 100 }, { "reso", 95 }, { "curve", 55 }, { "delay", 40 } } },
            { "Phaser Tunnel", { { "type", Uplifter }, { "bars", 3 }, { "noise", 70 }, { "tone", 40 }, { "phaser", 100 }, { "sweep", 80 }, { "reso", 50 } } },
            { "Lowpass Creep", { { "type", Uplifter }, { "bars", 4 }, { "noise", 60 }, { "tone", 50 }, { "sweep", 100 }, { "reso", 50 }, { "curve", 75 }, { "pfrom", 0 }, { "pto", 7 } } },
            { "Band Riser", { { "type", Uplifter }, { "bars", 2 }, { "noise", 90 }, { "tone", 20 }, { "color", 30 }, { "sweep", 100 }, { "reso", 85 }, { "phaser", 20 } } },
        });
        bank ("NOISE SWEEPS", {
            { "White Wall Up", { { "type", Uplifter }, { "bars", 2 }, { "noise", 100 }, { "tone", 0 }, { "color", 5 }, { "sweep", 100 }, { "reso", 40 }, { "curve", 60 }, { "width", 100 } } },
            { "Noise Swell And Fade", { { "type", NoiseSweep }, { "bars", 1 }, { "noise", 100 }, { "tone", 0 }, { "color", 10 }, { "sweep", 95 }, { "reso", 60 } } },
            { "Pink Breath", { { "type", NoiseSweep }, { "bars", 2 }, { "color", 50 }, { "noise", 90 }, { "tone", 0 }, { "reso", 30 }, { "phaser", 60 }, { "reverb", 60 } } },
            { "Resonant Hiss", { { "type", Uplifter }, { "bars", 2 }, { "noise", 100 }, { "tone", 0 }, { "color", 15 }, { "sweep", 100 }, { "reso", 90 } } },
            { "Jet Pass", { { "type", NoiseSweep }, { "bars", 0 }, { "color", 0 }, { "noise", 100 }, { "tone", 0 }, { "reso", 75 }, { "phaser", 90 }, { "width", 100 } } },
            { "Brown Rumble Up", { { "type", Uplifter }, { "bars", 2 }, { "noise", 100 }, { "color", 90 }, { "tone", 10 }, { "sweep", 80 }, { "reso", 30 } } },
        });
        bank ("REVERSE SWELLS", {
            { "Reverse Cymbal", { { "type", ReverseCrash }, { "bars", 0 }, { "noise", 100 }, { "color", 10 }, { "curve", 50 }, { "reverb", 40 } } },
            { "Reverse Swell", { { "type", ReverseCrash }, { "bars", 1 }, { "curve", 65 }, { "reverb", 70 }, { "width", 100 } } },
            { "Suck Back", { { "type", ReverseCrash }, { "bars", 0 }, { "curve", 85 }, { "noise", 100 }, { "reverb", 20 }, { "delay", 0 } } },
            { "Long Reverse Wash", { { "type", ReverseCrash }, { "bars", 2 }, { "curve", 55 }, { "reverb", 85 }, { "phaser", 30 } } },
        });
        bank ("DOWNLIFTERS", {
            { "Fall Out", { { "type", Downlifter }, { "bars", 1 }, { "curve", 40 }, { "noise", 80 }, { "tone", 30 }, { "reverb", 60 } } },
            { "Long Exhale", { { "type", Downlifter }, { "bars", 2 }, { "curve", 55 }, { "noise", 70 }, { "tone", 20 }, { "color", 60 }, { "reverb", 75 }, { "delay", 40 } } },
            { "Pitch Dive", { { "type", Downlifter }, { "bars", 1 }, { "noise", 20 }, { "tone", 90 }, { "pfrom", -24 }, { "pto", 24 }, { "sweep", 50 }, { "reso", 50 } } },
            { "Post-Drop Wash", { { "type", Downlifter }, { "bars", 2 }, { "noise", 95 }, { "tone", 0 }, { "color", 30 }, { "sweep", 90 }, { "reverb", 85 }, { "phaser", 40 } } },
            { "Tape Stop Fall", { { "type", Downlifter }, { "bars", 0 }, { "curve", 25 }, { "tone", 80 }, { "noise", 30 }, { "pto", 12 }, { "reverb", 20 }, { "delay", 0 } } },
            { "Sub Slide", { { "type", SubDrop }, { "bars", 1 }, { "pto", 36 }, { "curve", 60 }, { "reverb", 20 }, { "delay", 0 } } },
        });
        bank ("PSYTRANCE", {
            { "Psy 24 Bar Breakdown Rise", { { "type", Uplifter }, { "bars", 5 }, { "curve", 75 }, { "noise", 60 }, { "tone", 60 }, { "pfrom", -24 }, { "pto", 36 }, { "sweep", 95 }, { "reso", 60 }, { "roll", 40 }, { "rrate", 5 }, { "phaser", 50 }, { "reverb", 55 }, { "delay", 45 } } },
            { "Psy 16 Bar Launch", { { "type", Uplifter }, { "bars", 4 }, { "curve", 72 }, { "noise", 60 }, { "tone", 60 }, { "pfrom", -24 }, { "pto", 36 }, { "sweep", 95 }, { "reso", 55 }, { "roll", 35 }, { "width", 90 }, { "phaser", 45 }, { "reverb", 55 }, { "delay", 45 } } },
            { "Goa Siren", { { "type", Siren }, { "bars", 2 }, { "tone", 90 }, { "noise", 10 }, { "pfrom", 0 }, { "pto", 24 }, { "reso", 60 }, { "delay", 55 } } },
            { "Full-On Snare Ramp", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 3 }, { "noise", 85 }, { "tone", 45 }, { "pfrom", -7 }, { "pto", 12 }, { "sweep", 85 }, { "reso", 30 }, { "reverb", 45 } } },
            { "Twilight Laser Storm", { { "type", Laser }, { "bars", 2 }, { "tone", 100 }, { "noise", 5 }, { "reso", 75 }, { "delay", 60 }, { "roll", 40 }, { "phaser", 50 } } },
            { "Psy Tunnel", { { "type", Uplifter }, { "bars", 3 }, { "noise", 80 }, { "tone", 30 }, { "color", 30 }, { "phaser", 85 }, { "sweep", 90 }, { "reso", 70 }, { "roll", 20 } } },
            { "Dark Psy Downshift", { { "type", Downlifter }, { "bars", 1 }, { "tone", 80 }, { "noise", 40 }, { "pfrom", -24 }, { "pto", 12 }, { "reso", 60 }, { "delay", 40 } } },
            { "Acid Psy Lift", { { "type", Uplifter }, { "bars", 2 }, { "tone", 95 }, { "noise", 15 }, { "pfrom", -12 }, { "pto", 24 }, { "reso", 90 }, { "sweep", 100 }, { "delay", 50 } } },
            { "Forest Breath", { { "type", NoiseSweep }, { "bars", 2 }, { "color", 60 }, { "noise", 90 }, { "tone", 10 }, { "phaser", 80 }, { "reverb", 80 }, { "delay", 40 } } },
        });
        bank ("TECHNO", {
            { "Warehouse Noise Lift", { { "type", Uplifter }, { "bars", 3 }, { "noise", 95 }, { "tone", 10 }, { "color", 25 }, { "sweep", 90 }, { "reso", 40 }, { "reverb", 70 }, { "curve", 70 } } },
            { "Industrial Sweep", { { "type", NoiseSweep }, { "bars", 2 }, { "noise", 100 }, { "tone", 0 }, { "color", 0 }, { "reso", 85 }, { "phaser", 60 }, { "width", 100 } } },
            { "Hypnotic Rise", { { "type", Uplifter }, { "bars", 2 }, { "noise", 70 }, { "tone", 40 }, { "pfrom", 0 }, { "pto", 12 }, { "roll", 25 }, { "delay", 55 }, { "reverb", 50 } } },
            { "Rumble Drop", { { "type", SubDrop }, { "bars", 0 }, { "pto", 12 }, { "curve", 40 }, { "reverb", 50 } } },
            { "Peak Time Wash", { { "type", ReverseCrash }, { "bars", 1 }, { "reverb", 90 }, { "width", 100 }, { "curve", 60 } } },
            { "Hard Techno Snare Run", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 2 }, { "noise", 100 }, { "tone", 30 }, { "reso", 20 }, { "reverb", 30 } } },
            { "Detroit Down", { { "type", Downlifter }, { "bars", 2 }, { "noise", 60 }, { "tone", 60 }, { "reverb", 70 }, { "delay", 50 } } },
        });
        bank ("HOUSE", {
            { "Classic Snare Roll", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 3 }, { "noise", 75 }, { "tone", 55 }, { "pfrom", 0 }, { "pto", 12 }, { "reverb", 50 } } },
            { "Disco Filter Lift", { { "type", Uplifter }, { "bars", 2 }, { "noise", 40 }, { "tone", 70 }, { "pfrom", 0 }, { "pto", 12 }, { "sweep", 100 }, { "reso", 60 }, { "phaser", 40 } } },
            { "Tech House Woosh", { { "type", NoiseSweep }, { "bars", 1 }, { "noise", 100 }, { "tone", 0 }, { "color", 20 }, { "reso", 50 }, { "reverb", 50 }, { "delay", 30 } } },
            { "Deep Reverse Swell", { { "type", ReverseCrash }, { "bars", 1 }, { "reverb", 80 }, { "curve", 50 }, { "color", 40 } } },
            { "Piano House Uplift", { { "type", Uplifter }, { "bars", 3 }, { "noise", 60 }, { "tone", 50 }, { "pfrom", -12 }, { "pto", 12 }, { "curve", 60 }, { "reverb", 65 }, { "roll", 30 } } },
            { "Garage Zap", { { "type", Laser }, { "bars", 0 }, { "tone", 90 }, { "noise", 5 }, { "delay", 40 }, { "reso", 50 } } },
        });
        bank ("SPLICE LAYER", {
            { "Splice 24 Bar Riser", { { "type", Uplifter }, { "bars", 5 }, { "curve", 70 }, { "synth", 45 }, { "smix", 85 }, { "sfit", 1 }, { "sshape", 50 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Riser Gated 1-16", { { "type", Uplifter }, { "bars", 2 }, { "rrate", 5 }, { "roll", 75 }, { "rdecay", 55 }, { "synth", 35 }, { "smix", 90 }, { "sfit", 1 }, { "sshape", 30 }, { "spliceKind", kRise }, { "spliceLength", lAny } } },
            { "Splice Snare Build", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 3 }, { "rrate", 1 }, { "rdecay", 70 }, { "synth", 55 }, { "smix", 80 }, { "sfit", 1 }, { "sshape", 40 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Dry Riser Wet Synth", { { "type", Uplifter }, { "bars", 3 }, { "reverb", 75 }, { "delay", 45 }, { "synth", 45 }, { "smix", 90 }, { "smpfx", 0 }, { "sfit", 1 }, { "sshape", 20 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Long Riser Lift", { { "type", Uplifter }, { "bars", 3 }, { "synth", 45 }, { "smix", 85 }, { "sfit", 1 }, { "sshape", 40 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Short Riser On Drop", { { "type", Uplifter }, { "bars", 1 }, { "synth", 40 }, { "smix", 90 }, { "sfit", 0 }, { "salign", 1 }, { "sshape", 0 }, { "spliceKind", kRise }, { "spliceLength", lShort } } },
            { "Splice Tape Riser", { { "type", Uplifter }, { "bars", 2 }, { "synth", 30 }, { "smix", 90 }, { "sfit", 2 }, { "sshape", 20 }, { "spliceKind", kRise }, { "spliceLength", lAny } } },
            { "Splice Psy 16 Bar", { { "type", Uplifter }, { "bars", 4 }, { "curve", 70 }, { "pfrom", -24 }, { "pto", 36 }, { "roll", 30 }, { "synth", 50 }, { "smix", 80 }, { "sfit", 1 }, { "sshape", 50 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Filtered Build", { { "type", Uplifter }, { "bars", 3 }, { "sweep", 100 }, { "reso", 60 }, { "synth", 35 }, { "smix", 90 }, { "sfit", 1 }, { "sshape", 90 }, { "spliceKind", kRise }, { "spliceLength", lAny } } },
            { "Splice Riser Up 12", { { "type", Uplifter }, { "bars", 1 }, { "synth", 30 }, { "smix", 90 }, { "sfit", 0 }, { "salign", 1 }, { "stune", 12 }, { "sshape", 0 }, { "spliceKind", kRise }, { "spliceLength", lShort } } },
            { "Splice Mostly Sample", { { "type", Uplifter }, { "bars", 2 }, { "noise", 50 }, { "tone", 20 }, { "synth", 15 }, { "smix", 100 }, { "sfit", 1 }, { "sshape", 0 }, { "reverb", 30 }, { "spliceKind", kRise }, { "spliceLength", lAny } } },
            { "Splice Reverse Swell", { { "type", ReverseCrash }, { "bars", 1 }, { "synth", 35 }, { "smix", 90 }, { "sfit", 1 }, { "sshape", 20 }, { "spliceKind", kReverse }, { "spliceLength", lAny } } },
            { "Splice Riser Flipped Down", { { "type", Downlifter }, { "bars", 2 }, { "synth", 40 }, { "smix", 85 }, { "sfit", 1 }, { "srev", 1 }, { "sshape", 30 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Downlifter", { { "type", Downlifter }, { "bars", 2 }, { "synth", 40 }, { "smix", 90 }, { "sfit", 0 }, { "salign", 0 }, { "sshape", 10 }, { "spliceKind", kDown }, { "spliceLength", lAny } } },
            { "Splice Impact And Sub", { { "type", SubDrop }, { "bars", 1 }, { "pto", 24 }, { "synth", 70 }, { "smix", 90 }, { "sfit", 0 }, { "salign", 0 }, { "sshape", 0 }, { "spliceKind", kImpact }, { "spliceLength", lAny } } },
            { "Splice Impact Boom", { { "type", Impact }, { "bars", 1 }, { "reverb", 70 }, { "synth", 50 }, { "smix", 95 }, { "sfit", 0 }, { "salign", 0 }, { "sshape", 0 }, { "spliceKind", kImpact }, { "spliceLength", lShort } } },
            { "Splice Sweep Layer", { { "type", NoiseSweep }, { "bars", 1 }, { "synth", 40 }, { "smix", 85 }, { "sfit", 1 }, { "sshape", 60 }, { "spliceKind", kSweep }, { "spliceLength", lAny } } },
            { "Splice Techno Wash", { { "type", NoiseSweep }, { "bars", 3 }, { "reverb", 75 }, { "synth", 40 }, { "smix", 85 }, { "sfit", 1 }, { "sshape", 60 }, { "spliceKind", kSweep }, { "spliceLength", lLong } } },
            { "Splice House Build", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 3 }, { "synth", 60 }, { "smix", 75 }, { "sfit", 1 }, { "sshape", 40 }, { "spliceKind", kRise }, { "spliceLength", lLong } } },
            { "Splice Reverse Into Drop", { { "type", ReverseCrash }, { "bars", 0 }, { "synth", 30 }, { "smix", 90 }, { "sfit", 0 }, { "salign", 1 }, { "srev", 1 }, { "sshape", 0 }, { "spliceKind", kImpact }, { "spliceLength", lAny } } },
        });
        bank ("IMPACTS", {
            { "Drop Boom", { { "type", Impact }, { "bars", 1 }, { "tone", 100 }, { "noise", 60 }, { "reverb", 80 }, { "curve", 60 } } },
            { "Short Thud", { { "type", Impact }, { "bars", 0 }, { "curve", 15 }, { "reverb", 30 }, { "tone", 90 }, { "noise", 40 } } },
            { "Cinematic Hit", { { "type", Impact }, { "bars", 1 }, { "curve", 90 }, { "reverb", 95 }, { "delay", 30 }, { "noise", 80 } } },
            { "Sub Bomb Long", { { "type", SubDrop }, { "bars", 1 }, { "pto", 24 }, { "curve", 90 }, { "reverb", 30 } } },
            { "Noise Crack", { { "type", Impact }, { "bars", 0 }, { "noise", 100 }, { "color", 0 }, { "tone", 60 }, { "curve", 30 }, { "reverb", 60 } } },
        });
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
        factory ("Snare Build", { { "type", SnareRoll }, { "roll", 100 }, { "bars", 2 }, { "tone", 50 } });
        factory ("Wash Down", { { "type", Downlifter }, { "bars", 2 }, { "reverb", 70 } });
        factory ("Laser Rain", { { "type", Laser }, { "bars", 1 }, { "delay", 50 } });
        factory ("Reverse Wash", { { "type", ReverseCrash }, { "bars", 1 } });
        factory ("Sub Bomb", { { "type", SubDrop }, { "bars", 1 }, { "pto", 24 } });

        // Hand-made banks: by job, by genre, and ones that layer the user's own Splice samples
        designed ([&] (const char* folder, std::initializer_list<Designed> items)
        {
            for (auto& d : items)
            {
                skazi::Preset p { d.name, folder, defs };
                for (auto& [id, v] : d.changes) p.values[id] = v;
                list.push_back (std::move (p));
            }
        });

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

        // Uplifts and rises first. Impacts, downlifters, lasers, sirens and random FX move to their own
        // FX folders at the end of the list (names unchanged, so nothing that refers to them breaks).
        for (auto& p : list)
        {
            if (p.name == "Init") continue;
            switch ((int) p.values.at ("type"))
            {
                case Downlifter:      p.category = "FX: DOWNLIFTERS"; break;
                case Impact: case SubDrop: p.category = "FX: IMPACTS"; break;
                case Laser: case Siren:    p.category = "FX: LASER + SIREN"; break;
                case RandomFx:        p.category = "FX: RANDOM"; break;
                default:
                    if (p.category == "UPLIFT")             p.category = "MORE UPLIFTS";
                    else if (p.category == "NOISE SWEEP")   p.category = "MORE NOISE SWEEPS";
                    else if (p.category == "SNARE ROLL")    p.category = "MORE SNARE ROLLS";
                    else if (p.category == "REVERSE CRASH") p.category = "MORE REVERSE SWELLS";
                    break;
            }
        }
        std::stable_partition (list.begin(), list.end(), [] (const skazi::Preset& p) { return ! p.category.startsWith ("FX:"); });
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
    // the Splice layer too, for whoever has a sample loaded
    gen.range ("synth", 40, 100);
    gen.set ("sfit", (float) gen.pick ({ 0, 1, 1, 2 }));
    gen.set ("salign", gen.chance (0.7f) ? 1.0f : 0.0f);
    gen.range ("sshape", 0, 80);
    gen.set ("srev", gen.chance (0.15f) ? 1.0f : 0.0f);
    gen.set ("rrate", (float) gen.pick ({ 0, 0, 1, 3, 4, 5, 5, 6 }));
    gen.range ("rdecay", 30, 80);
    gen.set ("rshift", 0);
    return p;
}
} // namespace bamba
