#include "Presets.h"
#include "Params.h"
#if __has_include("PresetLevels.h")
 #include "PresetLevels.h" // measured output level per preset, written by BullyTest --levels
#endif

namespace bully
{
namespace
{
    enum Table { TBasic, TSaw, TGrowl, TSub, TPwm, TVocal, TMetal, TSync, TAcid, TOrgan, TSpectral, TFold };
    // note length indices (skazi::noteLengths)
    enum Rate { R1_1, R1_2, R1_4, R1_8, R1_16, R1_32, R1_2T, R1_4T, R1_8T, R1_16T, R1_2D, R1_4D, R1_8D, R1_16D, R2_1, R4_1 };
    constexpr float kSawPos = 66.7f, kSquarePos = 100.0f;

    int oscDest (int o, int d) { return DOsc0 + o * kOscDests + d; }

    // Writes parameter values in their real units
    struct Builder
    {
        juce::Random& r;
        std::map<juce::String, float> v = skazi::defaultValues (paramDefs());
        int slot = 0;

        float rr (float lo, float hi) { return std::round ((lo + r.nextFloat() * (hi - lo)) * 10.0f) / 10.0f; }
        int ri (int lo, int hi) { return lo + r.nextInt (hi - lo + 1); }
        bool chance (float p) { return r.nextFloat() < p; }
        template <typename T> T pick (std::initializer_list<T> l) { return *(l.begin() + r.nextInt ((int) l.size())); }

        void set (const std::string& id, float x) { v[juce::String (id)] = x; }
        static std::string o (int i, const char* what) { return std::string ("o") + oscLetter (i) + what; }

        void osc (int i, int tab, float pos, int oct = 0, int semi = 0, int uni = 1, float det = 25, float lvl = 75)
        {
            set (o (i, "on"), 1); set (o (i, "tab"), (float) tab); set (o (i, "pos"), pos); set (o (i, "oct"), (float) oct);
            set (o (i, "semi"), (float) semi); set (o (i, "uni"), (float) uni); set (o (i, "det"), det); set (o (i, "lvl"), lvl);
        }
        void oscOff (int i) { set (o (i, "on"), 0); }
        void warp (int i, int which, int type, float amt)
        {
            set (o (i, which == 1 ? "w1t" : "w2t"), (float) type);
            set (o (i, which == 1 ? "w1" : "w2"), amt);
        }
        void route (int i, bool f1, bool f2) { set (o (i, "f1"), f1 ? 1.0f : 0.0f); set (o (i, "f2"), f2 ? 1.0f : 0.0f); }
        void phase (int i, float phs, float rnd) { set (o (i, "phs"), phs); set (o (i, "rnd"), rnd); }
        void filt (int f, int type, float cut, float res, float drv = 0, float var = 0, float key = 0, float mix = 100)
        {
            const std::string p = "f" + std::to_string (f);
            set (p + "on", 1); set (p + "type", (float) type); set (p + "cut", cut); set (p + "res", res);
            set (p + "drv", drv); set (p + "var", var); set (p + "key", key); set (p + "mix", mix);
        }
        void env (int e, float a, float d, float s, float rel)
        {
            const std::string p = "e" + std::to_string (e);
            set (p + "a", a); set (p + "d", d); set (p + "s", s); set (p + "r", rel);
        }
        void lfo (int l, int shape, int rate, int mode = LTrig, float smooth = 0)
        {
            const std::string p = "l" + std::to_string (l);
            set (p + "shp", (float) shape); set (p + "sync", 1); set (p + "rate", (float) rate); set (p + "mode", (float) mode); set (p + "smo", smooth);
        }
        void lfoHz (int l, int shape, float hz, int mode = LFree)
        {
            const std::string p = "l" + std::to_string (l);
            set (p + "shp", (float) shape); set (p + "sync", 0); set (p + "hz", hz); set (p + "mode", (float) mode);
        }
        void mod (int src, int dst, float amt)
        {
            if (slot >= kSlots) return;
            const std::string p = "x" + std::to_string (++slot);
            set (p + "src", (float) src); set (p + "dst", (float) dst); set (p + "amt", amt);
        }
        void poly() { set ("vmode", VPoly); set ("glide", 0); }
        void mono (float glide = 0) { set ("vmode", VMono); set ("glide", glide); }
        void legato (float glide) { set ("vmode", VLegato); set ("glide", glide); }

        void dist (int type, float drive, float mix = 100, float tone = 14000) { set ("dson", 1); set ("dstype", (float) type); set ("dsdrv", drive); set ("dsmix", mix); set ("dstone", tone); }
        void ott (float depth, float up = 60, float down = 60) { set ("oton", 1); set ("otdep", depth); set ("otup", up); set ("otdn", down); }
        void hyper (int voices, float det, float mix) { set ("hyon", 1); set ("hyuni", (float) voices); set ("hydet", det); set ("hymix", mix); }
        void modfx (int type, float rate, float depth, float fb, float mix) { set ("moon", 1); set ("motype", (float) type); set ("morate", rate); set ("modep", depth); set ("mofb", fb); set ("momix", mix); }
        void delay (int note, float fb, float mix, bool pp = true, float tone = 65) { set ("dlon", 1); set ("dlsync", 1); set ("dlnote", (float) note); set ("dlfb", fb); set ("dlmix", mix); set ("dlpp", pp ? 1.0f : 0.0f); set ("dltone", tone); }
        void reverb (float size, float decay, float mix, float damp = 40, float pre = 10) { set ("rvon", 1); set ("rvsize", size); set ("rvdec", decay); set ("rvmix", mix); set ("rvdamp", damp); set ("rvpre", pre); }
        void eq (float lo, float midF, float mid, float hi) { set ("eqon", 1); set ("eqlo", lo); set ("eqmidf", midF); set ("eqmid", mid); set ("eqhi", hi); }
        void shift (float hz, float mix) { set ("fson", 1); set ("fsfrq", hz); set ("fsmix", mix); }
        void split (float lo, float hi) { set ("spon", 1); set ("splo", lo); set ("sphi", hi); }
        void arp (int mode, int rate, int oct, float gate, float swing = 0) { set ("arpon", 1); set ("arpmode", (float) mode); set ("arprate", (float) rate); set ("arpoct", (float) oct); set ("arpgate", gate); set ("arpswing", swing); }
    };

    //== recipes, one per style
    void psyBass (Builder& b)
    {
        // clean, phase-locked saw; ENV 2 snaps the pitch for the click, ENV 3 punches the filter
        const bool basic = b.chance (0.5f);
        b.osc (0, basic ? TBasic : TSaw, basic ? kSawPos : b.rr (85, 100), b.pick ({ -1, 0 }), 0, 1, 0, b.rr (70, 85));
        b.phase (0, 0, 0);
        if (b.chance (0.4f)) { b.osc (2, TSub, b.rr (0, 20), -1, 0, 1, 0, b.rr (25, 45)); b.phase (2, 0, 0); b.route (2, false, false); }
        b.mono();
        b.env (1, 0, b.rr (110, 170), 0, b.rr (10, 15));
        b.env (2, 0, b.rr (8, 15), 0, 10);
        b.env (3, 0, b.rr (80, 130), 0, 40);
        b.mod (SEnv1 + 1, oscDest (0, OPitch), b.pick ({ 50.0f, 100.0f }));
        b.filt (1, FLp24, b.rr (90, 220), b.rr (8, 30), b.rr (15, 25));
        b.mod (SEnv1 + 2, DF1Cut, b.rr (30, 52));
        b.set ("f2on", 0);
        b.dist (b.pick ({ 0, 3 }), b.rr (10, 20));
        b.ott (b.rr (25, 45), b.rr (30, 55), b.rr (50, 75));
        b.set ("velo", 20);
        b.set ("drift", 0);  // every hit identical
        b.set ("spread", 0);
        b.set ("vol", -5);
    }

    void rollingBass (Builder& b)
    {
        b.osc (0, b.pick ({ TPwm, TBasic, TAcid }), b.rr (20, 90), -1, 0, 1, 0, 80);
        b.phase (0, 0, 0);
        b.osc (1, TSub, 0, -1, 0, 1, 0, b.rr (30, 50));
        b.route (1, false, false);
        b.legato (b.rr (20, 70));
        b.env (1, 0, b.rr (150, 260), b.rr (0, 30), 20);
        b.env (2, 0, b.rr (60, 160), 0, 40);
        b.filt (1, b.pick ({ FLp24, FDirty, FLp12 }), b.rr (150, 400), b.rr (20, 55), b.rr (10, 40), 0, b.rr (20, 50));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (25, 50));
        b.lfo (1, LSine, R1_16, LTrig);
        b.mod (SLfo1, oscDest (0, OPos), b.rr (10, 30));
        b.dist (b.pick ({ 0, 1, 3 }), b.rr (15, 35));
        if (b.chance (0.5f)) b.ott (b.rr (20, 40));
        b.set ("vol", -5);
    }

    void reese (Builder& b)
    {
        b.osc (0, TSaw, b.rr (70, 100), -1, 0, b.ri (3, 5), b.rr (14, 30), 70);
        b.osc (1, b.pick ({ TSaw, TPwm }), b.rr (40, 100), -1, 0, b.ri (2, 4), b.rr (18, 34), 60);
        b.set ("obfine", b.rr (-12, 12));
        b.mono (b.rr (0, 40));
        b.env (1, b.rr (0, 10), 800, b.rr (80, 100), b.rr (60, 200));
        b.filt (1, FLp24, b.rr (300, 900), b.rr (10, 35), b.rr (10, 35));
        b.lfoHz (1, LSine, b.rr (0.1f, 0.6f));
        b.mod (SLfo1, DF1Cut, b.rr (10, 25));
        b.mod (SLfo1, oscDest (0, OPos), b.rr (-30, -10));
        b.dist (b.pick ({ 0, 1, 4 }), b.rr (20, 45));
        if (b.chance (0.6f)) b.ott (b.rr (30, 55));
        b.set ("vol", -7);
    }

    void growl (Builder& b)
    {
        b.osc (0, b.pick ({ TGrowl, TFold, TSync }), b.rr (10, 60), -1, 0, b.ri (1, 3), b.rr (5, 20), 80);
        b.warp (0, 1, b.pick ({ WFm, WBendP, WMirror, WSync }), b.rr (10, 50));
        b.osc (2, TSub, 0, -2, 0, 1, 0, b.rr (30, 50));
        b.route (2, false, false);
        b.mono();
        b.env (1, 0, 600, 100, 60);
        const int rate = b.pick ({ (int) R1_8, (int) R1_16, (int) R1_8T, (int) R1_4 });
        b.lfo (1, b.pick ({ (int) LTri, (int) LSawDown, (int) LHump, (int) LSine }), rate, LTrig);
        b.mod (SLfo1, oscDest (0, OPos), b.rr (30, 70));
        b.filt (1, b.pick ({ FFormant, FCombP, FDirty, FLp24, FPhaser }), b.rr (400, 2500), b.rr (30, 70), b.rr (10, 40), b.rr (0, 60));
        b.mod (SLfo1, DF1Cut, b.rr (20, 45));
        if (b.chance (0.5f)) b.mod (SLfo1, oscDest (0, OW1), b.rr (20, 50));
        b.dist (b.pick ({ 0, 2, 4, 3 }), b.rr (25, 55));
        b.ott (b.rr (40, 70));
        b.set ("vol", -8);
    }

    void technoBass (Builder& b)
    {
        b.osc (0, b.pick ({ TBasic, TPwm, TSub }), b.pick ({ kSawPos, kSquarePos, 50.0f, 20.0f }), -1, 0, 1, 0, 80);
        b.phase (0, 0, 0);
        b.mono();
        b.env (1, 0, b.rr (180, 400), b.rr (0, 40), b.rr (20, 80));
        b.env (2, 0, b.rr (40, 150), 0, 50);
        b.filt (1, b.pick ({ FLp24, FLp12, FDirty }), b.rr (120, 500), b.rr (25, 65), b.rr (10, 40));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (20, 45));
        b.mod (SVel, DF1Cut, b.rr (5, 15));
        if (b.chance (0.5f)) b.dist (b.pick ({ 0, 1, 3 }), b.rr (10, 30));
        if (b.chance (0.3f)) b.delay (R1_8D, 20, 10);
        b.set ("vol", -5);
    }

    void lead (Builder& b)
    {
        b.osc (0, TSaw, b.rr (80, 100), 0, 0, b.ri (5, 8), b.rr (20, 40), 65);
        b.osc (1, b.pick ({ TSaw, TPwm, TBasic }), b.rr (50, 100), b.pick ({ 0, 1 }), b.pick ({ 0, 7, 12 }), b.ri (3, 7), b.rr (15, 35), 50);
        b.legato (b.rr (20, 90));
        b.env (1, b.rr (2, 15), 600, 90, b.rr (150, 400));
        b.filt (1, FLp24, b.rr (2500, 9000), b.rr (5, 25), 0, b.rr (0, 30), 30);
        b.lfo (1, LSine, R1_8, LFree);
        b.mod (SModWheel, oscDest (0, OPitch), 1.0f);
        b.mod (SModWheel, oscDest (1, OPitch), 1.0f);
        b.hyper (b.ri (3, 6), b.rr (20, 45), b.rr (20, 40));
        b.delay (b.pick ({ (int) R1_8D, (int) R1_4, (int) R1_8 }), b.rr (25, 45), b.rr (15, 30));
        b.reverb (b.rr (50, 80), b.rr (35, 60), b.rr (15, 30));
        b.set ("vol", -9);
    }

    void afroLead (Builder& b)
    {
        // body stays mono and dry, the highs get the delay and the big reverb (3-band splitter)
        b.osc (0, b.pick ({ TVocal, TOrgan, TBasic, TPwm }), b.rr (20, 70), 0, 0, b.ri (1, 3), b.rr (8, 18), 70);
        b.osc (1, b.pick ({ TBasic, TMetal, TOrgan }), b.rr (0, 45), b.pick ({ 0, 1 }), 0, 1, 0, b.rr (25, 45));
        b.poly();
        b.env (1, b.rr (3, 20), b.rr (400, 900), b.rr (40, 70), b.rr (300, 700));
        b.env (2, 0, b.rr (200, 500), 0, 300);
        b.filt (1, b.pick ({ FLp12, FLp24 }), b.rr (1800, 5000), b.rr (10, 30), b.rr (5, 20), 0, 40);
        b.mod (SEnv1 + 1, DF1Cut, b.rr (10, 25));
        b.lfoHz (1, LSine, b.rr (4.5f, 6.0f), LTrig);
        b.mod (SModWheel, oscDest (0, OPitch), 0.8f);
        b.dist (DTube, b.rr (5, 15), 60);
        b.modfx (MChorus, b.rr (0.2f, 0.6f), b.rr (25, 45), 10, b.rr (25, 45));
        b.eq (b.rr (-2, 0), 1000, b.rr (1.5f, 3.5f), b.rr (0, 2));
        b.delay (b.pick ({ (int) R1_8D, (int) R1_4D }), b.rr (30, 45), b.rr (25, 40), true, b.rr (60, 85));
        b.reverb (b.rr (65, 90), b.rr (48, 62), b.rr (30, 40), b.rr (60, 80), 20);
        b.split (300, 2500);
        b.set ("vol", -9);
    }

    void pluck (Builder& b)
    {
        b.osc (0, b.pick ({ TSaw, TBasic, TPwm, TSync, TOrgan }), b.rr (30, 100), 0, 0, b.ri (1, 4), b.rr (10, 25), 70);
        if (b.chance (0.6f)) b.osc (1, b.pick ({ TBasic, TMetal, TVocal }), b.rr (0, 80), b.pick ({ 0, 1 }), b.pick ({ 0, 7, 12 }), 1, 0, 40);
        b.poly();
        b.env (1, 0, b.rr (150, 450), 0, b.rr (120, 300));
        b.env (2, 0, b.rr (60, 220), 0, 100);
        b.filt (1, b.pick ({ FLp24, FLp12, FLp12 }), b.rr (700, 2000), b.rr (15, 45), b.rr (0, 20), 0, b.rr (20, 60));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (30, 50));
        b.mod (SVel, DF1Cut, b.rr (5, 20));
        b.delay (b.pick ({ (int) R1_8D, (int) R1_16, (int) R1_8, (int) R1_4T }), b.rr (25, 50), b.rr (15, 35));
        b.reverb (b.rr (40, 75), b.rr (30, 55), b.rr (15, 30));
        b.set ("vol", -7);
    }

    void pad (Builder& b)
    {
        b.osc (0, b.pick ({ TSaw, TVocal, TOrgan, TSpectral, TBasic }), b.rr (10, 80), 0, 0, b.ri (4, 8), b.rr (20, 45), 55);
        b.osc (1, b.pick ({ TVocal, TSpectral, TMetal, TSaw }), b.rr (10, 80), b.pick ({ 0, 1, -1 }), b.pick ({ 0, 7, 12, 5 }), b.ri (2, 6), b.rr (15, 40), 45);
        b.poly();
        b.env (1, b.rr (300, 1800), 1500, b.rr (70, 100), b.rr (1200, 4000));
        b.filt (1, b.pick ({ FLp12, FLp24, FFormant }), b.rr (900, 5000), b.rr (5, 30), 0, b.rr (0, 50));
        b.lfoHz (1, b.pick ({ (int) LSine, (int) LTri, (int) LSmooth }), b.rr (0.05f, 0.4f));
        b.lfoHz (2, LSmooth, b.rr (0.1f, 0.5f));
        b.mod (SLfo1, oscDest (0, OPos), b.rr (20, 50));
        b.mod (SLfo1 + 1, oscDest (1, OPos), b.rr (20, 50));
        b.mod (SLfo1, DF1Cut, b.rr (5, 15));
        b.modfx (MChorus, b.rr (0.1f, 0.5f), b.rr (30, 60), 10, b.rr (30, 50));
        b.reverb (b.rr (75, 100), b.rr (60, 85), b.rr (30, 50), b.rr (30, 60), 30);
        if (b.chance (0.4f)) b.delay (R1_4D, 35, 15);
        b.set ("vol", -10);
    }

    void keys (Builder& b)
    {
        b.osc (0, b.pick ({ TOrgan, TBasic, TVocal }), b.rr (10, 90), 0, 0, b.ri (1, 2), 10, 70);
        if (b.chance (0.6f)) b.osc (1, TBasic, b.rr (0, 30), 1, 0, 1, 0, 30);
        b.poly();
        b.env (1, b.rr (1, 8), b.rr (600, 1500), b.rr (30, 60), b.rr (200, 500));
        b.filt (1, FLp12, b.rr (1500, 6000), b.rr (5, 20), 0, 0, 50);
        b.mod (SVel, DF1Cut, b.rr (10, 25));
        b.modfx (b.pick ({ (int) MChorus, (int) MPhaser }), b.rr (0.2f, 1.2f), b.rr (20, 50), 20, b.rr (20, 40));
        b.reverb (b.rr (40, 70), b.rr (30, 50), b.rr (15, 25));
        b.set ("vol", -8);
    }

    void bell (Builder& b)
    {
        b.osc (0, b.pick ({ TMetal, TBasic, TSpectral }), b.rr (20, 100), 0, 0, 1, 0, 65);
        b.osc (1, b.pick ({ TBasic, TMetal }), b.rr (0, 40), b.pick ({ 1, 2 }), b.pick ({ 0, 7 }), 1, 0, 50);
        b.warp (0, 1, b.pick ({ WFm, WRing }), b.rr (20, 60));
        b.poly();
        b.env (1, 0, b.rr (1200, 3000), 0, b.rr (1200, 3000));
        b.env (2, 0, b.rr (300, 900), 0, 600);
        b.mod (SEnv1 + 1, oscDest (0, OW1), b.rr (20, 45));
        b.set ("f1on", 0);
        b.delay (b.pick ({ (int) R1_4D, (int) R1_8D }), b.rr (30, 45), b.rr (20, 30));
        b.reverb (b.rr (60, 90), b.rr (55, 75), b.rr (25, 40));
        b.set ("vol", -9);
    }

    void arpPreset (Builder& b)
    {
        pluck (b);
        b.arp (b.pick ({ (int) AUp, (int) AUpDown, (int) ARandom, (int) AAsPlayed, (int) ADown }), b.pick ({ (int) R1_16, (int) R1_8, (int) R1_16T }),
               b.ri (1, 3), b.rr (35, 80), b.chance (0.4f) ? b.rr (10, 35) : 0.0f);
    }

    void stab (Builder& b)
    {
        b.osc (0, TSaw, b.rr (70, 100), 0, 0, b.ri (4, 7), b.rr (20, 35), 60);
        b.osc (1, b.pick ({ TPwm, TBasic, TOrgan }), b.rr (40, 100), 0, b.pick ({ 0, 12 }), b.ri (2, 5), b.rr (15, 30), 50);
        b.poly();
        b.env (1, 0, b.rr (200, 450), b.rr (0, 20), b.rr (150, 400));
        b.env (2, 0, b.rr (150, 300), 0, 200);
        b.filt (1, FLp24, b.rr (700, 2500), b.rr (10, 35), b.rr (0, 20));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (20, 40));
        b.ott (b.rr (25, 45));
        b.reverb (b.rr (60, 85), b.rr (40, 60), b.rr (20, 35));
        if (b.chance (0.5f)) b.delay (R1_8D, 30, 20);
        b.set ("vol", -10);
    }

    void psyLead (Builder& b)
    {
        b.osc (0, b.pick ({ TAcid, TSync, TSaw, TPwm }), b.rr (20, 80), 0, 0, b.ri (1, 3), b.rr (5, 20), 75);
        b.mono (b.rr (20, 80));
        b.env (1, 0, 500, b.rr (60, 100), b.rr (60, 150));
        b.env (2, 0, b.rr (100, 300), 0, 100);
        b.filt (1, b.pick ({ FDirty, FLp24, FCombP, FPhaser }), b.rr (500, 2000), b.rr (50, 85), b.rr (15, 45));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (20, 45));
        b.lfo (1, b.pick ({ (int) LSine, (int) LTri, (int) LSawDown }), b.pick ({ (int) R1_16, (int) R1_8, (int) R1_8T }), LTrig);
        b.mod (SLfo1, oscDest (0, OPos), b.rr (20, 60));
        b.mod (SLfo1, DF1Cut, b.rr (10, 30));
        b.dist (b.pick ({ 0, 3, 4 }), b.rr (15, 35));
        b.delay (b.pick ({ (int) R1_8D, (int) R1_16D, (int) R1_8 }), b.rr (35, 55), b.rr (20, 35));
        b.reverb (b.rr (40, 70), b.rr (35, 55), b.rr (12, 25));
        b.set ("vol", -9);
    }

    void fxRiser (Builder& b)
    {
        b.osc (0, b.pick ({ TSaw, TSpectral, TSync, TFold }), b.rr (0, 100), b.pick ({ 0, 1 }), 0, b.ri (3, 8), b.rr (25, 60), 50);
        b.set ("non", 1); b.set ("ntype", (float) b.pick ({ (int) NWhite, (int) NPink })); b.set ("nlvl", b.rr (30, 60));
        b.poly();
        b.env (1, b.rr (50, 800), 2000, 100, b.rr (1000, 3000));
        b.lfo (1, LSawUp, b.pick ({ (int) R2_1, (int) R4_1, (int) R1_1 }), LEnv);
        b.mod (SLfo1, oscDest (0, OPitch), b.rr (25, 50));
        b.mod (SLfo1, DF1Cut, b.rr (30, 60));
        b.filt (1, b.pick ({ FLp24, FBp12, FPhaser, FCombP }), b.rr (200, 800), b.rr (30, 70));
        b.shift (b.rr (-150, 150), b.rr (20, 50));
        b.delay (R1_8D, 50, 30);
        b.reverb (95, 85, 45);
        b.set ("vol", -11);
    }

    void chaosTexture (Builder& b)
    {
        b.osc (0, b.pick ({ TSpectral, TMetal, TFold, TVocal, TGrowl }), b.rr (0, 100), 0, 0, b.ri (2, 6), b.rr (20, 50), 55);
        b.osc (1, b.pick ({ TSpectral, TMetal, TSync }), b.rr (0, 100), b.pick ({ 0, -1, 1 }), b.pick ({ 0, 7, 5 }), b.ri (1, 4), 20, 45);
        b.poly();
        b.env (1, b.rr (100, 1500), 1500, b.rr (60, 100), b.rr (800, 3000));
        b.set ("c1rate", b.rr (10, 60)); b.set ("c2rate", b.rr (5, 50));
        b.mod (SChaos1, oscDest (0, OPos), b.rr (20, 50));
        b.mod (SChaos1 + 1, DF1Cut, b.rr (15, 35));
        b.mod (SChaos1, oscDest (1, OW1), b.rr (20, 50));
        b.warp (1, 1, b.pick ({ WFm, WBendP, WMirror, WAsym }), b.rr (10, 40));
        b.filt (1, b.pick ({ FFormant, FPhaser, FCombM, FLp12 }), b.rr (500, 3000), b.rr (20, 60), 0, b.rr (0, 80));
        b.modfx (b.pick ({ (int) MFlanger, (int) MPhaser, (int) MChorus }), b.rr (0.05f, 0.5f), b.rr (30, 70), b.rr (20, 60), b.rr (25, 50));
        b.reverb (b.rr (70, 100), b.rr (60, 90), b.rr (30, 50));
        b.set ("vol", -11);
    }

    // BULLY-only signature sounds
    void warpBass (Builder& b)
    {
        b.osc (0, b.pick ({ TSaw, TPwm, TSync, TBasic }), b.rr (30, 100), -1, 0, 1, 0, 80);
        b.phase (0, 0, 0);
        b.warp (0, 1, b.pick ({ WSync, WBendP, WAsym, WMirror }), b.rr (10, 35));
        b.warp (0, 2, b.pick ({ WBendM, WPwm, WFm }), b.rr (5, 25));
        b.osc (2, TSub, 0, -1, 0, 1, 0, b.rr (30, 45));
        b.route (2, false, false);
        b.mono (b.chance (0.4f) ? b.rr (15, 60) : 0.0f);
        b.env (1, 0, b.rr (180, 500), b.rr (20, 70), b.rr (30, 90));
        b.env (2, 0, b.rr (90, 300), 0, 60);
        b.mod (SEnv1 + 1, oscDest (0, OW1), b.rr (30, 60));
        b.filt (1, b.pick ({ FFormant, FCombP, FLp24, FDirty }), b.rr (300, 1400), b.rr (30, 65), b.rr (15, 40), b.rr (20, 80));
        b.mod (SEnv1 + 1, DF1Cut, b.rr (20, 40));
        b.dist (b.pick ({ (int) DTube, (int) DFold, (int) DDiode }), b.rr (20, 45));
        b.ott (b.rr (30, 55));
        b.set ("vol", -6);
    }

    void formantTalk (Builder& b)
    {
        b.osc (0, b.pick ({ TVocal, TSaw, TGrowl }), b.rr (0, 100), b.pick ({ -1, 0 }), 0, b.ri (1, 3), b.rr (5, 15), 80);
        b.warp (0, 1, b.pick ({ WMirror, WBendP, WFm }), b.rr (10, 40));
        b.mono (b.rr (20, 70));
        b.env (1, b.rr (0, 20), 600, b.rr (70, 100), b.rr (80, 250));
        b.filt (1, FFormant, b.rr (400, 3000), b.rr (40, 75), b.rr (10, 30), b.rr (20, 70));
        b.lfo (1, b.pick ({ (int) LTri, (int) LSine, (int) LSmooth, (int) LStairs }), b.pick ({ (int) R1_4, (int) R1_8, (int) R1_2, (int) R1_8D }), LTrig);
        b.mod (SLfo1, DF1Cut, b.rr (30, 60));
        b.mod (SLfo1, oscDest (0, OPos), b.rr (20, 50));
        b.mod (SLfo1, DF1Var, b.rr (10, 30));
        b.dist (DSoft, b.rr (15, 35));
        b.modfx (MPhaser, b.rr (0.1f, 0.5f), b.rr (30, 60), b.rr (20, 50), b.rr (20, 40));
        b.delay (R1_8D, b.rr (25, 40), b.rr (15, 25));
        b.set ("vol", -8);
    }

    void shiftMetal (Builder& b)
    {
        b.osc (0, b.pick ({ TMetal, TSpectral, TSync }), b.rr (10, 90), 0, 0, b.ri (1, 3), b.rr (5, 20), 70);
        b.warp (0, 1, WRing, b.rr (30, 70));
        b.osc (1, b.pick ({ TBasic, TMetal }), b.rr (0, 60), b.pick ({ 1, 0 }), b.pick ({ 7, 0, 5 }), 1, 0, 55);
        b.poly();
        b.env (1, 0, b.rr (300, 1500), b.rr (0, 40), b.rr (400, 1500));
        b.filt (1, b.pick ({ FCombP, FCombM, FPhaser, FRing }), b.rr (200, 2500), b.rr (40, 80), b.rr (0, 25), b.rr (20, 80));
        b.lfoHz (1, LSmooth, b.rr (0.1f, 1.5f));
        b.mod (SLfo1, DF1Cut, b.rr (15, 35));
        b.shift (b.rr (-120, 120), b.rr (30, 55));
        b.delay (b.pick ({ (int) R1_8D, (int) R1_4T }), b.rr (35, 55), b.rr (20, 30));
        b.reverb (b.rr (60, 90), b.rr (50, 75), b.rr (25, 40));
        b.set ("vol", -9);
    }

    // What makes BULLY sound like BULLY, layered on every factory preset:
    // moving warps and wavetables, chaos drift, macros pre-mapped, and a full FX chain for the style.
    enum Kind { KBass, KTightBass, KLead, KPluck, KPad, KFx };
    bool routed (Builder& b, int src, int dst)
    {
        for (int s = 1; s <= b.slot; ++s)
            if ((int) b.v[juce::String ("x" + std::to_string (s) + "src")] == src && (int) b.v[juce::String ("x" + std::to_string (s) + "dst")] == dst) return true;
        return false;
    }
    bool isOn (Builder& b, const char* id) { return b.v[juce::String (id)] > 0.5f; }

    void bullyfy (Builder& b, Kind kind)
    {
        const bool bass = kind == KBass || kind == KTightBass;
        // 0. analog character per style (drift, saturation, boost, stereo spread)
        switch (kind)
        {
            case KTightBass: b.set ("drift", 0); b.set ("spread", 0); b.set ("sat", b.rr (25, 40)); b.set ("boost", b.rr (20, 35)); break;
            case KBass:      b.set ("drift", b.rr (15, 30)); b.set ("spread", b.rr (0, 10)); b.set ("sat", b.rr (35, 60)); b.set ("boost", b.rr (25, 45)); break;
            case KLead:      b.set ("drift", b.rr (25, 50)); b.set ("spread", b.rr (40, 80)); b.set ("sat", b.rr (25, 50)); b.set ("boost", b.rr (10, 30)); break;
            case KPluck:     b.set ("drift", b.rr (20, 40)); b.set ("spread", b.rr (40, 75)); b.set ("sat", b.rr (20, 45)); b.set ("boost", b.rr (10, 30)); break;
            case KPad:       b.set ("drift", b.rr (35, 65)); b.set ("spread", b.rr (60, 100)); b.set ("sat", b.rr (10, 30)); b.set ("boost", b.rr (10, 25)); break;
            case KFx:        b.set ("drift", b.rr (20, 80)); b.set ("spread", b.rr (30, 100)); b.set ("sat", b.rr (20, 70)); b.set ("boost", b.rr (0, 30)); break;
        }
        // metallic edge: a second FM / ring warp, or a note-tracked comb after the filter
        if (! bass)
        {
            if ((int) b.v["oaw2t"] == WNone && b.chance (0.5f)) b.warp (0, 2, b.pick ({ (int) WFm, (int) WRing }), b.rr (8, 25));
            if (! isOn (b, "f2on") && b.chance (0.4f))
            {
                b.filt (2, b.pick ({ FCombP, FCombM }), b.rr (300, 1500), b.rr (40, 70), 0, b.rr (20, 60), 100, b.rr (20, 40));
                b.set ("froute", 0);
            }
        }
        // 1. warps that move
        if ((int) b.v["oaw1t"] == WNone)
        {
            const int w = bass ? b.pick ({ (int) WSync, (int) WBendP, (int) WAsym, (int) WFm })
                               : b.pick ({ (int) WSync, (int) WMirror, (int) WBendM, (int) WPwm, (int) WFm, (int) WRing });
            b.warp (0, 1, w, b.rr (8, kind == KTightBass ? 20.0f : 40.0f));
        }
        if (! routed (b, SEnv1 + 1, oscDest (0, OW1)) && ! routed (b, SLfo1, oscDest (0, OW1)))
            b.mod (kind == KPad || kind == KFx ? SLfo1 + 2 : SEnv1 + 1, oscDest (0, OW1), b.rr (15, 40));
        if (isOn (b, "obon") && (int) b.v["obw1t"] == WNone)
            b.warp (1, 1, b.pick ({ (int) WMirror, (int) WBendP, (int) WFm, (int) WQuant, (int) WSync }), b.rr (5, 35));

        // 2. the wavetable moves
        if (! routed (b, SLfo1, oscDest (0, OPos)) && ! routed (b, SLfo1 + 2, oscDest (0, OPos)))
        {
            if (kind == KPad || kind == KFx) b.lfoHz (3, b.pick ({ (int) LSine, (int) LSmooth, (int) LTri }), b.rr (0.05f, 0.4f));
            else b.lfo (3, b.pick ({ (int) LSine, (int) LTri, (int) LSawDown, (int) LHump }), b.pick ({ (int) R1_4, (int) R1_8, (int) R1_2, (int) R1_1 }), bass ? LTrig : LFree);
            b.mod (SLfo1 + 2, oscDest (0, OPos), b.rr (kind == KTightBass ? 8.0f : 15.0f, kind == KTightBass ? 20.0f : 40.0f));
        }

        // 3. chaos drift (not on the phase-locked psy basses)
        if (kind != KTightBass)
        {
            b.set ("c1rate", b.rr (5, 25)); b.set ("c1smo", b.rr (40, 80));
            b.mod (SChaos1, oscDest (0, OPitch), b.rr (0.3f, 0.7f));
            if (isOn (b, "obon")) b.mod (SChaos1 + 1, oscDest (1, OPitch), b.rr (0.3f, 0.7f));
            b.mod (SChaos1 + 1, DF1Cut, b.rr (3, 8));
        }

        // 4. macros: M1 brightness, M2 warp, M3 space, M4 dirt
        b.mod (SMacro1, DF1Cut, 35);
        b.mod (SMacro1 + 1, oscDest (0, OW1), 45);
        b.mod (SMacro1 + 2, DRvMix, 40);
        b.mod (SMacro1 + 2, DDlMix, 30);
        b.mod (SMacro1 + 3, DDsDrv, 45);

        // 5. full FX chain for the style (only adds what the recipe left off)
        auto on = [&] (const char* id) { return isOn (b, id); };
        switch (kind)
        {
            case KTightBass:
            case KBass:
                if (! on ("dson")) b.dist (b.pick ({ (int) DTube, (int) DDiode, (int) DSoft }), b.rr (10, 25));
                if (! on ("oton")) b.ott (b.rr (20, 40));
                if (! on ("eqon")) b.eq (b.rr (1, 3), b.rr (600, 1500), b.rr (-2, 1), b.rr (1, 3));
                if (kind == KBass && ! on ("moon") && b.chance (0.5f)) b.modfx (b.pick ({ (int) MPhaser, (int) MFlanger }), b.rr (0.1f, 0.6f), b.rr (20, 50), b.rr (10, 40), b.rr (12, 25));
                if (! on ("spon")) b.split (b.rr (120, 220), b.rr (2500, 5000)); // keeps the low end mono
                if (! on ("dlon") && kind == KBass && b.chance (0.4f)) b.delay (b.pick ({ (int) R1_8D, (int) R1_16D }), b.rr (15, 30), b.rr (8, 15));
                break;
            case KLead:
                if (! on ("hyon")) b.hyper (b.ri (3, 6), b.rr (15, 40), b.rr (15, 35));
                if (! on ("dson")) b.dist (b.pick ({ (int) DTube, (int) DSoft }), b.rr (8, 20), b.rr (50, 90));
                if (! on ("moon")) b.modfx (b.pick ({ (int) MChorus, (int) MFlanger, (int) MPhaser }), b.rr (0.1f, 0.8f), b.rr (20, 50), b.rr (10, 40), b.rr (15, 35));
                if (! on ("fson") && b.chance (0.35f)) b.shift (b.rr (-8, 8), b.rr (15, 30));
                if (! on ("oton")) b.ott (b.rr (15, 30));
                if (! on ("dlon")) b.delay (b.pick ({ (int) R1_8D, (int) R1_4D, (int) R1_8 }), b.rr (25, 45), b.rr (15, 30));
                if (! on ("rvon")) b.reverb (b.rr (50, 85), b.rr (40, 65), b.rr (15, 30));
                break;
            case KPluck:
                if (! on ("dson")) b.dist (b.pick ({ (int) DTube, (int) DSoft, (int) DCrush }), b.rr (8, 22), b.rr (40, 80));
                if (! on ("oton")) b.ott (b.rr (20, 40));
                if (! on ("moon") && b.chance (0.6f)) b.modfx (b.pick ({ (int) MChorus, (int) MPhaser }), b.rr (0.2f, 1.0f), b.rr (20, 45), b.rr (10, 30), b.rr (15, 30));
                if (! on ("eqon")) b.eq (b.rr (-3, 0), b.rr (1500, 4000), b.rr (0, 3), b.rr (1, 4));
                if (! on ("dlon")) b.delay (b.pick ({ (int) R1_8D, (int) R1_16, (int) R1_4T }), b.rr (25, 45), b.rr (15, 30));
                if (! on ("rvon")) b.reverb (b.rr (40, 75), b.rr (30, 55), b.rr (15, 30));
                break;
            case KPad:
                if (! on ("hyon")) b.hyper (b.ri (4, 7), b.rr (25, 55), b.rr (25, 45));
                if (! on ("moon")) b.modfx (MChorus, b.rr (0.1f, 0.4f), b.rr (30, 60), 10, b.rr (25, 45));
                if (! on ("fson")) b.shift (b.rr (-6, 6), b.rr (15, 30));
                if (! on ("eqon")) b.eq (b.rr (-4, -1), b.rr (400, 900), b.rr (-3, 0), b.rr (1, 4));
                if (! on ("dlon")) b.delay (b.pick ({ (int) R1_4D, (int) R1_2 }), b.rr (30, 50), b.rr (15, 25));
                if (! on ("rvon")) b.reverb (b.rr (75, 100), b.rr (60, 90), b.rr (30, 50));
                break;
            case KFx:
                if (! on ("dson")) b.dist (b.pick ({ (int) DFold, (int) DCrush, (int) DDownsample, (int) DRectify }), b.rr (15, 40), b.rr (30, 70));
                if (! on ("moon")) b.modfx (b.pick ({ (int) MFlanger, (int) MPhaser }), b.rr (0.05f, 1.5f), b.rr (30, 70), b.rr (30, 70), b.rr (25, 50));
                if (! on ("fson")) b.shift (b.rr (-200, 200), b.rr (20, 45));
                if (! on ("dlon")) b.delay (b.pick ({ (int) R1_8D, (int) R1_4T, (int) R1_4D }), b.rr (40, 60), b.rr (20, 35));
                if (! on ("rvon")) b.reverb (b.rr (80, 100), b.rr (70, 95), b.rr (30, 50));
                break;
        }
        // wetter than the other SKAZI synths
        if (! bass)
        {
            if (on ("rvon")) b.set ("rvmix", juce::jmin (55.0f, b.v["rvmix"] * 1.35f));
            if (on ("dlon")) b.set ("dlmix", juce::jmin (45.0f, b.v["dlmix"] * 1.35f));
        }
    }

    using Recipe = void (*) (Builder&);
    struct Style { const char* name; Recipe fn; int count; float trimDb; Kind kind; }; // trim: rough level match (PresetLevels.h refines it)
}

const std::vector<skazi::Preset>& presets()
{
    static const std::vector<skazi::Preset> list = []
    {
        std::vector<skazi::Preset> out;
        juce::Random dummy (1);
        Builder init { dummy };
        for (auto& [id, val] : initPatch()) init.v[id] = val;
        out.push_back ({ "Init", "FACTORY", init.v });
        {
            juce::Random r (777);
            Builder b { r };
            psyBass (b);
            b.v["vol"] += 6.0f;
            out.push_back ({ "Bully Roller", "FACTORY", b.v });
        }
        static const Style styles[] = {
            { "PSY BASS", psyBass, 40, 6, KTightBass },   { "ROLLING BASS", rollingBass, 25, 4, KTightBass }, { "WARP BASS", warpBass, 25, 4, KBass },
            { "REESE", reese, 25, 2, KBass },             { "GROWL", growl, 30, -1, KBass },                  { "TECHNO BASS", technoBass, 25, 4, KBass },
            { "LEAD", lead, 30, 10, KLead },              { "AFRO LEAD", afroLead, 25, 3, KLead },            { "PSY LEAD", psyLead, 25, 5, KLead },
            { "FORMANT TALK", formantTalk, 20, 5, KLead },{ "PLUCK", pluck, 30, 6, KPluck },                  { "ARP", arpPreset, 25, 7, KPluck },
            { "PAD", pad, 30, 4, KPad },                  { "KEYS", keys, 20, 6, KPluck },                    { "BELL", bell, 15, 8, KPluck },
            { "STAB", stab, 20, 6, KPluck },              { "SHIFT METAL", shiftMetal, 15, 6, KFx },          { "FX RISER", fxRiser, 15, 6, KFx },
            { "CHAOS", chaosTexture, 20, 6, KFx },
        };
        int g = 0;
        for (int s = 0; s < (int) std::size (styles); ++s)
            for (int i = 0; i < styles[s].count; ++i)
            {
                juce::Random r (1000 + s * 997 + i * 31);
                Builder b { r };
                styles[s].fn (b);
                bullyfy (b, styles[s].kind);
                b.v["vol"] =juce::jmin (6.0f, b.v["vol"] + styles[s].trimDb);
                out.push_back ({ skazi::presetName (g++ * 7 % 1600), styles[s].name, b.v });
            }
#ifdef BULLY_HAS_PRESET_LEVELS
        if (std::size (kPresetVolume) == out.size())
            for (size_t i = 0; i < out.size(); ++i) out[i].values["vol"] = kPresetVolume[i];
#endif
        return out;
    }();
    return list;
}

skazi::Preset randomPreset (juce::Random& rng)
{
    Builder b { rng };
    const int kind = rng.nextInt (4); // 0 bass, 1 lead, 2 pluck, 3 pad: keeps the envelopes playable
    for (int o = 0; o < kOsc; ++o)
    {
        if (o > 0 && ! b.chance (o == 1 ? 0.75f : 0.5f)) { b.oscOff (o); continue; }
        const int oct = kind == 0 ? b.pick ({ -2, -1, -1, 0 }) : b.pick ({ -1, 0, 0, 0, 1 });
        b.osc (o, rng.nextInt (kTables), b.rr (0, 100), oct, b.chance (0.25f) ? b.pick ({ 7, 12, -5, 5, -12 }) : 0,
               kind == 0 ? b.ri (1, 2) : b.ri (1, 8), b.rr (5, 50), b.rr (45, 85));
        if (b.chance (0.6f)) b.warp (o, 1, 1 + rng.nextInt (NumWarps - 1), b.rr (5, 70));
        if (b.chance (0.35f)) b.warp (o, 2, 1 + rng.nextInt (NumWarps - 1), b.rr (5, 60));
        b.set (Builder::o (o, "fine"), b.chance (0.3f) ? b.rr (-15, 15) : 0.0f);
        b.set (Builder::o (o, "bld"), b.rr (30, 100));
        b.set (Builder::o (o, "pan"), b.chance (0.3f) ? b.rr (-40, 40) : 0.0f);
        b.phase (o, kind == 0 ? 0.0f : b.rr (0, 100), kind == 0 ? 0.0f : b.rr (0, 100));
        b.route (o, b.chance (0.85f), b.chance (0.4f));
    }
    if (b.chance (0.3f)) { b.set ("non", 1); b.set ("ntype", (float) rng.nextInt (6)); b.set ("ntone", b.rr (20, 90)); b.set ("nlvl", b.rr (5, 35)); }

    b.filt (1, rng.nextInt (NumFilterTypes), kind == 0 ? b.rr (120, 1500) : b.rr (400, 9000), b.rr (0, 70), b.rr (0, 40), b.rr (0, 100), b.rr (0, 60), b.rr (60, 100));
    if (b.chance (0.5f)) b.filt (2, rng.nextInt (NumFilterTypes), b.rr (80, 8000), b.rr (0, 60), b.rr (0, 30), b.rr (0, 100), 0, b.rr (40, 100));
    else b.set ("f2on", 0);
    b.set ("froute", b.chance (0.3f) ? 1.0f : 0.0f);

    switch (kind)
    {
        case 0:  b.env (1, 0, b.rr (120, 600), b.rr (0, 80), b.rr (10, 120)); b.mono (b.chance (0.4f) ? b.rr (10, 80) : 0.0f); break;
        case 1:  b.env (1, b.rr (0, 30), b.rr (300, 1200), b.rr (60, 100), b.rr (100, 600)); b.legato (b.rr (0, 80)); break;
        case 2:  b.env (1, 0, b.rr (120, 600), b.rr (0, 20), b.rr (100, 500)); b.poly(); break;
        default: b.env (1, b.rr (150, 2000), 1500, b.rr (60, 100), b.rr (800, 4000)); b.poly(); break;
    }
    for (int e = 2; e <= kEnv; ++e) b.env (e, b.rr (0, 200), b.rr (30, 1500), b.rr (0, 60), b.rr (30, 1500));
    for (int l = 1; l <= kLfo; ++l)
    {
        if (b.chance (0.5f)) b.lfo (l, rng.nextInt (NumLfoShapes), rng.nextInt (16), rng.nextInt (3), b.chance (0.3f) ? b.rr (5, 50) : 0.0f);
        else b.lfoHz (l, rng.nextInt (NumLfoShapes), b.rr (0.05f, 8.0f), rng.nextInt (3));
    }
    for (int c = 1; c <= kChaos; ++c) { b.set ("c" + std::to_string (c) + "type", (float) rng.nextInt (3)); b.set ("c" + std::to_string (c) + "rate", b.rr (5, 70)); b.set ("c" + std::to_string (c) + "smo", b.rr (0, 60)); }
    for (int m = 1; m <= kMacro; ++m) b.set ("m" + std::to_string (m), b.rr (0, 100));

    // matrix: a handful of random routings (envelope to cutoff for bass and plucks)
    if (kind == 0 || kind == 2) b.mod (SEnv1 + 1, DF1Cut, b.rr (20, 55));
    const int routings = b.ri (3, 8);
    static const int sources[] = { SEnv1 + 1, SEnv1 + 2, SEnv1 + 3, SLfo1, SLfo1 + 1, SLfo1 + 2, SLfo1 + 3, SLfo1 + 4, SLfo1 + 5,
                                   SChaos1, SChaos1 + 1, SMacro1, SMacro1 + 1, SVel, SModWheel, SRand };
    for (int k = 0; k < routings; ++k)
    {
        const int dst = 1 + rng.nextInt (NumDests - 1);
        const bool pitch = dst >= DOsc0 && dst < DNTone && (dst - DOsc0) % kOscDests == OPitch;
        const float amt = pitch ? b.pick ({ 1.0f, 2.1f, -2.1f, 4.2f, 50.0f }) : b.rr (-55, 60);
        b.mod (sources[rng.nextInt ((int) std::size (sources))], dst, amt);
    }

    // FX: each module rolls on/off (at least two on), with random settings
    int on = 0;
    auto roll = [&] (float p) { const bool x = b.chance (p); on += x ? 1 : 0; return x; };
    for (int attempt = 0; attempt < 3 && on < 2; ++attempt)
    {
        if (roll (0.3f)) b.hyper (b.ri (2, 7), b.rr (10, 60), b.rr (15, 50));
        if (roll (0.45f)) b.dist (rng.nextInt (8), b.rr (5, 50), b.rr (40, 100), b.rr (3000, 18000));
        if (roll (0.4f)) b.modfx (rng.nextInt (3), b.rr (0.05f, 3.0f), b.rr (15, 70), b.rr (0, 70), b.rr (15, 50));
        if (roll (0.15f)) b.shift (b.rr (-300, 300), b.rr (10, 45));
        if (roll (0.35f)) b.ott (b.rr (15, 55));
        if (roll (0.3f)) b.eq (b.rr (-6, 4), b.rr (300, 4000), b.rr (-6, 6), b.rr (-6, 5));
        if (roll (0.5f)) b.delay (rng.nextInt (16) % 14, b.rr (15, 60), b.rr (10, 35), b.chance (0.6f), b.rr (30, 90));
        if (roll (0.55f)) b.reverb (b.rr (30, 100), b.rr (20, 85), b.rr (10, 40), b.rr (10, 80), b.rr (0, 60));
    }
    if (b.chance (0.2f)) b.split (b.rr (150, 500), b.rr (1500, 5000));
    if (b.chance (0.12f) && kind != 3) b.arp (rng.nextInt (6), b.pick ({ (int) R1_16, (int) R1_8, (int) R1_16T }), b.ri (1, 3), b.rr (30, 80));
    else b.set ("arpon", 0);
    b.set ("drift", b.rr (0, 70));
    b.set ("sat", b.rr (10, 65));
    b.set ("boost", b.rr (0, 45));
    b.set ("spread", kind == 0 ? b.rr (0, 15) : b.rr (20, 100));
    b.set ("vol", kind == 3 ? -11.0f : -8.0f);
    return { "RANDOM", "RANDOM", b.v };
}
} // namespace bully
