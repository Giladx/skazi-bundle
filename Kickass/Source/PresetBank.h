#pragma once

// Factory kicks, hand-tuned and grouped by style. Every factory kick uses the
// built-in noise click (so the bank sounds the same on every machine) and has
// DRIVE fully off: the tone comes from the pitch shape and the EQ, not distortion.
//
// How the styles differ:
//   Psytrance   - tight and dry: hard snap, punch around 200-280 Hz, a short sweep,
//                 a long hold and a short release so the tail stops before the bassline.
//   Progressive - a little longer and rounder than full-on psy, still a clean tail.
//   Goa/Forest  - the snappiest, clickiest tops; forest kicks are shortest.
//   House       - rounder: lower punch, a longer sweep and a longer, softer release.
//   Tech House  - tight like psy but with house roundness; often a mid scoop.
//   Techno      - longer tails for rumble.
//   808         - low punch and a long release; meant for KEY TRACK.
// The EQ mostly scoops the boxy 300-500 Hz area and adds a little top or low end.

#include "KickEngine.h"
#include <vector>

namespace kick
{
struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

namespace detail
{
/** Builder: the sub's shape up front, everything else by name. Drive stays 0. */
struct Kick
{
    Params p;

    Kick (float start, float punch, float end, float snap, float sweep, float hold, float decay)
    {
        p.startHz = start; p.punchHz = punch; p.endHz = end; p.snapMs = snap;
        p.pitchMs = sweep; p.holdMs = hold; p.ampMs = decay;
        p.drive = 0.0f;
    }

    /** Noise click (or sample) layer: level, tune, high-pass, low-pass. */
    Kick& click (float level, float tune = 0.0f, float hpf = 20.0f, float lpf = 20000.0f)
    {
        p.click = level; p.clickTune = tune; p.clickHpfHz = hpf; p.clickLpfHz = lpf;
        return *this;
    }

    Kick& eq (float low, float mid, float midHz, float high)
    {
        p.eqLowDb = low; p.eqMidDb = mid; p.eqMidHz = midHz; p.eqHighDb = high;
        return *this;
    }

    Kick& level (float db) { p.levelDb = db; return *this; }

    operator Params() const { return p; }
};
} // namespace detail

inline const std::vector<Preset>& factoryPresets()
{
    using K = detail::Kick;
    //                                 start punch end  snap sweep hold decay
    static const std::vector<Preset> bank {
        { "Init",        "Init Kick",        K (3000, 180, 50, 3.0f, 35, 40, 300).click (0.25f, 0, 20, 12000) },

        // ---- Psytrance: tight, dry, ends before the bassline -----------------------------------------
        { "Psytrance",   "Full On Punch",    K (4000, 220, 50, 2.5f, 30, 55, 150).click (0.30f, 0, 20, 12000).eq ( 1.5f, -3.0f, 400,  1.5f) },
        { "Psytrance",   "Skazi Slam",       K (5000, 240, 50, 2.0f, 32, 60, 160).click (0.40f, 0, 900, 10000).eq ( 2.0f, -4.0f, 450,  2.0f).level (-4) },
        { "Psytrance",   "Night Roller",     K (3500, 200, 46, 3.0f, 28, 45, 130).click (0.25f, 0, 20, 10000).eq ( 1.0f, -2.5f, 380,  1.0f) },
        { "Psytrance",   "Twilight Tight",   K (5000, 230, 53, 2.0f, 25, 40, 120).click (0.35f, 3, 1200, 14000).eq ( 0.0f, -3.0f, 420,  2.5f) },
        { "Psytrance",   "Rolling Thunder",  K (3500, 210, 47, 2.5f, 30, 65, 170).click (0.25f, 0, 20, 9000).eq ( 2.5f, -3.0f, 350,  0.0f).level (-4) },
        { "Psytrance",   "Batyam Bounce",    K (4500, 250, 52, 2.0f, 27, 50, 140).click (0.35f, 2, 600, 12000).eq ( 1.5f, -3.5f, 420,  2.0f) },
        { "Psytrance",   "Acid Floor",       K (4000, 230, 49, 2.5f, 29, 55, 150).click (0.30f, 0, 400, 11000).eq ( 1.0f, -4.0f, 500,  1.5f) },
        { "Psytrance",   "Peak Time",        K (5500, 260, 51, 1.8f, 26, 60, 155).click (0.40f, 4, 1000, 14000).eq ( 2.0f, -3.5f, 400,  3.0f).level (-4) },
        { "Psytrance",   "Dark Matter",      K (3000, 190, 45, 3.0f, 33, 60, 160).click (0.20f, -3, 20, 7000).eq ( 2.5f, -2.0f, 320, -1.5f).level (-4) },
        { "Psytrance",   "Sunrise Drop",     K (4500, 240, 53, 2.2f, 28, 50, 145).click (0.30f, 2, 800, 13000).eq ( 1.0f, -3.0f, 450,  2.0f) },
        { "Psytrance",   "Tribal Stomp",     K (3500, 220, 48, 3.0f, 30, 70, 175).click (0.25f, -2, 300, 9000).eq ( 2.0f, -2.0f, 300,  0.5f).level (-4) },
        { "Psytrance",   "Snappy Psy",       K (6000, 270, 54, 1.5f, 24, 40, 125).click (0.45f, 5, 1500, 16000).eq ( 0.5f, -4.0f, 450,  3.0f) },

        // ---- Progressive psy: longer and rounder, still a clean tail ---------------------------------
        { "Progressive", "Progressive",      K (3000, 190, 52, 3.0f, 36, 60, 200).click (0.25f, 0, 20, 11000).eq ( 1.5f, -2.5f, 400,  1.0f) },
        { "Progressive", "Deep Prog",        K (2500, 170, 48, 3.5f, 40, 65, 220).click (0.20f, -2, 20, 8000).eq ( 2.0f, -2.0f, 350,  0.0f).level (-4) },
        { "Progressive", "Melodic Prog",     K (3000, 185, 51, 3.0f, 38, 55, 210).click (0.25f, 0, 200, 12000).eq ( 1.0f, -3.0f, 420,  1.5f) },
        { "Progressive", "Prog Roller",      K (3500, 200, 50, 2.5f, 33, 60, 190).click (0.30f, 1, 400, 12000).eq ( 1.5f, -3.0f, 400,  1.5f) },
        { "Progressive", "Hypnotic",         K (2800, 180, 47, 3.0f, 36, 70, 230).click (0.20f, -1, 20, 9000).eq ( 2.0f, -2.5f, 360,  0.5f).level (-4) },
        { "Progressive", "Spacious",         K (3200, 190, 53, 3.0f, 40, 45, 240).click (0.25f, 0, 300, 13000).eq ( 1.0f, -2.0f, 450,  2.0f) },

        // ---- Goa / Forest: snappy and clicky; forest is shortest -------------------------------------
        { "Goa Forest",  "Goa Thump",        K (2500, 170, 48, 4.0f, 40, 70, 180).click (0.25f, 0, 20, 10000).eq ( 1.5f, -2.0f, 350,  1.0f) },
        { "Goa Forest",  "Goa Classic",      K (3500, 200, 50, 3.0f, 35, 60, 170).click (0.30f, 0, 300, 12000).eq ( 1.0f, -2.5f, 400,  1.5f) },
        { "Goa Forest",  "Forest Knock",     K (6000, 260, 55, 1.5f, 22, 35, 110).click (0.40f, 5, 1800, 16000).eq ( 0.0f, -3.5f, 500,  2.5f) },
        { "Goa Forest",  "Dark Forest",      K (5000, 250, 52, 1.8f, 23, 35, 105).click (0.35f, 3, 1500, 12000).eq ( 0.5f, -4.0f, 480,  1.0f) },
        { "Goa Forest",  "Hi-Tech Tick",     K (7000, 290, 56, 1.2f, 20, 30, 95).click (0.35f, 7, 2500, 18000).eq (-0.5f, -4.0f, 550,  3.0f).level (-5) },

        // ---- House: rounder bodies, longer softer tails -----------------------------------------------
        { "House",       "Classic Nine",     K (2500, 170, 55, 3.0f, 45, 30, 320).click (0.30f, 0, 20, 12000).eq ( 1.0f, -2.0f, 400,  1.5f) },
        { "House",       "Deep Round",       K (1200, 130, 50, 5.0f, 55, 40, 380).click (0.12f, -7, 20, 3500).eq ( 2.0f, -1.5f, 300, -1.0f).level (-4) },
        { "House",       "Disco Punch",      K (3000, 180, 58, 2.5f, 35, 25, 260).click (0.30f, 0, 20, 12000).eq ( 1.0f, -2.5f, 420,  2.0f) },
        { "House",       "Afro Soft",        K (1500, 140, 56, 4.0f, 40, 20, 280).click (0.15f, -5, 20, 5000).eq ( 1.5f, -1.0f, 350,  0.0f) },
        { "House",       "Garage Thump",     K (2200, 160, 54, 3.5f, 42, 35, 300).click (0.25f, 0, 20, 9000).eq ( 2.0f, -2.0f, 380,  1.0f).level (-4) },
        { "House",       "Soulful",          K (1800, 150, 52, 4.0f, 48, 30, 340).click (0.18f, -3, 20, 7000).eq ( 1.5f, -1.5f, 350,  0.5f) },
        { "House",       "Melodic House",    K (2500, 165, 53, 3.0f, 42, 40, 320).click (0.25f, 0, 200, 11000).eq ( 1.5f, -2.5f, 400,  1.5f) },
        { "House",       "Big Room Punch",   K (4000, 210, 55, 2.0f, 35, 45, 300).click (0.35f, 2, 600, 14000).eq ( 2.0f, -3.0f, 450,  2.5f).level (-4) },

        // ---- Tech House: tight like psy, round like house ---------------------------------------------
        { "Tech House",  "Tech House Tight", K (3500, 200, 53, 2.0f, 30, 30, 220).click (0.30f, 0, 300, 12000).eq ( 1.0f, -3.0f, 420,  1.5f) },
        { "Tech House",  "Minimal Tick",     K (4500, 220, 55, 1.8f, 26, 20, 180).click (0.35f, 4, 1200, 15000).eq ( 0.0f, -3.5f, 480,  2.5f) },
        { "Tech House",  "Groovy Thud",      K (2500, 170, 51, 3.0f, 38, 40, 260).click (0.22f, -2, 20, 9000).eq ( 2.0f, -2.5f, 380,  0.5f).level (-4) },
        { "Tech House",  "Latin Tech",       K (3000, 190, 54, 2.5f, 33, 30, 240).click (0.28f, 1, 300, 12000).eq ( 1.0f, -2.5f, 420,  1.5f) },
        { "Tech House",  "Bass House",       K (3500, 200, 50, 2.5f, 32, 45, 230).click (0.30f, 0, 400, 13000).eq ( 2.5f, -3.5f, 400,  2.0f).level (-4) },
        { "Tech House",  "Rolling Tech",     K (3000, 190, 52, 2.5f, 34, 35, 250).click (0.25f, 0, 200, 11000).eq ( 1.5f, -3.0f, 400,  1.0f) },

        // ---- Techno: longer tails for rumble ----------------------------------------------------------
        { "Techno",      "Warehouse",        K (2500, 160, 48, 3.0f, 45, 50, 450).click (0.22f, 0, 20, 10000).eq ( 2.0f, -2.5f, 380,  1.0f).level (-4) },
        { "Techno",      "Rumble Seed",      K (2000, 150, 45, 4.0f, 50, 60, 800).click (0.15f, -2, 20, 8000).eq ( 3.0f, -2.0f, 320,  0.0f).level (-5) },
        { "Techno",      "Industrial",       K (5000, 220, 50, 2.0f, 35, 40, 350).click (0.40f, 3, 1000, 15000).eq ( 1.0f, -4.0f, 500,  3.0f).level (-4) },
        { "Techno",      "Hypno Techno",     K (3000, 180, 47, 3.0f, 40, 55, 420).click (0.25f, 0, 300, 11000).eq ( 2.0f, -3.0f, 400,  1.0f).level (-4) },
        { "Techno",      "Peak Techno",      K (4000, 200, 50, 2.5f, 35, 50, 380).click (0.32f, 2, 600, 13000).eq ( 1.5f, -3.5f, 450,  2.0f).level (-4) },
        { "Techno",      "Dub Techno",       K (1500, 140, 46, 4.5f, 50, 40, 500).click (0.12f, -5, 20, 4000).eq ( 2.5f, -1.5f, 300, -2.0f).level (-4) },

        // ---- 808: long, low; use KEY TRACK to play them -----------------------------------------------
        { "808",         "Long 808",         K ( 800,  90, 40, 6.0f, 60, 50, 1600).click (0.05f, 0, 20, 6000) },
        { "808",         "Boom Sub",         K (1000, 110, 38, 5.0f, 50, 40, 1100).click (0.10f, 0, 20, 8000).eq ( 1.5f, 0.0f, 500, 0.0f).level (-4) },
        { "808",         "Short 808",        K (1200, 120, 45, 4.0f, 40, 40, 450).click (0.10f, 0, 20, 8000).eq ( 1.0f, -1.0f, 400, 0.0f) },
        { "808",         "Trap Sub",         K ( 900, 100, 41, 5.0f, 55, 60, 1300).click (0.05f, 0, 20, 6000).eq ( 2.0f, 0.0f, 500, -1.0f).level (-4) },

        // ---- FX ---------------------------------------------------------------------------------------
        { "FX",          "Laser Drop",       K (5000, 1000, 30, 30.0f, 250, 0, 900).click (0.0f).level (-6) }, // the old zap, on purpose
        { "FX",          "Tom Click",        K (3000, 300, 160, 2.0f, 20, 20, 160).click (0.50f, 12, 2500).eq (0.0f, -2.0f, 600, 2.0f).level (-4) },
    };
    return bank;
}

inline int numFactoryPresets() { return (int) factoryPresets().size(); }
} // namespace kick
