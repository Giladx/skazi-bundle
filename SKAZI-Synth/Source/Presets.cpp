#include "Presets.h"
#include "PluginProcessor.h"
#include "PresetGains.h"
#include <random>
#include <map>
#include <algorithm>
#include <initializer_list>

// Factory presets are generated from per-category recipes with seeded variation,
// so all 3000 are deterministic and need no data files.
namespace
{
    // Choice indices (see Params.h lists)
    enum Wave { Sine, Saw, Square, Tri, WtBasic, WtPwm, WtVocal, WtDigital };
    enum Flt { FOff, Acid24, LP12, LP24, HP12, HP24, BP12, BP24, Notch, Comb };
    enum Div { D32, D16T, D16, D16D, D8T, D8, D8D, D4T, D4, D4D, D2, Bar1, Bar2, Bar4 };

    struct Category { const char* name; int size; int gen; const char* adj[20]; const char* noun[10]; };

    const Category cats[] = {
        { "145 BPM Subs", 350, 0,
          { "Rolling", "Night", "Forest", "Goa", "Tribal", "Dark", "Deep", "Shaman", "Lunar", "Hypnotic",
            "Twisted", "Ritual", "Cosmic", "Electric", "Liquid", "Tight", "Punchy", "Solid", "Mystic", "Primal" },
          { "Sub", "Roller", "Bassline", "Engine", "Pulse", "Drive", "Kick Bass", "Groove", "Motion", "Core" } },
        { "Techno Bass", 250, 12,
          { "Warehouse", "Concrete", "Berlin", "Industrial", "Steel", "Detroit", "Bunker", "Iron", "Black", "Raw",
            "Hypnotic", "Machine", "Acid", "Dark", "Heavy", "Grinding", "Peak", "Tunnel", "Static", "Voltage" },
          { "Rumble", "Stab", "Pulse", "Drive", "Loop", "Thud", "Engine", "Grind", "Hammer", "Floor" } },
        { "House Bass", 200, 13,
          { "Chicago", "Garage", "Soulful", "Jackin", "Disco", "Funky", "Velvet", "Sunset", "Groovy", "Rubber",
            "Bouncy", "Classic", "Warehouse", "Midnight", "Golden", "Smooth", "Club", "Late", "Ibiza", "Retro" },
          { "Bass", "Organ", "Pluck", "Groove", "Bounce", "Line", "Thump", "Jack", "Swing", "Pump" } },
        { "Deep Bass", 200, 14,
          { "Deep", "Dub", "Smoky", "Warm", "Analog", "Velvet", "Murky", "Foggy", "Low", "Mellow",
            "Night", "Ocean", "Sunken", "Dusty", "Soft", "Hazy", "Heavy", "Round", "Dark", "Silky" },
          { "Sub", "Reese", "Bass", "Pluck", "Weight", "Pressure", "Rumble", "Depth", "Current", "Tide" } },
        { "Acid Leads", 250, 1,
          { "Screaming", "Molten", "Acid", "Burning", "Rubber", "Toxic", "Laser", "Sharp", "Wild", "Twisted",
            "Liquid", "Neon", "Squelchy", "Biting", "Hot", "Rave", "Psychedelic", "Electric", "Sour", "Venom" },
          { "Lead", "Squelch", "Line", "Scream", "Hook", "Worm", "Snake", "Riff", "Siren", "Spike" } },
        { "Hard Leads", 300, 15,
          { "Massive", "Brutal", "Supersonic", "Atomic", "Mega", "Savage", "Titan", "Hyper", "Turbo", "Monster",
            "Inferno", "Thunder", "Razor", "Blazing", "Epic", "Nuclear", "Rage", "Overdrive", "Arena", "Stadium" },
          { "Lead", "Saw", "Hoover", "Scream", "Anthem", "Blade", "Rush", "Blast", "Fury", "Assault" } },
        { "LFO Madness", 300, 11,
          { "Wobbly", "Chaos", "Glitch", "Stutter", "Random", "Twisted", "Spinning", "Warped", "Broken", "Insane",
            "Mutant", "Rapid", "Jittery", "Wild", "Crazy", "Fractal", "Quantum", "Laser", "Pulse", "Liquid" },
          { "Wobble", "Gate", "Machine", "Scanner", "Zapper", "Growl", "Spiral", "Engine", "Storm", "Loop" } },
        { "FM Plucks", 200, 2,
          { "Glass", "Crystal", "Metal", "Bright", "Tiny", "Digital", "Chime", "Steel", "Frozen", "Pixel",
            "Silver", "Ice", "Bamboo", "Bell", "Tight", "Short", "Shiny", "Clean", "Hollow", "Plastic" },
          { "Pluck", "Bell", "Mallet", "Ping", "Chime", "Hit", "Drop", "Tine", "Click", "Spark" } },
        { "FX", 200, 3,
          { "Rising", "Falling", "Space", "Alien", "Laser", "Storm", "Noise", "Cosmic", "Broken", "Deep",
            "Black", "White", "Orbital", "Static", "Echo", "Radar", "Siren", "Vortex", "Sonic", "Ghost" },
          { "Riser", "Sweep", "Zap", "Drone", "Impact", "Wash", "Bleeps", "Signal", "Wind", "Hole" } },
        { "Acid Lines", 120, 4,
          { "Rubber", "Squelch", "Liquid", "Toxic", "Neon", "Phuture", "Warehouse", "Silver", "Rave", "Hypno",
            "Laser", "Bubble", "Chrome", "Wobble", "Fizzy", "Electric", "Midnight", "Sour", "Lime", "Mutant" },
          { "Line", "Worm", "Box", "Burner", "Pulse", "Snake", "Juice", "Rain", "Trip", "Groove" } },
        { "Bass", 200, 5,
          { "Deep", "Dark", "Heavy", "Sub", "Fat", "Growl", "Rolling", "Low", "Tech", "Dirty",
            "Warm", "Round", "Thick", "Punchy", "Analog", "Reese", "Gritty", "Murky", "Solid", "Velvet" },
          { "Bass", "Floor", "Rumble", "Engine", "Weight", "Pressure", "Drive", "Thump", "Motion", "Core" } },
        { "Lead", 120, 6,
          { "Screaming", "Bright", "Hyper", "Golden", "Sharp", "Soaring", "Super", "Glass", "Digital", "Cosmic",
            "Hard", "Euphoric", "Vintage", "Neon", "Burning", "Crystal", "Sync", "Wild", "Laser", "Royal" },
          { "Lead", "Saw", "Hook", "Anthem", "Signal", "Voice", "Beam", "Edge", "Flight", "Ray" } },
        { "Pad", 200, 7,
          { "Endless", "Warm", "Frozen", "Ethereal", "Velvet", "Distant", "Lunar", "Ocean", "Dream", "Silent",
            "Aurora", "Misty", "Cathedral", "Glowing", "Soft", "Floating", "Infinite", "Hollow", "Amber", "Solar" },
          { "Pad", "Air", "Cloud", "Horizon", "Field", "Choir", "Haze", "Space", "Waves", "Light" } },
        { "Pluck", 60, 8,
          { "Tiny", "Glass", "Bouncy", "Crisp", "Dew", "Wooden", "Shiny", "Pixel", "Rain", "Pop",
            "Snappy", "Marimba", "Stab", "Bell", "Short", "Digital", "Soft", "Kalimba", "Plastic", "Island" },
          { "Pluck", "Drop", "Pick", "Tap", "Ping", "Chip", "Spark", "Hit", "Click", "Pop" } },
        { "Keys", 25, 9,
          { "Electric", "Vintage", "Smooth", "Soul", "Late", "Dusty", "Silk", "Tine", "Velvet", "Lounge",
            "Warm", "Jazz", "Studio", "Classic", "Glass", "Cool", "Mellow", "Night", "Church", "Retro" },
          { "Keys", "Piano", "Organ", "Chords", "Bells", "Tines", "Clav", "Stab", "Comp", "Board" } },
        { "Sequence", 25, 10,
          { "Running", "Machine", "Arp", "Rolling", "Circuit", "Pulse", "Endless", "Robot", "Kinetic", "Motor",
            "Spiral", "Cascade", "Loop", "Chase", "Turbo", "Clock", "Grid", "Quantum", "Pattern", "Cyber" },
          { "Sequence", "Arp", "Runner", "Steps", "Motion", "Train", "Cycle", "Chain", "Pattern", "Loop" } },
    };
    constexpr int numCats = (int) (sizeof (cats) / sizeof (cats[0]));

    void locate (int index, int& cat, int& local)
    {
        cat = 0; local = index;
        while (cat < numCats - 1 && local >= cats[cat].size) { local -= cats[cat].size; ++cat; }
    }

    //==========================================================================
    struct Ctx
    {
        AcidanceProcessor* p;          // null during a metadata-only dry run
        std::mt19937 rng;
        juce::StringArray* tags = nullptr;
        int slot = 0;

        void tag (const char* t) { if (tags != nullptr) tags->addIfNotAlreadyThere (t); }

        float r (float a, float b) { return a + (b - a) * (float) (rng() % 10000) / 9999.f; }
        int ri (int a, int b) { return a + (int) (rng() % (unsigned) (b - a + 1)); }
        bool chance (float pr) { return r (0.f, 1.f) < pr; }
        void set (const char* id, float v) { if (p != nullptr) p->setParam (id, v); }
        void set (const juce::String& id, float v) { if (p != nullptr) p->setParam (id, v); }

        // Pattern + custom LFO writers. The RNG is always drawn so dry runs stay in step.
        void seqRandomise (bool arp)
        {
            const auto seed = (uint32_t) rng();
            if (p == nullptr) return;
            if (arp) p->seq.randomiseArp (0, seed); else p->seq.randomise (0, seed);
        }
        void seqStep (int i, bool gate, int note, int oct, bool acc, bool slide)
        {
            if (p == nullptr) return;
            auto& st = p->seq.steps[0][i];
            st.gate = (int8_t) gate; st.note = (int8_t) note; st.octave = (int8_t) oct;
            st.accent = (int8_t) acc; st.slide = (int8_t) slide;
        }
        void lfoStep (int l, int s, float v) { if (p != nullptr) p->lfoCustom[l][s] = v; }
        void lock (int n) { set ("osc" + juce::String (n) + "_lock", 1.f); set ("osc" + juce::String (n) + "_rphase", 0.f); }

        void osc (int n, int wave, float level, int oct = 0, int semi = 0, float fine = 0.f,
                  int uni = 1, float detune = 0.25f, float blend = 0.5f)
        {
            const juce::String o = "osc" + juce::String (n) + "_";
            set (o + "on", 1.f); set (o + "wave", (float) wave); set (o + "level", level);
            set (o + "oct", (float) oct); set (o + "semi", (float) semi); set (o + "fine", fine);
            set (o + "uni", (float) uni); set (o + "detune", detune); set (o + "blend", blend);
            if (wave >= 4) tag ("wavetable");
            if (uni >= 5) tag ("wide");
        }
        void wt (int n, float pos) { set ("osc" + juce::String (n) + "_wtpos", pos); }
        void warp (int n, int mode, float amt) { if (mode == 1) tag ("fm"); if (mode == 3) tag ("ring"); set ("osc" + juce::String (n) + "_warp", (float) mode); set ("osc" + juce::String (n) + "_warpamt", amt); }
        void fixedPhase (int n) { set ("osc" + juce::String (n) + "_rphase", 0.f); }

        void flt (int n, int type, float cutoff, float reso, float env, float drive = 0.f, float key = 0.3f)
        {
            if (type == BP12 || type == BP24)
            {
                // Band-pass only passes a narrow band: keep it where the notes are or the patch goes silent
                cutoff = std::min (cutoff, 1500.f);
                reso = std::min (reso, 0.6f);
                env = std::clamp (env, -0.1f, 0.25f);
            }
            const juce::String f = "flt" + juce::String (n) + "_";
            set (f + "type", (float) type); set (f + "cutoff", cutoff); set (f + "reso", reso);
            set (f + "env", env); set (f + "drive", drive); set (f + "key", key);
            if (type == Acid24) tag ("acid");
        }
        void env (int n, float a, float d, float s, float rel)
        {
            const juce::String e = "env" + juce::String (n) + "_";
            set (e + "a", a); set (e + "d", d); set (e + "s", s); set (e + "r", rel);
        }
        void lfoHz (int n, int shape, float hz, bool retrig = false, float fade = 0.f)
        {
            const juce::String l = "lfo" + juce::String (n) + "_";
            set (l + "shape", (float) shape); set (l + "rate", hz); set (l + "sync", 0.f);
            set (l + "trig", retrig ? 1.f : 0.f); set (l + "fade", fade);
        }
        void lfoSync (int n, int shape, int div, bool retrig = false)
        {
            const juce::String l = "lfo" + juce::String (n) + "_";
            set (l + "shape", (float) shape); set (l + "sync", 1.f); set (l + "div", (float) div);
            set (l + "trig", retrig ? 1.f : 0.f);
        }
        void mod (int src, int dst, float amt)
        {
            if (slot >= kNumModSlots) return;
            const juce::String m = "mod" + juce::String (++slot) + "_";
            set (m + "src", (float) src); set (m + "dst", (float) dst); set (m + "amt", amt);
        }
        void voice (int mode, float glide = 0.f) { set ("mode", (float) mode); set ("glide", glide); tag (mode == 0 ? "poly" : "mono"); if (glide > 0.f) tag ("glide"); }

        void delay (float mix, int div = D8D, float fb = 0.4f) { set ("dly_on", 1.f); set ("dly_sync", 1.f); set ("dly_div", (float) div); set ("dly_mix", mix); set ("dly_fb", fb); }
        void reverb (float mix, float size, float decay) { if (mix >= 0.4f) tag ("spacious"); set ("rev_on", 1.f); set ("rev_mix", mix); set ("rev_size", size); set ("rev_decay", decay); }
        void chorus (float mix, float rate = 0.6f, float depth = 0.5f) { set ("mfx_on", 1.f); set ("mfx_type", 0.f); set ("mfx_mix", mix); set ("mfx_rate", rate); set ("mfx_depth", depth); }
        void phaser (float mix, float rate, float fb) { set ("mfx_on", 1.f); set ("mfx_type", 1.f); set ("mfx_mix", mix); set ("mfx_rate", rate); set ("mfx_fb", fb); }
        void distortion (int type, float drive, float mix) { tag ("dirty"); set ("dist_on", 1.f); set ("dist_type", (float) type); set ("dist_drive", drive); set ("dist_mix", mix); }
        void ott (float depth) { tag ("ott"); set ("comp_on", 1.f); set ("comp_mode", 1.f); set ("comp_depth", depth); }
        void seqOn (float gate) { tag ("sequenced"); set ("seq_on", 1.f); set ("seq_pattern", 1.f); set ("seq_gate", gate); }
    };

    void randomMotion (Ctx& c, int maxMods, bool allowAmp);
    void randomFx (Ctx& c, bool aggressive);
    void randomOsc1 (Ctx& c, std::initializer_list<int> pool, float level, int oct, int maxUni);
    void randomLayer (Ctx& c, int baseOct);
    void groove (Ctx& c, int style);

    //==========================================================================
    void acid (Ctx& c, int k)
    {
        c.tag ("line"); c.tag ("bass");
        c.voice (2, c.r (0.05f, 0.1f));
        c.set ("accent", c.r (0.6f, 1.f));
        c.set ("vel_sens", 0.2f);
        const int w = k % 3 == 2 ? Square : (k % 7 == 5 ? WtBasic : Saw);
        c.osc (1, w, 0.8f);
        if (w == WtBasic) c.wt (1, c.r (0.66f, 1.f));
        if (c.chance (0.3f)) c.osc (2, w == Square ? Square : Saw, c.r (0.3f, 0.5f), 0, 0, c.r (8.f, 15.f));
        if (c.chance (0.2f)) c.set ("sub_level", c.r (0.2f, 0.4f));
        c.flt (1, Acid24, c.r (120.f, 700.f), c.r (0.65f, 0.93f), c.r (0.45f, 0.85f), c.r (0.2f, 0.7f), 0.2f);
        if (k % 4 == 3) c.flt (2, HP12, c.r (60.f, 140.f), 0.1f, 0.f);
        c.env (1, 0.002f, 2.f, 0.9f, 0.05f);
        c.env (2, 0.001f, c.r (0.15f, 0.7f), 0.f, 0.1f);
        c.seqOn (c.r (0.45f, 0.6f));
        c.seqRandomise (false);
        if (c.chance (0.4f)) { c.lfoSync (1, 0, c.chance (0.5f) ? Bar2 : Bar4); c.mod (S_Lfo1, D_Cut1, c.r (0.1f, 0.3f)); }
        if (c.chance (0.5f)) c.distortion (c.chance (0.5f) ? 0 : 1, c.r (0.2f, 0.5f), c.r (0.5f, 1.f));
        if (c.chance (0.5f)) c.delay (c.r (0.15f, 0.3f), D8D, c.r (0.3f, 0.5f));
        if (c.chance (0.3f)) c.reverb (c.r (0.1f, 0.2f), 0.4f, 1.5f);
        if (c.chance (0.3f)) c.ott (c.r (0.3f, 0.5f));
    }

    void bass (Ctx& c, int k)
    {
        c.tag ("bass");
        c.voice (1, c.chance (0.3f) ? c.r (0.02f, 0.06f) : 0.f);
        c.set ("vel_sens", 0.3f);
        switch (k % 5)
        {
            case 0: // sub
                c.tag ("sub");
                c.osc (1, Sine, 0.9f); c.fixedPhase (1);
                if (c.chance (0.5f)) c.osc (2, Tri, 0.3f, 0);
                c.flt (1, LP12, c.r (400.f, 900.f), 0.1f, 0.f, 0.f, 0.f);
                c.env (1, 0.002f, 1.f, 1.f, c.r (0.08f, 0.15f));
                c.distortion (1, c.r (0.1f, 0.25f), 0.3f);
                break;
            case 1: // reese
                c.tag ("reese");
                c.osc (1, Saw, 0.8f, 0, 0, 0, c.ri (3, 5), c.r (0.25f, 0.45f));
                c.osc (2, Saw, 0.6f, 0, 0, c.r (10.f, 20.f), 3, 0.3f);
                c.set ("sub_level", 0.5f);
                c.flt (1, LP24, c.r (250.f, 700.f), c.r (0.2f, 0.4f), 0.1f, 0.2f);
                c.env (1, 0.005f, 1.f, 1.f, 0.15f);
                c.lfoHz (1, 0, c.r (0.1f, 0.4f));
                c.mod (S_Lfo1, D_Cut1, c.r (0.08f, 0.2f));
                c.mod (S_Lfo1, D_Detune, 0.1f);
                if (c.chance (0.3f)) c.distortion (0, 0.3f, 0.6f);
                if (c.chance (0.5f)) c.ott (0.4f);
                break;
            case 2: // pluck bass
                c.osc (1, c.chance (0.5f) ? Saw : Square, 0.8f);
                c.osc (2, Square, 0.5f, -1);
                c.flt (1, LP24, c.r (150.f, 400.f), c.r (0.3f, 0.5f), c.r (0.5f, 0.75f));
                c.env (1, 0.002f, 0.4f, 0.6f, 0.1f);
                c.env (2, 0.001f, c.r (0.15f, 0.3f), 0.f, 0.1f);
                break;
            case 3: // FM
                c.osc (1, Sine, 0.9f); c.fixedPhase (1);
                c.osc (2, Sine, 0.f, c.ri (1, 2));
                c.warp (1, 1, c.r (0.2f, 0.5f));
                c.env (3, 0.001f, c.r (0.1f, 0.3f), 0.f, 0.1f);
                c.mod (S_Env3, D_Osc1Warp, c.r (0.3f, 0.6f));
                c.flt (1, LP12, c.r (1500.f, 4000.f), 0.1f, 0.f);
                c.env (1, 0.002f, 0.8f, 0.8f, 0.1f);
                break;
            default: // wavetable growl
                c.tag ("growl");
                c.osc (1, c.chance (0.5f) ? WtDigital : WtVocal, 0.8f, 0, 0, 0, c.ri (1, 3), 0.2f);
                c.wt (1, c.r (0.2f, 0.5f));
                c.set ("sub_level", 0.4f);
                c.lfoSync (1, c.chance (0.5f) ? 1 : 0, c.chance (0.5f) ? D8 : D16, true);
                c.mod (S_Lfo1, D_Osc1Wt, c.r (0.3f, 0.5f));
                c.mod (S_Lfo1, D_Cut1, c.r (0.2f, 0.4f));
                c.flt (1, LP24, c.r (400.f, 1200.f), 0.3f, 0.1f);
                c.env (1, 0.003f, 1.f, 1.f, 0.12f);
                c.distortion (c.chance (0.5f) ? 2 : 1, 0.3f, 0.7f);
                c.ott (0.5f);
                break;
        }
    }

    void lead (Ctx& c, int k)
    {
        c.tag ("lead");
        c.env (1, 0.005f, 0.5f, 0.8f, 0.25f);
        auto vibrato = [&c]
        {
            c.lfoHz (2, 0, c.r (5.f, 6.f), true, c.r (0.3f, 0.6f));
            c.mod (S_Lfo2, D_Osc1Pitch, 0.01f);
            c.mod (S_Lfo2, D_Osc2Pitch, 0.01f);
        };
        switch (k % 5)
        {
            case 0:
                c.osc (1, Saw, 0.8f, 0, 0, 0, c.ri (7, 9), c.r (0.3f, 0.5f), 0.6f);
                c.osc (2, Saw, 0.4f, 1, 0, 0, 5, 0.3f);
                c.flt (1, LP24, c.r (3000.f, 8000.f), 0.2f, 0.2f);
                vibrato();
                c.reverb (0.2f, 0.6f, 2.5f); c.delay (0.2f);
                break;
            case 1:
                c.voice (2, c.r (0.05f, 0.15f));
                c.osc (1, Square, 0.8f);
                c.osc (2, Square, 0.5f, 0, c.chance (0.5f) ? 7 : 0, 12.f);
                c.flt (1, LP12, c.r (2000.f, 5000.f), 0.3f, 0.2f);
                vibrato(); c.delay (0.25f);
                break;
            case 2:
                c.voice (2, 0.08f);
                c.osc (1, Saw, 0.8f, 0, 0, 0, 3, 0.2f);
                c.flt (1, Acid24, c.r (600.f, 1500.f), 0.7f, 0.5f, 0.3f);
                c.env (2, 0.001f, 0.3f, 0.2f, 0.2f);
                c.distortion (1, 0.3f, 0.6f); c.delay (0.2f);
                break;
            case 3:
                c.osc (1, WtDigital, 0.8f, 0, 0, 0, 3, 0.2f); c.wt (1, c.r (0.5f, 0.9f));
                c.osc (2, WtPwm, 0.5f); c.wt (2, 0.3f);
                c.lfoHz (1, 1, c.r (0.5f, 2.f));
                c.mod (S_Lfo1, D_Osc1Wt, 0.2f);
                c.flt (1, LP24, 6000.f, 0.2f, 0.1f);
                c.chorus (0.3f); c.reverb (0.2f, 0.5f, 2.f);
                break;
            default:
                c.voice (1, 0.03f);
                c.osc (1, Saw, 0.8f); c.warp (1, 1, c.r (0.3f, 0.5f));
                c.osc (2, Sine, 0.2f, 1);
                c.env (3, 0.001f, 0.4f, 0.2f, 0.3f);
                c.mod (S_Env3, D_Osc1Warp, 0.4f);
                c.flt (1, LP24, 3000.f, 0.25f, 0.2f);
                vibrato(); c.delay (0.2f);
                break;
        }
    }

    void pad (Ctx& c, int k)
    {
        c.tag ("pad"); c.tag ("atmosphere");
        c.env (1, c.r (0.6f, 2.f), 1.f, 0.85f, c.r (2.f, 4.f));
        c.env (2, c.r (1.f, 3.f), 2.f, 0.5f, 2.f);
        switch (k % 4)
        {
            case 0:
                c.osc (1, Saw, 0.7f, 0, 0, 0, 7, c.r (0.35f, 0.5f));
                c.osc (2, Saw, 0.5f, c.chance (0.5f) ? 1 : -1, 0, 0, 5, 0.3f);
                c.flt (1, LP12, c.r (1200.f, 3000.f), 0.2f, 0.25f);
                c.lfoHz (1, 0, c.r (0.1f, 0.3f)); c.mod (S_Lfo1, D_Cut1, 0.1f);
                c.chorus (0.4f);
                c.reverb (0.45f, 0.85f, c.r (4.f, 8.f));
                break;
            case 1:
                c.osc (1, WtVocal, 0.7f, 0, 0, 0, 5, 0.3f);
                c.osc (2, WtVocal, 0.5f, 0, 7, 0, 3, 0.25f); c.wt (2, 0.5f);
                c.lfoHz (1, 1, c.r (0.05f, 0.2f)); c.mod (S_Lfo1, D_Osc1Wt, 0.4f);
                c.lfoHz (2, 0, c.r (0.07f, 0.15f)); c.mod (S_Lfo2, D_Osc2Wt, 0.3f);
                c.flt (1, LP12, c.r (2000.f, 4000.f), 0.15f, 0.1f);
                c.reverb (0.55f, 0.9f, c.r (6.f, 10.f));
                break;
            case 2:
                c.osc (1, WtPwm, 0.7f, 0, 0, 0, 3, 0.3f); c.wt (1, 0.2f);
                c.osc (2, WtPwm, 0.5f, 0, 0, 8.f, 3, 0.3f); c.wt (2, 0.5f);
                c.lfoHz (1, 0, 0.3f); c.mod (S_Lfo1, D_Osc1Wt, 0.4f); c.mod (S_Lfo1, D_Osc2Wt, -0.3f);
                c.flt (1, LP24, c.r (1000.f, 2000.f), 0.2f, 0.2f);
                c.chorus (0.35f); c.reverb (0.4f, 0.8f, 5.f);
                break;
            default:
                c.osc (1, WtDigital, 0.6f, 0, 0, 0, 5, 0.3f); c.wt (1, 0.1f);
                c.set ("noise_type", 1.f); c.set ("noise_level", 0.1f);
                c.set ("flt_routing", 1.f);
                c.flt (1, LP24, c.r (500.f, 1000.f), 0.2f, 0.2f);
                c.flt (2, BP12, 1200.f, 0.5f, 0.f);
                c.lfoHz (1, 5, c.r (0.2f, 0.6f)); c.mod (S_Lfo1, D_Cut2, 0.3f);
                c.reverb (0.6f, 0.95f, 8.f);
                break;
        }
        if (k % 5 == 4)
        {
            // Trance gate on LFO 3 (custom 16-step shape)
            c.tag ("gated");
            c.lfoSync (3, 6, Bar1);
            for (int s = 0; s < kLfoSteps; ++s) c.lfoStep (2, s, c.chance (0.65f) ? 0.f : -1.f);
            c.mod (S_Lfo3, D_Amp, 1.f);
            c.env (1, 0.01f, 1.f, 1.f, 0.4f);
        }
    }

    void pluck (Ctx& c, int k)
    {
        c.tag ("pluck");
        c.env (1, 0.001f, c.r (0.4f, 0.9f), 0.f, 0.3f);
        c.env (2, 0.001f, c.r (0.12f, 0.35f), 0.f, 0.2f);
        if (k % 4 == 3)
        {
            c.osc (1, Saw, 0.3f);
            c.set ("noise_level", 0.6f);
            c.flt (1, Comb, c.r (200.f, 600.f), 0.75f, 0.f, 0.f, 1.f);
            c.env (1, 0.001f, 0.8f, 0.f, 0.4f);
        }
        else
        {
            const int w = k % 4 == 0 ? Saw : (k % 4 == 1 ? Square : WtBasic);
            c.osc (1, w, 0.8f, 0, 0, 0, c.chance (0.5f) ? 3 : 1, 0.15f);
            if (w == WtBasic) c.wt (1, c.r (0.3f, 0.9f));
            if (c.chance (0.5f)) c.osc (2, Saw, 0.3f, 1);
            else if (c.chance (0.5f)) c.osc (2, w, 0.5f, 0, 0, 7.f);
            c.flt (1, c.chance (0.6f) ? LP24 : LP12, c.r (300.f, 900.f), c.r (0.2f, 0.45f), c.r (0.55f, 0.85f), 0.f, 0.5f);
        }
        c.delay (0.25f, D8D, 0.4f);
        c.reverb (0.25f, 0.6f, 2.f);
    }

    void keys (Ctx& c, int k)
    {
        c.tag ("keys");
        switch (k % 3)
        {
            case 0:
                c.osc (1, Sine, 0.8f); c.fixedPhase (1);
                c.osc (2, Sine, 0.3f, 1);
                c.warp (1, 1, c.r (0.1f, 0.25f));
                c.env (3, 0.001f, 0.3f, 0.f, 0.2f); c.mod (S_Env3, D_Osc1Warp, 0.2f);
                c.flt (1, LP12, c.r (3000.f, 6000.f), 0.1f, 0.1f);
                c.env (1, 0.002f, c.r (1.5f, 2.5f), c.r (0.2f, 0.4f), 0.4f);
                c.lfoHz (1, 0, c.r (4.f, 5.f)); c.mod (S_Lfo1, D_Amp, 0.15f);
                c.chorus (0.3f);
                break;
            case 1:
                c.osc (1, WtBasic, 0.7f); c.wt (1, 0.25f);
                c.osc (2, Sine, 0.5f, 1);
                c.set ("sub_level", 0.3f);
                c.env (1, 0.005f, 0.1f, 1.f, 0.1f);
                c.flt (1, LP12, 5000.f, 0.1f, 0.f);
                if (c.chance (0.5f)) c.phaser (0.4f, 0.4f, 0.4f); else c.chorus (0.4f, 5.f, 0.3f);
                break;
            default:
                c.osc (1, Square, 0.7f);
                c.osc (2, WtPwm, 0.5f); c.wt (2, 0.7f);
                c.flt (1, LP24, 1500.f, 0.3f, 0.5f);
                c.flt (2, HP12, 150.f, 0.1f, 0.f);
                c.env (1, 0.002f, 0.6f, 0.3f, 0.2f);
                c.env (2, 0.001f, 0.25f, 0.1f, 0.2f);
                break;
        }
        c.reverb (0.2f, 0.5f, 2.f);
    }

    void sequence (Ctx& c, int k)
    {
        c.tag ("arp");
        c.voice (2, 0.f);
        c.seqOn (c.r (0.3f, 0.5f));
        c.seqRandomise (true);
        c.set ("seq_transpose", 12.f);
        switch (k % 3)
        {
            case 0:
                c.osc (1, Saw, 0.8f, 0, 0, 0, 3, 0.15f);
                c.flt (1, LP24, c.r (400.f, 900.f), 0.35f, 0.6f);
                c.env (2, 0.001f, 0.2f, 0.f, 0.1f);
                break;
            case 1:
                c.osc (1, WtDigital, 0.8f); c.wt (1, 0.3f);
                c.lfoSync (1, 1, Bar2); c.mod (S_Lfo1, D_Osc1Wt, 0.5f);
                c.flt (1, LP24, 3000.f, 0.3f, 0.2f);
                break;
            default:
                c.osc (1, Sine, 0.8f); c.fixedPhase (1);
                c.osc (2, Sine, 0.f, 1); c.warp (1, 1, 0.3f);
                c.lfoSync (1, 0, Bar4); c.mod (S_Lfo1, D_Osc1Warp, 0.3f);
                c.flt (1, LP12, 4000.f, 0.1f, 0.f);
                break;
        }
        c.env (1, 0.002f, 0.4f, 0.5f, 0.1f);
        c.delay (0.3f, D8D, 0.45f);
        c.reverb (0.2f, 0.5f, 2.f);
    }

    void fx (Ctx& c, int k)
    {
        switch (k % 5)
        {
            case 0: // riser
                c.tag ("riser");
                c.osc (1, Saw, 0.3f, 0, 0, 0, 5, 0.4f);
                c.set ("noise_level", 0.7f);
                c.flt (1, BP12, 300.f, 0.6f, 0.f);
                c.env (3, c.r (4.f, 8.f), 1.f, 1.f, 1.f);
                c.mod (S_Env3, D_Cut1, 0.8f); c.mod (S_Env3, D_Osc1Pitch, 0.5f);
                c.env (1, 0.5f, 1.f, 1.f, 2.f);
                c.reverb (0.5f, 0.9f, 8.f); c.delay (0.3f);
                break;
            case 1: // downlifter
                c.tag ("downlifter");
                c.osc (1, Saw, 0.6f, 0, 0, 0, 5, 0.4f);
                c.set ("noise_level", 0.4f);
                c.env (3, 0.001f, c.r (3.f, 5.f), 0.f, 1.f);
                c.mod (S_Env3, D_Osc1Pitch, 0.5f); c.mod (S_Env3, D_Cut1, 0.6f);
                c.flt (1, LP24, 400.f, 0.3f, 0.f);
                c.env (1, 0.01f, 4.f, 0.f, 2.f);
                c.reverb (0.5f, 0.9f, 8.f);
                break;
            case 2: // S&H bleeps
                c.tag ("bleeps");
                c.osc (1, Square, 0.7f);
                c.flt (1, LP24, 1200.f, 0.7f, 0.f);
                c.lfoSync (1, 5, D16); c.mod (S_Lfo1, D_Cut1, 0.5f);
                c.env (1, 0.005f, 1.f, 1.f, 0.5f);
                c.delay (0.35f, D8D, 0.55f);
                break;
            case 3: // laser zap
                c.tag ("zap");
                c.voice (1);
                c.osc (1, Saw, 0.8f);
                c.env (3, 0.001f, c.r (0.1f, 0.25f), 0.f, 0.1f);
                c.mod (S_Env3, D_Osc1Pitch, c.r (0.9f, 1.f));
                c.flt (1, LP12, 8000.f, 0.3f, 0.f);
                c.env (1, 0.001f, 0.3f, 0.f, 0.2f);
                c.delay (0.3f, D16D, 0.5f);
                break;
            default: // drone
                c.tag ("drone");
                c.osc (1, WtVocal, 0.6f, 0, 0, 0, 9, 0.4f);
                c.osc (2, WtDigital, 0.4f, -1, 0, 0, 5, 0.3f);
                c.lfoHz (1, 1, 0.05f); c.mod (S_Lfo1, D_Osc1Wt, 0.5f);
                c.lfoHz (2, 0, 0.08f); c.mod (S_Lfo2, D_Osc2Wt, 0.5f);
                c.set ("flt_routing", 1.f);
                c.flt (1, LP24, 2000.f, 0.2f, 0.f);
                c.flt (2, Comb, 220.f, 0.6f, 0.f);
                c.env (1, 2.f, 1.f, 1.f, 5.f);
                c.reverb (0.6f, 1.f, 15.f);
                break;
        }
    }
    //==========================================================================
    // Psytrance bass at 145 BPM. Every preset rolls independent choices for
    // oscillator, layer, sub, filter, attack, movement, groove and note style.
    void psySub (Ctx& c, int k)
    {
        juce::ignoreUnused (k);
        c.tag ("psy"); c.tag ("145 bpm"); c.tag ("bass");
        c.set ("bpm", 145.f);
        c.voice (1);

        static const int waves[] = { Saw, Square, Sine, WtDigital, WtBasic, WtVocal, WtPwm, Tri };
        static const char* const waveTags[] = { "saw", "square", "sine", "digital", "wavetable", "vocal", "pwm", "triangle" };
        const int wi = c.ri (0, 7);
        c.osc (1, waves[wi], 0.85f, 0, 0, 0, c.chance (0.25f) ? 2 : 1, c.r (0.05f, 0.15f));
        c.lock (1);
        c.tag (waveTags[wi]);
        if (waves[wi] >= WtBasic) c.wt (1, c.r (0.f, 1.f));

        switch (c.ri (0, 4))   // second layer
        {
            case 1: c.osc (2, Saw, c.r (0.15f, 0.35f), 1); c.lock (2); c.tag ("bright"); break;
            case 2:
                c.osc (2, Sine, 0.f, c.ri (1, 2), c.chance (0.5f) ? 7 : 0); c.lock (2);
                c.warp (1, 1, c.r (0.1f, 0.45f));
                c.env (3, 0.001f, c.r (0.03f, 0.15f), 0.f, 0.05f);
                c.mod (S_Env3, D_Osc1Warp, c.r (0.2f, 0.6f));
                break;
            case 3: c.osc (2, Square, c.r (0.3f, 0.5f), 0, 0, c.r (5.f, 12.f)); c.lock (2); c.tag ("fat"); break;
            case 4: c.osc (2, Sine, 0.f, 0, c.ri (3, 7)); c.warp (1, 3, c.r (0.2f, 0.5f)); break;
            default: break;
        }
        c.set ("sub_wave", (float) c.ri (0, 2));
        c.set ("sub_level", c.r (0.2f, 0.8f));

        static const int filters[] = { LP24, Acid24, LP12, LP24, Acid24, LP12 };
        int ft = filters[c.ri (0, 5)];
        if (ft == BP12 && (waves[wi] == Sine || waves[wi] == Tri)) ft = LP24;   // band-pass needs harmonics to work on
        const float cutoff = ft == BP12 ? c.r (250.f, 700.f) : c.r (110.f, 900.f);
        c.flt (1, ft, cutoff, c.r (0.f, 0.6f), c.r (0.2f, 0.9f), c.r (0.f, 0.5f), c.r (0.f, 0.6f));
        if (ft == BP12) c.tag ("mid");
        if (cutoff > 500.f && ft != BP12) c.tag ("bright");
        if (cutoff < 220.f) c.tag ("dark");

        const float dec = c.r (0.09f, 0.32f);
        c.env (1, 0.001f, dec, c.chance (0.3f) ? c.r (0.1f, 0.4f) : 0.f, c.r (0.02f, 0.08f));
        c.env (2, 0.001f, c.r (0.03f, 0.25f), 0.f, 0.05f);
        c.tag (dec < 0.16f ? "tight" : "boomy");

        switch (c.ri (0, 3))   // attack character
        {
            case 1:
                c.env (4, 0.001f, c.r (0.01f, 0.04f), 0.f, 0.02f);
                c.mod (S_Env4, D_Osc1Pitch, c.r (0.1f, 0.3f));
                c.tag ("punchy");
                break;
            case 2:
                c.env (4, 0.001f, c.r (0.008f, 0.025f), 0.f, 0.01f);
                c.mod (S_Env4, D_Noise, c.r (0.3f, 0.7f));
                c.set ("noise_type", (float) c.ri (0, 1));
                c.tag ("click");
                break;
            default: break;
        }

        if (c.chance (0.35f))
        {
            c.lfoSync (1, c.ri (0, 2), c.chance (0.5f) ? Bar1 : Bar2);
            c.mod (S_Lfo1, D_Cut1, c.r (0.1f, 0.35f));
            c.tag ("moving");
        }
        c.set ("accent", c.r (0.3f, 0.9f));
        c.set ("vel_sens", 0.1f);
        c.set ("warmth", c.r (0.1f, 0.6f));
        if (c.chance (0.4f)) c.distortion (c.ri (0, 2), c.r (0.1f, 0.5f), c.r (0.3f, 0.7f));
        if (c.chance (0.3f)) c.ott (c.r (0.2f, 0.5f));

        // Groove and notes
        c.seqOn (c.r (0.4f, 0.9f));
        static const int keys[] = { -12, -10, -9, -8, -7, -5, -4, -2, 0, 2, 3, 5 };
        c.set ("seq_transpose", (float) keys[c.ri (0, 11)]);
        const int g = c.ri (0, 7);
        static const char* const grooves[] = { "rolling", "gallop", "offbeat", "driving", "16ths", "shuffle", "skipping", "broken" };
        c.tag (grooves[g]);
        if (g == 5) c.set ("shuffle", c.r (0.4f, 0.65f));
        const int noteStyle = c.ri (0, 3);
        static const int scale[] = { 0, 3, 5, 7, 10 };
        for (int i = 0; i < kNumSteps; ++i)
        {
            const int pos = i % 4;
            bool gate;
            switch (g)
            {
                case 0: case 5: gate = pos != 0; break;
                case 1: gate = pos == 2 || pos == 3; break;
                case 2: gate = pos == 2; break;
                case 3: gate = pos != 0 || i == 8; break;
                case 4: gate = true; break;
                case 6: gate = pos == 1 || pos == 3; break;
                default: gate = pos != 0 && c.chance (0.8f); break;
            }
            int note = 0, oct = 1;
            if (gate)
            {
                if (noteStyle == 1 && c.chance (0.18f)) oct = 2;
                else if (noteStyle == 2 && i >= 12 && c.chance (0.5f)) note = c.chance (0.5f) ? 7 : 5;
                else if (noteStyle == 3 && c.chance (0.3f)) note = scale[c.ri (1, 4)];
            }
            c.seqStep (i, gate, note, oct, gate && c.chance (0.25f), false);
        }
        if (noteStyle == 3) c.tag ("melodic");
    }

    //==========================================================================
    void acidLead (Ctx& c, int k)
    {
        juce::ignoreUnused (k);
        c.tag ("acid"); c.tag ("lead");
        c.voice (c.chance (0.75f) ? 2 : 1, c.r (0.03f, 0.16f));
        c.set ("accent", c.r (0.4f, 1.f));
        randomOsc1 (c, { Saw, Square, WtDigital, WtPwm, WtBasic }, 0.8f, c.chance (0.2f) ? 1 : 0, 3);
        randomLayer (c, 0);
        static const int fts[] = { Acid24, Acid24, LP24, LP12, Acid24, LP12 };
        const int ft = fts[c.ri (0, 5)];
        const bool bp = ft == BP12 || ft == BP24;
        c.flt (1, ft, bp ? c.r (500.f, 1800.f) : c.r (350.f, 3200.f), bp ? c.r (0.2f, 0.65f) : c.r (0.5f, 0.95f), c.r (0.2f, 0.85f), c.r (0.2f, 0.85f), c.r (0.2f, 0.6f));
        if (c.chance (0.3f)) c.flt (2, c.chance (0.5f) ? HP12 : Notch, c.r (150.f, 900.f), c.r (0.1f, 0.5f), 0.f);
        c.env (1, c.r (0.001f, 0.02f), c.r (0.3f, 1.2f), c.r (0.4f, 0.9f), c.r (0.08f, 0.4f));
        c.env (2, 0.001f, c.r (0.08f, 0.8f), c.r (0.f, 0.4f), 0.2f);
        if (c.chance (0.45f))
        {
            c.lfoHz (1, 0, c.r (4.5f, 6.5f), true, c.r (0.2f, 0.7f));
            c.mod (S_Lfo1, D_Osc1Pitch, c.r (0.006f, 0.015f));
            c.mod (S_Lfo1, D_Osc2Pitch, 0.01f);
            c.tag ("vibrato");
        }
        randomMotion (c, 2, false);
        c.mod (S_ModWheel, D_Cut1, 0.4f);
        randomFx (c, true);
        if (c.chance (0.25f)) { c.seqOn (c.r (0.3f, 0.7f)); groove (c, 4); }
    }

    //==========================================================================
    void fmPluck (Ctx& c, int k)
    {
        c.tag ("fm"); c.tag ("pluck");
        c.voice (0);
        const int style = k % 3;
        c.osc (1, Sine, 0.85f);
        c.lock (1);
        if (style == 2)
        {
            c.osc (2, WtDigital, 0.2f, 1);
            c.wt (2, c.r (0.3f, 0.8f));
            c.warp (1, 3, c.r (0.2f, 0.5f));
            c.tag ("metal");
        }
        else
        {
            static const int ratios[][2] = { { 1, 0 }, { 2, 0 }, { 1, 7 }, { 2, 7 }, { 3, 0 } };   // octave, semitone
            const auto& rr = ratios[c.ri (0, 4)];
            c.osc (2, Sine, c.chance (0.3f) ? 0.12f : 0.f, rr[0], rr[1]);
            c.lock (2);
            c.warp (1, 1, c.r (0.15f, 0.45f));
            c.tag (style == 0 ? "bell" : "mallet");
        }
        c.env (3, 0.001f, style == 0 ? c.r (0.2f, 0.5f) : c.r (0.05f, 0.18f), 0.f, 0.1f);
        c.mod (S_Env3, D_Osc1Warp, c.r (0.3f, 0.7f));
        if (c.chance (0.4f)) c.mod (S_Velocity, D_Osc1Warp, 0.2f);
        c.flt (1, LP12, c.r (2500.f, 9000.f), c.r (0.05f, 0.3f), c.r (0.1f, 0.4f), 0.f, 0.5f);
        c.env (1, 0.001f, style == 0 ? c.r (0.6f, 1.4f) : c.r (0.2f, 0.6f), 0.f, c.r (0.2f, 0.5f));
        c.env (2, 0.001f, c.r (0.1f, 0.3f), 0.f, 0.2f);
        c.delay (c.r (0.15f, 0.35f), c.chance (0.5f) ? D8D : D16D, c.r (0.3f, 0.45f));
        c.reverb (c.r (0.15f, 0.35f), 0.6f, c.r (1.5f, 3.f));
        if (c.chance (0.2f)) c.chorus (0.25f);
    }

    //==========================================================================
    // Sequencer grooves for the bass folders
    //   0 rolling 16ths (psy), 1 offbeat 8ths, 2 hypnotic 16ths, 3 house bounce, 4 random acid line
    void groove (Ctx& c, int style)
    {
        if (style == 4) { c.seqRandomise (false); return; }
        static const int hypnoticNotes[] = { 0, 0, 0, 0, 7, 12, 3, 0 };
        for (int i = 0; i < kNumSteps; ++i)
        {
            const int pos = i % 4;
            bool gate;
            int note = 0, oct = 1;
            switch (style)
            {
                case 0: gate = pos != 0; break;
                case 1: gate = pos == 2; break;
                case 2: gate = c.chance (0.7f); note = hypnoticNotes[c.ri (0, 7)]; break;
                default: gate = pos == 2 || (pos == 3 && c.chance (0.4f)) || (i == 0 && c.chance (0.3f)); break;
            }
            if (gate && style != 2 && i >= 12 && c.chance (0.3f))
            {
                if (c.chance (0.5f)) oct = 2; else note = c.chance (0.5f) ? 7 : 10;
            }
            const bool acc = gate && c.chance (0.3f);
            const bool slide = gate && style == 2 && c.chance (0.12f);
            c.seqStep (i, gate, note, oct, acc, slide);
        }
    }

    //==========================================================================
    void lfoMadness (Ctx& c, int k)
    {
        c.tag ("lfo"); c.tag ("crazy");
        static const int divs[] = { D4, D8, D8T, D16, D16T, D8D };
        auto pickDiv = [&c] { return divs[c.ri (0, 5)]; };
        switch (k % 8)
        {
            case 0: // wobble bass
                c.tag ("wobble"); c.tag ("bass");
                c.voice (1, 0.03f);
                c.osc (1, Saw, 0.8f, -1, 0, 0, 3, 0.2f);
                c.osc (2, Square, 0.6f, -1, 0, c.r (5.f, 10.f));
                c.set ("sub_level", 0.5f);
                c.flt (1, LP24, c.r (120.f, 300.f), c.r (0.4f, 0.7f), 0.f, c.r (0.3f, 0.6f), 0.2f);
                c.lfoSync (1, c.chance (0.5f) ? 0 : 1, pickDiv(), true);
                c.mod (S_Lfo1, D_Cut1, c.r (0.5f, 0.8f));
                c.mod (S_ModWheel, D_Lfo1Rate, 0.5f);
                c.env (1, 0.003f, 1.f, 1.f, 0.1f);
                c.distortion (c.chance (0.5f) ? 2 : 0, c.r (0.3f, 0.6f), 0.8f);
                c.ott (c.r (0.4f, 0.7f));
                break;
            case 1: // trance gate
                c.tag ("gated"); c.tag ("trance"); c.tag ("pad");
                c.osc (1, Saw, 0.7f, 0, 0, 0, 9, c.r (0.3f, 0.5f));
                c.osc (2, Saw, 0.5f, 1, 0, 0, 5, 0.3f);
                c.flt (1, LP12, c.r (2000.f, 6000.f), 0.2f, 0.1f);
                c.env (1, 0.01f, 1.f, 1.f, 0.4f);
                c.lfoSync (1, 6, Bar1);
                for (int s = 0; s < kLfoSteps; ++s) c.lfoStep (0, s, c.chance (0.6f) ? 0.f : -1.f);
                c.mod (S_Lfo1, D_Amp, 1.f);
                c.lfoSync (2, 0, Bar2);
                c.mod (S_Lfo2, D_Cut1, 0.3f);
                c.delay (0.25f, D8D, 0.4f);
                c.reverb (0.3f, 0.8f, 4.f);
                break;
            case 2: // sample & hold chaos
                c.tag ("s&h"); c.tag ("bleeps");
                c.osc (1, Square, 0.8f);
                c.flt (1, Acid24, c.r (600.f, 1500.f), c.r (0.6f, 0.85f), 0.2f, 0.3f);
                c.lfoSync (1, 5, D16); c.mod (S_Lfo1, D_Cut1, c.r (0.4f, 0.7f));
                c.lfoSync (2, 5, D8);  c.mod (S_Lfo2, D_Osc1Pitch, c.chance (0.5f) ? 0.5f : 0.25f);
                c.lfoSync (3, 5, D16); c.mod (S_Lfo3, D_Pan, 0.7f);
                c.env (1, 0.002f, 0.3f, 0.6f, 0.2f);
                c.delay (0.35f, D8D, 0.55f);
                break;
            case 3: // audio-rate FM growl
                c.tag ("audio-rate"); c.tag ("growl"); c.tag ("fm"); c.tag ("bass");
                c.voice (1, 0.02f);
                c.osc (1, Saw, 0.8f, -1, 0, 0, 2, 0.15f);
                c.set ("sub_level", 0.5f);
                c.lfoHz (1, 0, c.r (80.f, 400.f), true);
                c.mod (S_Lfo1, D_Osc1Pitch, c.r (0.02f, 0.06f));
                c.lfoSync (2, 1, pickDiv(), true);
                c.mod (S_Lfo2, D_Lfo1Rate, c.r (0.2f, 0.4f));
                c.mod (S_Lfo2, D_Cut1, c.r (0.3f, 0.5f));
                c.flt (1, LP24, c.r (300.f, 800.f), c.r (0.3f, 0.6f), 0.f, 0.5f);
                c.env (1, 0.003f, 1.f, 1.f, 0.1f);
                c.distortion (0, c.r (0.4f, 0.7f), 0.9f);
                c.ott (0.6f);
                c.set ("oversample", 2.f);
                break;
            case 4: // stutter pluck
                c.tag ("stutter"); c.tag ("pluck");
                c.osc (1, Saw, 0.8f, 0, 0, 0, 3, 0.15f);
                c.flt (1, LP24, c.r (500.f, 1500.f), 0.35f, 0.6f);
                c.env (1, 0.001f, 0.8f, 0.3f, 0.3f);
                c.env (2, 0.001f, 0.3f, 0.f, 0.2f);
                c.lfoSync (1, 4, c.chance (0.5f) ? D32 : D16T, true); c.mod (S_Lfo1, D_Amp, 0.9f);
                c.lfoSync (2, 3, Bar1, true); c.mod (S_Lfo2, D_Cut1, 0.5f);
                c.delay (0.3f, D16D, 0.45f);
                c.reverb (0.2f, 0.5f, 2.f);
                break;
            case 5: // pitch-zap riser
                c.tag ("riser"); c.tag ("fx");
                c.osc (1, Saw, 0.6f, 0, 0, 0, 7, 0.4f);
                c.set ("noise_level", 0.4f);
                c.flt (1, BP12, 600.f, 0.6f, 0.f);
                c.lfoSync (1, 2, c.chance (0.5f) ? Bar2 : Bar4);
                c.mod (S_Lfo1, D_Osc1Pitch, 0.5f);
                c.mod (S_Lfo1, D_Cut1, 0.7f);
                c.lfoSync (2, 5, D16); c.mod (S_Lfo2, D_Pan, 0.9f);
                c.env (1, 0.2f, 1.f, 1.f, 1.f);
                c.reverb (0.5f, 0.9f, 8.f);
                c.delay (0.3f);
                break;
            case 6: // ring-mod madness
                c.tag ("ring"); c.tag ("metal");
                c.osc (1, Saw, 0.8f);
                c.osc (2, Sine, 0.f, c.ri (0, 1), c.ri (0, 7));
                c.warp (1, 3, c.r (0.5f, 0.9f));
                c.lfoSync (1, 1, pickDiv()); c.mod (S_Lfo1, D_Osc2Pitch, c.r (0.2f, 0.4f));
                c.lfoSync (2, 5, D16); c.mod (S_Lfo2, D_Osc1Warp, 0.4f);
                c.flt (1, LP24, 3000.f, 0.3f, 0.2f);
                c.env (1, 0.002f, 0.6f, 0.7f, 0.2f);
                c.delay (0.25f, D8D, 0.4f);
                break;
            default: // wavetable scanner
                c.tag ("scanner"); c.tag ("wavetable");
                c.osc (1, c.chance (0.5f) ? WtDigital : WtVocal, 0.8f, 0, 0, 0, 5, 0.25f);
                c.osc (2, WtPwm, 0.4f, -1);
                c.lfoSync (1, 2, pickDiv(), true); c.mod (S_Lfo1, D_Osc1Wt, 1.f);
                c.lfoSync (2, 1, D8); c.mod (S_Lfo2, D_Osc2Wt, 0.7f); c.mod (S_Lfo2, D_Cut1, 0.3f);
                c.lfoSync (3, 6, Bar1);
                for (int s = 0; s < kLfoSteps; ++s) c.lfoStep (2, s, c.chance (0.7f) ? 0.f : -0.8f);
                c.mod (S_Lfo3, D_Amp, 0.8f);
                c.flt (1, LP24, 4000.f, 0.25f, 0.1f);
                c.env (1, 0.005f, 1.f, 1.f, 0.3f);
                c.distortion (1, 0.3f, 0.6f);
                c.delay (0.25f);
                break;
        }
        if (c.chance (0.5f)) randomOsc1 (c, { Saw, Square, WtDigital, WtVocal, WtPwm, WtBasic, Tri }, 0.8f, (k % 8) == 0 || (k % 8) == 3 ? -1 : 0, 5);
        c.flt (1, c.chance (0.5f) ? LP24 : Acid24, c.r (200.f, 4000.f), c.r (0.1f, 0.8f), c.r (0.f, 0.5f), c.r (0.f, 0.6f), 0.3f);
        randomMotion (c, 2, (k % 8) != 1);
        if (c.chance (0.6f)) randomFx (c, (k % 8) == 0 || (k % 8) == 3);
    }

    //==========================================================================
    // Techno bass, 8 distinct sound families x many grooves
    void technoBass (Ctx& c, int k)
    {
        juce::ignoreUnused (k);
        c.tag ("techno"); c.tag ("bass");
        c.set ("bpm", c.r (126.f, 138.f));
        c.voice (1);
        c.set ("warmth", c.r (0.2f, 0.7f));
        const int family = c.ri (0, 7);
        switch (family)
        {
            case 0: // rumble
                c.tag ("rumble");
                c.osc (1, c.chance (0.5f) ? Sine : Tri, 0.9f, -1); c.lock (1);
                c.osc (2, Tri, c.r (0.2f, 0.5f), -1, 0, c.r (3.f, 9.f));
                c.flt (1, LP24, c.r (120.f, 320.f), c.r (0.1f, 0.4f), 0.2f, c.r (0.3f, 0.6f), 0.f);
                c.env (1, 0.002f, c.r (0.3f, 0.8f), 0.3f, 0.3f);
                c.lfoSync (1, 3, c.chance (0.5f) ? D16 : D8); c.mod (S_Lfo1, D_Amp, c.r (-0.7f, -0.3f));
                c.reverb (c.r (0.3f, 0.55f), c.r (0.6f, 0.95f), c.r (2.f, 7.f));
                c.distortion (c.ri (0, 1), c.r (0.3f, 0.7f), 0.7f);
                break;
            case 1: // acid stab
                c.tag ("acid"); c.tag ("stab");
                c.osc (1, c.chance (0.7f) ? Saw : Square, 0.85f);
                c.flt (1, Acid24, c.r (180.f, 700.f), c.r (0.7f, 0.95f), c.r (0.4f, 0.85f), c.r (0.3f, 0.8f));
                c.env (2, 0.001f, c.r (0.08f, 0.3f), 0.f, 0.1f);
                c.env (1, 0.001f, c.r (0.2f, 0.4f), 0.f, 0.1f);
                c.distortion (c.ri (0, 2), c.r (0.3f, 0.7f), 0.8f);
                break;
            case 2: // hypnotic pulse
                c.tag ("hypnotic");
                c.osc (1, Square, 0.7f);
                c.osc (2, Saw, 0.5f, 0, 0, 12.f);
                c.flt (1, LP24, c.r (250.f, 800.f), c.r (0.3f, 0.6f), 0.4f, 0.3f);
                c.lfoSync (1, c.ri (0, 2), c.chance (0.5f) ? Bar4 : Bar2); c.mod (S_Lfo1, D_Cut1, c.r (0.25f, 0.5f));
                c.env (1, 0.002f, c.r (0.15f, 0.35f), 0.f, 0.1f);
                c.env (2, 0.001f, c.r (0.08f, 0.2f), 0.f, 0.1f);
                c.delay (c.r (0.15f, 0.3f), c.chance (0.5f) ? D16D : D8D, 0.5f);
                break;
            case 3: // distorted kick-sub
                c.tag ("distorted"); c.tag ("sub");
                c.osc (1, Sine, 0.95f, -1); c.lock (1);
                c.flt (1, LP12, c.r (1200.f, 4000.f), 0.1f, 0.f, c.r (0.5f, 0.9f), 0.f);
                c.env (1, 0.001f, c.r (0.25f, 0.5f), 0.2f, 0.1f);
                c.env (3, 0.001f, c.r (0.03f, 0.1f), 0.f, 0.05f); c.mod (S_Env3, D_Osc1Pitch, c.r (0.15f, 0.35f));
                c.distortion (c.ri (0, 2), c.r (0.5f, 0.85f), 0.9f);
                c.ott (c.r (0.3f, 0.6f));
                break;
            case 4: // FM stab
                c.tag ("fm"); c.tag ("stab");
                c.osc (1, Sine, 0.85f, -1); c.lock (1);
                c.osc (2, Sine, 0.f, c.ri (0, 2), c.chance (0.5f) ? 7 : 0);
                c.warp (1, 1, c.r (0.3f, 0.7f));
                c.env (3, 0.001f, c.r (0.06f, 0.25f), 0.f, 0.1f); c.mod (S_Env3, D_Osc1Warp, c.r (0.4f, 0.8f));
                c.flt (1, LP24, c.r (800.f, 3000.f), 0.3f, 0.3f, 0.3f);
                c.env (1, 0.001f, c.r (0.2f, 0.4f), 0.f, 0.15f);
                c.delay (0.2f, D8D, 0.4f);
                break;
            case 5: // industrial noise bass
                c.tag ("industrial"); c.tag ("noise");
                c.osc (1, Saw, 0.7f, -1);
                c.set ("noise_level", c.r (0.2f, 0.5f));
                c.set ("flt_routing", 2.f);
                c.flt (1, LP24, c.r (200.f, 500.f), 0.3f, 0.4f, 0.5f);
                c.flt (2, Comb, c.r (80.f, 200.f), c.r (0.5f, 0.85f), 0.f);
                c.env (1, 0.001f, c.r (0.15f, 0.35f), 0.f, 0.1f);
                c.distortion (2, c.r (0.4f, 0.7f), 0.8f);
                break;
            case 6: // metallic ring bass
                c.tag ("metal"); c.tag ("ring");
                c.osc (1, Square, 0.8f, -1);
                c.osc (2, Sine, 0.f, 0, c.ri (5, 11));
                c.warp (1, 3, c.r (0.3f, 0.7f));
                c.flt (1, LP24, c.r (400.f, 1500.f), c.r (0.2f, 0.5f), 0.3f, 0.3f);
                c.env (1, 0.001f, c.r (0.15f, 0.3f), 0.f, 0.1f);
                c.reverb (0.2f, 0.5f, 1.5f);
                break;
            default: // techno reese
                c.tag ("reese");
                c.osc (1, Saw, 0.75f, -1, 0, 0, 4, c.r (0.2f, 0.4f));
                c.osc (2, Saw, 0.5f, -1, 0, c.r (10.f, 25.f), 2, 0.2f);
                c.flt (1, LP24, c.r (200.f, 600.f), 0.3f, 0.2f, 0.3f);
                c.lfoHz (1, 0, c.r (0.1f, 0.5f)); c.mod (S_Lfo1, D_Cut1, 0.2f); c.mod (S_Lfo1, D_Detune, 0.15f);
                c.env (1, 0.003f, 0.6f, 0.5f, 0.15f);
                c.ott (c.r (0.3f, 0.6f));
                break;
        }
        // Groove: sequenced on most, played from MIDI on some
        if (family != 3 && c.chance (0.85f))
        {
            c.seqOn (c.r (0.3f, 0.8f));
            static const int keys[] = { -7, -5, -4, -2, 0, 1, 3, 5 };
            c.set ("seq_transpose", (float) keys[c.ri (0, 7)]);
            static const char* const grooves[] = { "offbeat", "16ths", "syncopated", "hypnotic", "rolling", "random" };
            const int gs = c.ri (0, 5);
            c.tag (grooves[gs]);
            if (gs == 3) { groove (c, 2); return; }
            if (gs == 5) { groove (c, 4); return; }
            for (int i = 0; i < kNumSteps; ++i)
            {
                const int pos = i % 4;
                bool gate;
                switch (gs)
                {
                    case 0: gate = pos == 2; break;
                    case 1: gate = c.chance (0.85f); break;
                    case 2: gate = (i % 3) == 0; break;
                    default: gate = pos != 0; break;
                }
                int note = 0, oct = 1;
                if (gate && c.chance (0.2f)) { if (c.chance (0.5f)) oct = c.chance (0.5f) ? 2 : 0; else note = c.chance (0.5f) ? 7 : 3; }
                c.seqStep (i, gate, note, oct, gate && c.chance (0.3f), gate && c.chance (0.08f));
            }
        }
    }

    //==========================================================================
    // Shared random building blocks, used to give every preset its own character

    // Adds 0..maxMods extra LFO routings on LFOs 4-8 (generators use 1-3 themselves)
    void randomMotion (Ctx& c, int maxMods, bool allowAmp)
    {
        static const int divs[] = { D32, D16T, D16, D16D, D8T, D8, D8D, D4, Bar1, Bar2 };
        const int count = c.ri (0, maxMods);
        for (int m = 0; m < count; ++m)
        {
            const int lfoN = 4 + m;
            const int shape = c.ri (0, 6);
            if (c.chance (0.75f)) c.lfoSync (lfoN, shape, divs[c.ri (0, 9)], c.chance (0.5f));
            else c.lfoHz (lfoN, shape, c.r (0.1f, 12.f), c.chance (0.5f), c.chance (0.3f) ? c.r (0.2f, 1.f) : 0.f);
            if (shape == 6)
                for (int s = 0; s < kLfoSteps; ++s) c.lfoStep (lfoN - 1, s, c.chance (0.5f) ? c.r (-1.f, 1.f) : 0.f);
            const int src = S_Lfo1 + lfoN - 1;
            switch (c.ri (0, allowAmp ? 7 : 6))
            {
                case 0: c.mod (src, D_Cut1, c.r (0.15f, 0.6f)); c.tag ("filter sweep"); break;
                case 1: c.mod (src, D_Osc1Pitch, shape == 5 ? (c.chance (0.5f) ? 0.25f : 0.5f) : c.r (0.004f, 0.02f)); break;
                case 2: c.mod (src, D_Osc1Wt, c.r (0.3f, 1.f)); break;
                case 3: c.mod (src, D_Pan, c.r (0.3f, 0.9f)); c.tag ("stereo"); break;
                case 4: c.mod (src, D_Res1, c.r (0.15f, 0.4f)); break;
                case 5: c.mod (src, D_Osc1Warp, c.r (0.2f, 0.5f)); break;
                case 6: c.mod (src, D_Detune, c.r (0.2f, 0.5f)); break;
                default: c.mod (src, D_Amp, -c.r (0.3f, 0.9f)); c.tag ("pumping"); break;
            }
        }
        if (count > 0) c.tag ("moving");
    }

    // Random effect chain; aggressive = more drive / OTT
    void randomFx (Ctx& c, bool aggressive)
    {
        static const int divs[] = { D16, D16D, D8, D8D, D8T, D4, D4D };
        c.set ("warmth", c.r (0.1f, aggressive ? 0.8f : 0.5f));
        if (c.chance (aggressive ? 0.7f : 0.3f)) c.distortion (c.ri (0, 3), c.r (0.15f, aggressive ? 0.8f : 0.4f), c.r (0.3f, 1.f));
        if (c.chance (aggressive ? 0.6f : 0.2f)) c.ott (c.r (0.3f, aggressive ? 0.9f : 0.5f));
        if (c.chance (0.5f)) c.delay (c.r (0.1f, 0.35f), divs[c.ri (0, 6)], c.r (0.25f, 0.6f));
        if (c.chance (0.45f)) c.reverb (c.r (0.1f, 0.4f), c.r (0.3f, 0.9f), c.r (1.f, 5.f));
        switch (c.ri (0, 5))
        {
            case 0: c.chorus (c.r (0.2f, 0.5f), c.r (0.2f, 1.5f), c.r (0.3f, 0.8f)); break;
            case 1: c.phaser (c.r (0.2f, 0.5f), c.r (0.1f, 1.f), c.r (0.2f, 0.7f)); break;
            case 2: c.set ("mfx_on", 1.f); c.set ("mfx_type", 2.f); c.set ("mfx_mix", c.r (0.2f, 0.5f)); c.set ("mfx_fb", c.r (0.3f, 0.8f)); c.tag ("flanger"); break;
            default: break;
        }
    }

    // Oscillator 1 from a pool of waves (wavetables get a random position)
    void randomOsc1 (Ctx& c, std::initializer_list<int> pool, float level, int oct, int maxUni)
    {
        const int idx = c.ri (0, (int) pool.size() - 1);
        const int wave = *(pool.begin() + idx);
        c.osc (1, wave, level, oct, 0, 0, c.ri (1, maxUni), c.r (0.08f, 0.45f), c.r (0.3f, 0.8f));
        if (wave >= WtBasic) c.wt (1, c.r (0.f, 1.f));
    }

    // Optional second layer: detuned copy, octave, fifth, FM, ring, or none
    void randomLayer (Ctx& c, int baseOct)
    {
        switch (c.ri (0, 6))
        {
            case 0: c.osc (2, Saw, c.r (0.2f, 0.5f), baseOct, 0, c.r (5.f, 20.f), c.ri (1, 3), 0.2f); c.tag ("detuned"); break;
            case 1: c.osc (2, c.chance (0.5f) ? Square : Saw, c.r (0.15f, 0.4f), baseOct + 1); break;
            case 2: c.osc (2, Saw, c.r (0.15f, 0.35f), baseOct, 7); c.tag ("fifth"); break;
            case 3: c.osc (2, Sine, 0.f, baseOct + c.ri (0, 2)); c.warp (1, 1, c.r (0.1f, 0.5f)); break;
            case 4: c.osc (2, Sine, 0.f, baseOct, c.ri (3, 9)); c.warp (1, 3, c.r (0.2f, 0.6f)); break;
            case 5: c.osc (2, WtPwm, c.r (0.2f, 0.45f), baseOct); c.wt (2, c.r (0.f, 1.f)); break;
            default: break;
        }
    }

    // Optional sequencer groove for bass banks
    void randomGroove (Ctx& c, float chanceOn, std::initializer_list<int> styles)
    {
        if (! c.chance (chanceOn)) return;
        c.seqOn (c.r (0.3f, 0.85f));
        static const int keys[] = { -7, -5, -4, -2, 0, 2, 3, 5 };
        c.set ("seq_transpose", (float) keys[c.ri (0, 7)]);
        groove (c, *(styles.begin() + c.ri (0, (int) styles.size() - 1)));
        if (c.chance (0.25f)) c.set ("shuffle", c.r (0.2f, 0.6f));
    }

    //==========================================================================
    void houseBass (Ctx& c, int k)
    {
        juce::ignoreUnused (k);
        c.tag ("house"); c.tag ("bass");
        c.set ("bpm", c.r (118.f, 127.f));
        const bool legato = c.chance (0.35f);
        c.voice (legato ? 2 : 1, legato ? c.r (0.03f, 0.1f) : 0.f);
        static const char* const fam[] = { "organ", "pluck", "funky", "garage", "sub", "stab" };
        const int f = c.ri (0, 5);
        c.tag (fam[f]);
        switch (f)
        {
            case 0: randomOsc1 (c, { Sine, Tri, WtBasic }, 0.85f, 0, 1); c.lock (1); c.osc (2, Sine, c.r (0.2f, 0.5f), 1); c.lock (2); break;
            case 1: randomOsc1 (c, { Saw, Square, WtPwm }, 0.8f, 0, 2); randomLayer (c, 0); break;
            case 2: randomOsc1 (c, { Square, Saw, WtDigital }, 0.8f, 0, 2); randomLayer (c, 0); break;
            case 3: randomOsc1 (c, { Saw, WtVocal, WtDigital }, 0.8f, 0, 3); c.lfoSync (1, c.ri (0, 2), c.chance (0.5f) ? D8 : D8T, true); c.mod (S_Lfo1, D_Cut1, c.r (0.3f, 0.6f)); break;
            case 4: randomOsc1 (c, { Sine, Tri }, 0.9f, 0, 1); c.lock (1); break;
            default: randomOsc1 (c, { Saw, Square, WtBasic }, 0.8f, 0, 3); randomLayer (c, 0); break;
        }
        c.set ("sub_level", c.chance (0.6f) ? c.r (0.15f, 0.6f) : 0.f);
        static const int fts[] = { LP12, LP24, Acid24, BP12 };
        int ft = fts[c.ri (0, 3)];
        if (ft == BP12 && (f == 0 || f == 4)) ft = LP12;   // sine / triangle families need a low-pass
        c.flt (1, ft, ft == BP12 ? c.r (250.f, 800.f) : c.r (250.f, 2500.f), ft == BP12 ? c.r (0.f, 0.4f) : c.r (0.f, 0.6f), c.r (0.f, 0.8f), c.r (0.f, 0.4f), c.r (0.f, 0.5f));
        c.env (1, c.r (0.001f, 0.01f), c.r (0.15f, 0.8f), c.chance (0.5f) ? c.r (0.2f, 0.8f) : 0.f, c.r (0.05f, 0.25f));
        c.env (2, 0.001f, c.r (0.05f, 0.5f), c.r (0.f, 0.3f), 0.1f);
        randomMotion (c, 1, false);
        randomFx (c, false);
        randomGroove (c, 0.5f, { 1, 3, 4 });
    }

    //==========================================================================
    void deepBass (Ctx& c, int k)
    {
        juce::ignoreUnused (k);
        c.tag ("deep"); c.tag ("bass");
        const bool legato = c.chance (0.4f);
        c.voice (legato ? 2 : 1, legato ? c.r (0.04f, 0.15f) : 0.f);
        static const char* const fam[] = { "sub", "dub", "reese", "analog", "pluck", "warm" };
        const int f = c.ri (0, 5);
        c.tag (fam[f]);
        switch (f)
        {
            case 0: randomOsc1 (c, { Sine, Tri }, 0.9f, 0, 1); c.lock (1); if (c.chance (0.5f)) c.osc (2, Tri, c.r (0.2f, 0.4f), 0, 0, c.r (3.f, 9.f)); break;
            case 1: randomOsc1 (c, { Saw, Square, WtPwm }, 0.75f, 0, 3); c.reverb (c.r (0.25f, 0.45f), c.r (0.6f, 0.9f), c.r (2.f, 6.f)); break;
            case 2: c.osc (1, Saw, 0.7f, 0, 0, 0, c.ri (3, 6), c.r (0.2f, 0.45f)); c.osc (2, Saw, 0.5f, 0, 0, c.r (8.f, 25.f), 2, 0.2f);
                    c.lfoHz (1, 0, c.r (0.05f, 0.4f)); c.mod (S_Lfo1, D_Cut1, c.r (0.1f, 0.3f)); break;
            case 3: randomOsc1 (c, { Saw, Square }, 0.75f, 0, 1); randomLayer (c, -1); break;
            case 4: randomOsc1 (c, { Tri, Sine, WtBasic, WtVocal }, 0.85f, 0, 1); c.osc (2, Sine, c.r (0.1f, 0.35f), 1); break;
            default: randomOsc1 (c, { WtBasic, WtVocal, Tri, Saw }, 0.8f, 0, 2); randomLayer (c, 0); break;
        }
        c.set ("sub_level", c.chance (0.5f) ? c.r (0.2f, 0.6f) : 0.f);
        static const int fts[] = { LP12, LP24, Acid24 };
        c.flt (1, fts[c.ri (0, 2)], c.r (180.f, 1400.f), c.r (0.f, 0.45f), c.r (0.f, 0.6f), c.r (0.f, 0.4f), c.r (0.f, 0.5f));
        const bool pluck = f == 4 || c.chance (0.3f);
        c.env (1, c.r (0.001f, 0.03f), c.r (0.2f, 1.2f), pluck ? 0.f : c.r (0.4f, 0.9f), c.r (0.08f, 0.5f));
        c.env (2, 0.001f, c.r (0.1f, 0.8f), c.r (0.f, 0.4f), 0.2f);
        if (pluck) c.tag ("pluck");
        randomMotion (c, 1, false);
        randomFx (c, false);
        randomGroove (c, 0.35f, { 1, 3, 2 });
    }

    //==========================================================================
    void hardLead (Ctx& c, int k)
    {
        c.tag ("lead"); c.tag ("in your face");
        c.set ("warmth", c.r (0.4f, 0.7f));
        auto vibrato = [&c]
        {
            c.lfoHz (2, 0, c.r (5.f, 6.5f), true, c.r (0.2f, 0.5f));
            c.mod (S_Lfo2, D_Osc1Pitch, 0.012f);
            c.mod (S_Lfo2, D_Osc2Pitch, 0.012f);
        };
        switch (k % 5)
        {
            case 0: // supersaw anthem
                c.tag ("supersaw"); c.tag ("trance"); c.tag ("big");
                c.osc (1, Saw, 0.8f, 0, 0, 0, c.ri (12, 16), c.r (0.4f, 0.6f), 0.65f);
                c.osc (2, Saw, 0.45f, 1, 0, 0, 9, 0.4f);
                c.flt (1, LP24, c.r (5000.f, 10000.f), 0.15f, 0.15f);
                c.env (1, 0.005f, 0.6f, 0.85f, 0.35f);
                c.distortion (1, 0.25f, 0.5f);
                c.ott (c.r (0.6f, 0.8f));
                c.reverb (0.3f, 0.75f, 3.f);
                c.delay (0.25f, D8D, 0.4f);
                break;
            case 1: // screamer
                c.tag ("scream"); c.tag ("psy");
                c.voice (2, c.r (0.04f, 0.1f));
                c.osc (1, Square, 0.8f, 0, 0, 0, 3, 0.15f);
                c.osc (2, Sine, 0.f, 1);
                c.warp (1, 1, c.r (0.15f, 0.35f));
                c.flt (1, Acid24, c.r (1200.f, 3000.f), c.r (0.75f, 0.9f), 0.4f, 0.6f);
                c.env (2, 0.001f, 0.4f, 0.4f, 0.2f);
                vibrato();
                c.distortion (2, c.r (0.5f, 0.7f), 0.8f);
                c.delay (0.3f, D8D, 0.5f);
                break;
            case 2: // hoover
                c.tag ("hoover"); c.tag ("rave");
                c.voice (2, c.r (0.08f, 0.15f));
                c.osc (1, Saw, 0.75f, 0, 0, 0, 7, c.r (0.5f, 0.7f));
                c.osc (2, WtPwm, 0.5f, -1, 0, 0, 5, 0.5f); c.wt (2, 0.3f);
                c.env (3, 0.001f, c.r (0.3f, 0.6f), 0.f, 0.2f); c.mod (S_Env3, D_Osc1Pitch, 0.1f); c.mod (S_Env3, D_Osc2Pitch, 0.1f);
                c.flt (1, LP24, 6000.f, 0.2f, 0.f);
                c.chorus (0.45f);
                c.distortion (0, 0.4f, 0.6f);
                c.ott (0.5f);
                break;
            case 3: // distorted digital
                c.tag ("digital"); c.tag ("aggressive");
                c.osc (1, WtDigital, 0.8f, 0, 0, 0, 5, 0.3f); c.wt (1, c.r (0.4f, 0.8f));
                c.lfoSync (1, 1, Bar1); c.mod (S_Lfo1, D_Osc1Wt, 0.3f);
                c.flt (1, LP24, c.r (4000.f, 8000.f), 0.3f, 0.2f);
                c.distortion (c.chance (0.3f) ? 3 : 2, c.r (0.5f, 0.75f), 0.8f);
                c.ott (c.r (0.7f, 0.9f));
                c.delay (0.2f, D16D, 0.4f);
                break;
            default: // psy lead
                c.tag ("psy"); c.tag ("acid");
                c.voice (2, c.r (0.03f, 0.08f));
                c.osc (1, Saw, 0.85f, 0, 0, 0, 2, 0.12f);
                c.flt (1, Acid24, c.r (800.f, 2000.f), c.r (0.7f, 0.9f), 0.5f, 0.5f);
                c.lfoSync (1, 5, D16); c.mod (S_Lfo1, D_Cut1, c.r (0.2f, 0.4f));
                c.env (2, 0.001f, 0.25f, 0.2f, 0.2f);
                c.distortion (0, c.r (0.4f, 0.6f), 0.8f);
                c.phaser (0.3f, 0.3f, 0.5f);
                c.delay (0.35f, D8D, 0.55f);
                break;
        }
        // Re-roll the core sound so every lead in a family is different
        if (c.chance (0.6f)) randomOsc1 (c, { Saw, Square, WtDigital, WtVocal, WtPwm, WtBasic }, 0.8f, c.chance (0.2f) ? 1 : 0, 16);
        {
            static const int fts[] = { LP24, LP12, Acid24, BP24 };
            const int ft = fts[c.ri (0, 3)];
            c.flt (1, ft, ft == BP24 ? c.r (700.f, 2000.f) : c.r (1500.f, 12000.f), ft == BP24 ? c.r (0.f, 0.5f) : c.r (0.f, 0.7f), c.r (0.f, 0.6f), c.r (0.f, 0.6f), 0.3f);
        }
        c.env (1, c.r (0.001f, 0.05f), c.r (0.2f, 1.f), c.r (0.4f, 1.f), c.r (0.1f, 0.6f));
        if (c.chance (0.5f)) randomLayer (c, 0);
        randomMotion (c, 2, false);
        randomFx (c, true);
        c.mod (S_ModWheel, D_Cut1, 0.4f);
    }

    //==========================================================================
    void run (int gen, Ctx& c, int k)
    {
        switch (gen)
        {
            case 0: psySub (c, k); break;
            case 1: acidLead (c, k); break;
            case 2: fmPluck (c, k); break;
            case 3: fx (c, k); c.tag ("fx"); break;
            case 4: acid (c, k); break;
            case 5: bass (c, k); break;
            case 6: lead (c, k); break;
            case 7: pad (c, k); break;
            case 8: pluck (c, k); break;
            case 9: keys (c, k); break;
            case 10: sequence (c, k); break;
            case 11: lfoMadness (c, k); break;
            case 12: technoBass (c, k); break;
            case 13: houseBass (c, k); break;
            case 14: deepBass (c, k); break;
            default: hardLead (c, k); break;
        }
    }

    uint32_t seedFor (int index) { return (uint32_t) index * 7919u + 13u; }

    // Tags for every factory preset, collected once with a dry run of each recipe
    const std::vector<juce::StringArray>& metadata()
    {
        static const std::vector<juce::StringArray> meta = []
        {
            std::vector<juce::StringArray> m;
            int index = 0;
            for (auto& cat : cats)
                for (int l = 0; l < cat.size; ++l, ++index)
                {
                    juce::StringArray t;
                    Ctx ctx { nullptr, std::mt19937 (seedFor (index)), &t };
                    run (cat.gen, ctx, l);
                    if (! t.contains ("mono") && ! t.contains ("poly")) t.add ("poly");
                    t.sort (true);
                    m.push_back (t);
                }
            return m;
        }();
        return meta;
    }
}

namespace Presets
{
    int count()
    {
        int n = 0;
        for (auto& c : cats) n += c.size;
        return n;
    }

    const juce::StringArray& categories()
    {
        static juce::StringArray names = []
        {
            juce::StringArray a;
            for (auto& c : cats) a.add (c.name);
            return a;
        }();
        return names;
    }

    int categoryStart (int category)
    {
        int s = 0;
        for (int i = 0; i < category && i < numCats; ++i) s += cats[i].size;
        return s;
    }

    int categorySize (int category) { return cats[juce::jlimit (0, numCats - 1, category)].size; }

    juce::String category (int index)
    {
        int c, l; locate (index, c, l);
        return cats[c].name;
    }

    juce::String name (int index)
    {
        int c, l; locate (index, c, l);
        static const char* const suffix[] = { "", " MK2", " X", " Max", " Prime", " Zero", " Nova", " Flux", " Ultra", " Omega" };
        return juce::String (cats[c].adj[l % 20]) + " " + cats[c].noun[(l / 20 + l) % 10] + suffix[(l / 200) % 10];
    }

    const juce::StringArray& tags (int index)
    {
        const auto& m = metadata();
        return m[(size_t) juce::jlimit (0, (int) m.size() - 1, index)];
    }

    const juce::StringArray& allTags()
    {
        static juce::StringArray sorted = []
        {
            std::map<juce::String, int> freq;
            for (auto& t : metadata())
                for (auto& s : t) ++freq[s];
            std::vector<std::pair<juce::String, int>> v (freq.begin(), freq.end());
            std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
            juce::StringArray out;
            for (auto& p : v) out.add (p.first);
            return out;
        }();
        return sorted;
    }

    void apply (int index, AcidanceProcessor& p)
    {
        int c, l; locate (index, c, l);
        p.resetAllParams();
        for (int i = 0; i < kNumLfos; ++i)
            for (int s = 0; s < kLfoSteps; ++s) p.lfoCustom[i][s] = (s % 4 == 0) ? 1.f : -0.5f;

        Ctx ctx { &p, std::mt19937 (seedFor (index)), nullptr };
        run (cats[c].gen, ctx, l);

        constexpr int numGains = (int) (sizeof (presetGains) / sizeof (presetGains[0]));
        const float gain = index < numGains ? presetGains[index] : 1.f;
        p.setParam ("volume", juce::jlimit (0.f, 2.f, 0.8f * gain));
    }

    juce::String randomSound (AcidanceProcessor& p)
    {
        auto& sys = juce::Random::getSystemRandom();
        const int c = sys.nextInt (numCats), l = sys.nextInt (cats[c].size);
        p.resetAllParams();
        for (int i = 0; i < kNumLfos; ++i)
            for (int s = 0; s < kLfoSteps; ++s) p.lfoCustom[i][s] = (s % 4 == 0) ? 1.f : -0.5f;

        Ctx ctx { &p, std::mt19937 ((uint32_t) sys.nextInt()), nullptr };
        run (cats[c].gen, ctx, l);

        // every effect gets rolled again, not just the sound: all off, then a random chain
        for (auto* id : { "dist_on", "comp_on", "dly_on", "rev_on", "mfx_on" })
            ctx.set (id, 0.f);
        randomFx (ctx, ctx.chance (0.4f));
        p.setParam ("volume", 0.6f); // unknown loudness, so start a little lower than the factory presets
        return cats[c].name;
    }

    juce::var toJson()
    {
        juce::Array<juce::var> list;
        for (int i = 0; i < count(); ++i)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("index", i);
            o->setProperty ("name", name (i));
            o->setProperty ("folder", category (i));
            juce::Array<juce::var> t;
            for (auto& s : tags (i)) t.add (s);
            o->setProperty ("tags", t);
            list.add (juce::var (o));
        }
        auto* root = new juce::DynamicObject();
        root->setProperty ("product", "SKAZI");
        root->setProperty ("author", "Asher Swissa");
        root->setProperty ("presets", list);
        return juce::var (root);
    }

    //==========================================================================
    juce::File userFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("SKAZI").getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    std::vector<UserPreset> userPresets()
    {
        std::vector<UserPreset> out;
        for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.skazi"))
        {
            UserPreset u { f, f.getFileNameWithoutExtension(), {} };
            if (auto xml = juce::XmlDocument::parse (f))
            {
                u.name = xml->getStringAttribute ("name", u.name);
                u.tags = juce::StringArray::fromTokens (xml->getStringAttribute ("tags"), ",", "");
                u.tags.trim();
                u.tags.removeEmptyStrings();
            }
            out.push_back (u);
        }
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });
        return out;
    }

    bool saveUser (AcidanceProcessor& p, const juce::String& presetName, const juce::StringArray& presetTags)
    {
        const auto clean = juce::File::createLegalFileName (presetName.trim());
        if (clean.isEmpty()) return false;
        juce::XmlElement root ("SKAZI_PRESET");
        root.setAttribute ("name", presetName.trim());
        root.setAttribute ("tags", presetTags.joinIntoString (","));
        root.setAttribute ("author", "User");
        root.addChildElement (p.createStateXml().release());
        const auto file = userFolder().getChildFile (clean + ".skazi");
        if (! root.writeTo (file)) return false;
        p.currentPresetName = presetName.trim();
        p.currentPresetFolder = "User";
        ++p.programChanges;
        return true;
    }

    bool loadUser (AcidanceProcessor& p, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("SKAZI_PRESET")) return false;
        if (auto* state = xml->getChildElement (0))
            p.applyStateXml (*state);
        p.currentPresetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        p.currentPresetFolder = "User";
        ++p.programChanges;
        return true;
    }
}
