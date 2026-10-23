#include "Presets.h"
#include "Params.h"

namespace subich
{
namespace
{
    using Values = std::map<juce::String, float>;

    // Note lengths (index into the LFO rate list): 0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/16, 5 1/32, 6 1/2T, 7 1/4T,
    // 8 1/8T, 9 1/16T, 10 1/2D, 11 1/4D, 12 1/8D, 13 1/16D, 14 2/1, 15 4/1
    struct Gen
    {
        juce::Random& r;
        Values& v;
        float rr (float lo, float hi) { return std::round ((lo + r.nextFloat() * (hi - lo)) * 10.0f) / 10.0f; }
        int pick (std::initializer_list<int> l) { return *(l.begin() + r.nextInt ((int) l.size())); }
        bool chance (float p) { return r.nextFloat() < p; }
        void set (const char* id, float x) { v[id] = x; }

        void amp (float a0, float a1, float d0, float d1, float s0, float s1, float r0, float r1)
        { set ("aa", rr (a0, a1)); set ("ad", rr (d0, d1)); set ("as", rr (s0, s1)); set ("ar", rr (r0, r1)); }
        void fenv (float a0, float a1, float d0, float d1, float s0, float s1, float r0, float r1)
        { set ("fa", rr (a0, a1)); set ("fd", rr (d0, d1)); set ("fs", rr (s0, s1)); set ("fr", rr (r0, r1)); }
        void f1 (int type, float c0, float c1, float res0, float res1, float e0, float e1, float dr0 = 0, float dr1 = 30)
        { set ("f1type", (float) type); set ("f1cut", rr (c0, c1)); set ("f1res", rr (res0, res1)); set ("f1env", rr (e0, e1)); set ("f1drive", rr (dr0, dr1)); set ("f1key", rr (30, 70)); }
        void osc1 (int wave, int oct, float shape0 = 0, float shape1 = 30, float lvl = 80)
        { set ("o1wave", (float) wave); set ("o1oct", (float) oct); set ("o1shape", rr (shape0, shape1)); set ("o1level", lvl); }
        void osc2 (int wave, int oct, float det0, float det1, float lvl0, float lvl1, int semi = 0)
        { set ("o2wave", (float) wave); set ("o2oct", (float) oct); set ("o2semi", (float) semi); set ("o2detune", rr (det0, det1)); set ("o2level", rr (lvl0, lvl1)); set ("o2shape", rr (0, 60)); }
        void sub (float l0, float l1) { set ("sublevel", rr (l0, l1)); set ("subwave", chance (0.8f) ? 0.0f : 1.0f); }
        void lfo (int n, int shape, int rate, float d0, float d1, int dest, bool sync = true, float hz = 1.0f)
        {
            const juce::String p = "l" + juce::String (n);
            v[p + "shape"] = (float) shape; v[p + "sync"] = sync ? 1.0f : 0.0f; v[p + "rate"] = (float) rate;
            v[p + "hz"] = hz; v[p + "depth"] = rr (d0, d1); v[p + "dest"] = (float) dest;
        }
        void hp (float c0 = 28, float c1 = 45) { set ("f2type", HP12); set ("f2cut", rr (c0, c1)); set ("f2res", rr (5, 25)); }
    };

    using Style = std::function<void (Gen&)>;

    struct Category { const char* name; int count; Style style; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c = {
            { "PSYTRANCE", 160, [] (Gen& g)
              {
                  switch (g.r.nextInt (5))
                  {
                      // Psy lines: square / pulse waves with glide between the rolling notes
                      case 0: // full-on rolling
                          g.osc1 (g.pick ({ Square, Square, Pulse, Saw }), -1, 0, 45); g.osc2 (g.pick ({ Square, Saw }), -1, -12, 12, 0, 35); g.sub (30, 60);
                          g.f1 (LP24, 140, 380, 15, 50, 55, 90, 15, 45); g.fenv (0, 1, 50, 140, 0, 8, 20, 60);
                          g.amp (0, 1, 80, 170, 0, 25, 15, 50); g.set ("drive", g.rr (25, 55));
                          g.set ("mode", Legato); g.set ("glide", g.rr (20, 70)); break;
                      case 1: // progressive
                          g.osc1 (g.pick ({ Square, Pulse, Tri }), -1, 0, 50); g.osc2 (g.pick ({ Sine, Square }), -1, -5, 5, 20, 50); g.sub (40, 70);
                          g.f1 (g.pick ({ LP24, LP12 }), 200, 600, 10, 35, 30, 60); g.fenv (0, 2, 120, 300, 10, 40, 40, 120);
                          g.amp (0, 2, 150, 320, 20, 55, 30, 90); g.set ("drive", g.rr (10, 35));
                          g.set ("mode", Legato); g.set ("glide", g.rr (35, 110)); break;
                      case 2: // dark / forest
                          g.osc1 (g.pick ({ FM, Square, Pulse }), -1, 20, 70); g.osc2 (g.pick ({ Saw, Square }), -1, -20, 20, 10, 40); g.sub (30, 55);
                          g.f1 (g.pick ({ Acid, LP24 }), 180, 450, 45, 80, 50, 85, 20, 60); g.set ("noise", g.rr (3, 15));
                          g.fenv (0, 1, 40, 110, 0, 10, 20, 50); g.amp (0, 1, 70, 150, 0, 20, 15, 45);
                          g.lfo (1, LSH, g.pick ({ 4, 5, 9 }), 10, 35, DCut1); g.set ("drive", g.rr (35, 70));
                          g.set ("mode", Legato); g.set ("glide", g.rr (15, 45)); break;
                      case 3: // goa acid
                          g.osc1 (g.pick ({ Square, Square, Saw }), -1); g.sub (25, 50);
                          g.f1 (Acid, 200, 600, 60, 88, 55, 90, 20, 50); g.fenv (0, 1, 80, 220, 0, 15, 30, 80);
                          g.amp (0, 1, 120, 260, 30, 70, 20, 60); g.set ("mode", Legato); g.set ("glide", g.rr (20, 80)); break;
                      default: // twilight
                          g.osc1 (Square, -1, 10, 50); g.osc2 (Pulse, 0, -8, 8, 10, 30); g.sub (35, 60);
                          g.f1 (LP12, 300, 800, 25, 55, 35, 60); g.fenv (0, 1, 80, 200, 5, 25, 30, 80);
                          g.amp (0, 1, 100, 220, 10, 40, 20, 60); g.lfo (2, LSine, 0, 2, 6, DPitch, false, g.rr (3, 6));
                          g.set ("mode", Legato); g.set ("glide", g.rr (40, 120)); break;
                  }
                  g.hp (30, 45);
              } },
            { "HOUSE", 140, [] (Gen& g)
              {
                  switch (g.r.nextInt (4))
                  {
                      case 0: // deep
                          g.osc1 (g.pick ({ Sine, Tri }), -1, 0, 40); g.osc2 (Tri, 0, -5, 5, 10, 30); g.sub (50, 80);
                          g.f1 (LP12, 300, 900, 5, 25, 10, 35); g.fenv (0, 5, 150, 400, 30, 70, 50, 150);
                          g.amp (0, 5, 200, 500, 50, 95, 50, 150); break;
                      case 1: // tech house
                          g.osc1 (g.pick ({ Saw, Square }), -1); g.sub (40, 70);
                          g.f1 (LP24, 200, 500, 20, 50, 40, 70, 10, 40); g.fenv (0, 1, 100, 250, 0, 20, 30, 80);
                          g.amp (0, 1, 120, 280, 10, 50, 30, 80); g.set ("drive", g.rr (20, 50)); break;
                      case 2: // organ (M1 style)
                          g.osc1 (FM, -1, 30, 60); g.osc2 (Sine, 0, -3, 3, 30, 60); g.sub (40, 70);
                          g.f1 (LP12, 800, 2500, 5, 20, 10, 30); g.fenv (0, 1, 150, 350, 40, 80, 50, 120);
                          g.amp (0, 2, 200, 400, 60, 100, 40, 100); break;
                      default: // funky
                          g.osc1 (g.pick ({ Square, Pulse }), -1, 10, 50); g.sub (30, 60);
                          g.f1 (LP24, 250, 700, 25, 55, 40, 75); g.fenv (0, 1, 70, 180, 0, 15, 20, 60);
                          g.amp (0, 1, 90, 200, 10, 40, 20, 60); g.set ("mode", Legato); g.set ("glide", g.rr (10, 40)); break;
                  }
                  g.hp (25, 40);
              } },
            { "INDIE DANCE", 120, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Saw, Square, Saw }), -1, 0, 30);
                  g.osc2 (g.pick ({ Saw, Square }), g.chance (0.3f) ? 0 : -1, 5, 14, 40, 75);
                  g.sub (20, 50);
                  g.f1 (g.pick ({ LP24, LP24, LP12 }), 300, 1200, 20, 50, 30, 70, 15, 45);
                  g.fenv (0, 3, 120, 350, 10, 45, 40, 140); g.amp (0, 3, 150, 400, 40, 85, 40, 140);
                  g.set ("mode", g.pick ({ Mono, Legato })); g.set ("glide", g.rr (15, 90)); g.set ("drive", g.rr (20, 50));
                  if (g.chance (0.3f)) g.lfo (1, LTri, g.pick ({ 0, 14 }), 5, 20, DCut1);
                  g.hp (30, 45);
              } },
            { "TECHNO", 140, [] (Gen& g)
              {
                  switch (g.r.nextInt (4))
                  {
                      case 0: // rumble
                          g.osc1 (Sine, -1, 20, 60); g.osc2 (Tri, -1, -4, 4, 20, 40); g.sub (60, 90);
                          g.f1 (LP12, 100, 220, 5, 25, 5, 25, 30, 60); g.fenv (0, 5, 200, 600, 30, 70, 100, 400);
                          g.amp (0, 10, 300, 900, 40, 80, 200, 600); g.set ("drive", g.rr (35, 65));
                          g.lfo (1, LSine, g.pick ({ 3, 4 }), 5, 20, DVolume); break;
                      case 1: // acid
                          g.osc1 (g.pick ({ Saw, Square }), -1); g.sub (20, 45);
                          g.f1 (Acid, 180, 500, 60, 90, 50, 90, 20, 60); g.fenv (0, 1, 80, 250, 0, 15, 30, 80);
                          g.amp (0, 1, 120, 300, 30, 70, 30, 80); g.set ("mode", Legato); g.set ("glide", g.rr (30, 70)); break;
                      case 2: // peak time
                          g.osc1 (Saw, -1, 0, 40); g.osc2 (Saw, -1, -10, 10, 20, 50); g.sub (40, 70);
                          g.f1 (LP24, 160, 420, 25, 55, 45, 80, 25, 55); g.fenv (0, 1, 60, 160, 0, 10, 20, 60);
                          g.amp (0, 1, 80, 200, 0, 30, 20, 60); g.set ("drive", g.rr (35, 70)); break;
                      default: // minimal
                          g.osc1 (g.pick ({ Sine, Tri, Pulse }), -1, 0, 50); g.sub (30, 60);
                          g.f1 (LP12, 250, 900, 10, 40, 20, 55); g.fenv (0, 1, 40, 120, 0, 10, 15, 40);
                          g.amp (0, 1, 50, 150, 0, 20, 15, 40); break;
                  }
                  g.hp (30, 45);
              } },
            { "TRANCE", 60, [] (Gen& g)
              {
                  g.osc1 (Saw, -1); g.osc2 (Saw, -1, 6, 15, 40, 70); g.sub (30, 55);
                  g.f1 (LP24, 400, 1500, 15, 40, 30, 60, 10, 35); g.fenv (0, 1, 60, 180, 0, 20, 20, 70);
                  g.amp (0, 1, 80, 200, 0, 40, 20, 60); g.hp (35, 50);
              } },
            { "MELODIC TECHNO", 60, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Saw, Square }), -1, 0, 40); g.osc2 (Saw, 0, -8, 8, 20, 50); g.sub (35, 65);
                  g.f1 (LP24, 250, 800, 20, 50, 25, 55, 10, 40); g.fenv (0, 10, 200, 600, 20, 50, 100, 300);
                  g.amp (0, 10, 300, 700, 50, 90, 100, 300); g.lfo (1, LSine, g.pick ({ 0, 14, 15 }), 10, 30, DCut1);
                  g.set ("mode", Legato); g.set ("glide", g.rr (20, 120)); g.hp (30, 45);
              } },
            { "DRUM & BASS", 50, [] (Gen& g)
              {
                  g.osc1 (Saw, -1); g.osc2 (Saw, -1, 15, 40, 60, 90); g.sub (40, 70);
                  g.f1 (LP24, 300, 900, 15, 45, 10, 40, 30, 60); g.fenv (0, 20, 300, 900, 40, 80, 100, 300);
                  g.amp (0, 5, 400, 900, 70, 100, 80, 250); g.lfo (2, LSine, 0, 10, 30, DCut1, false, g.rr (0.2f, 1.0f));
                  g.set ("drive", g.rr (40, 75)); g.set ("mode", Legato); g.set ("glide", g.rr (10, 60)); g.hp (25, 40);
              } },
            { "DUBSTEP", 40, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Saw, FM, Square }), -1, 20, 70); g.osc2 (Saw, -1, -15, 15, 30, 70); g.sub (40, 70);
                  g.f1 (g.pick ({ LP24, Acid }), 150, 400, 40, 70, 0, 20, 30, 60); g.fenv (0, 5, 300, 700, 50, 90, 100, 300);
                  g.amp (0, 5, 400, 900, 70, 100, 80, 250);
                  g.lfo (1, g.pick ({ LSine, LTri, LSaw }), g.pick ({ 2, 3, 8, 7 }), 50, 90, DCut1);
                  g.set ("drive", g.rr (40, 80)); g.hp (25, 40);
              } },
            { "UK GARAGE", 40, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Square, Sine, Tri }), -1, 0, 40); g.sub (40, 75);
                  g.f1 (LP12, 300, 1000, 10, 35, 20, 50); g.fenv (0, 1, 100, 250, 10, 40, 30, 90);
                  g.amp (0, 1, 150, 350, 30, 70, 30, 100); g.set ("mode", Legato); g.set ("glide", g.rr (30, 120)); g.hp (25, 40);
              } },
            { "NU DISCO", 40, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Square, Saw }), -1, 10, 40); g.osc2 (g.pick ({ Square, Saw }), 0, -6, 6, 25, 50); g.sub (25, 50);
                  g.f1 (LP24, 400, 1300, 20, 45, 35, 65, 10, 30); g.fenv (0, 1, 60, 160, 0, 20, 20, 60);
                  g.amp (0, 1, 80, 180, 0, 30, 20, 50); g.hp (35, 50);
              } },
            { "SYNTHWAVE", 40, [] (Gen& g)
              {
                  g.osc1 (Saw, -1); g.osc2 (Saw, -1, 8, 18, 50, 80); g.sub (30, 60);
                  g.f1 (LP24, 500, 2000, 10, 35, 20, 45, 5, 25); g.fenv (0, 5, 150, 400, 30, 60, 60, 200);
                  g.amp (0, 5, 200, 500, 60, 95, 60, 200); g.set ("glide", g.rr (0, 30)); g.hp (30, 45);
              } },
            { "AFRO HOUSE", 30, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Sine, Tri, FM }), -1, 0, 50); g.osc2 (Sine, 0, -3, 3, 10, 30); g.sub (40, 70);
                  g.f1 (LP12, 400, 1200, 5, 25, 15, 40); g.fenv (0, 1, 80, 200, 0, 30, 20, 60);
                  g.amp (0, 1, 120, 260, 10, 40, 30, 80); g.hp (25, 40);
              } },
            { "TRAP 808", 40, [] (Gen& g)
              {
                  g.osc1 (Sine, -1, 0, 40); g.osc2 (Tri, -1, -2, 2, 0, 20); g.sub (0, 30);
                  g.f1 (LP24, 1500, 6000, 0, 20, 0, 15, 30, 70); g.fenv (0, 1, 300, 800, 50, 90, 200, 500);
                  g.amp (0, 2, 700, 2000, 0, 10, 200, 600); g.set ("mode", Legato); g.set ("glide", g.rr (60, 220));
                  g.set ("drive", g.rr (30, 70)); g.hp (25, 35);
              } },
            { "ELECTRO", 40, [] (Gen& g)
              {
                  g.osc1 (g.pick ({ Square, Pulse, FM }), -1, 20, 70); g.osc2 (Saw, 0, -10, 10, 20, 50); g.sub (30, 55);
                  g.f1 (LP24, 250, 800, 30, 65, 40, 80, 30, 60); g.fenv (0, 1, 60, 160, 0, 15, 20, 60);
                  g.amp (0, 1, 80, 200, 0, 30, 20, 60); g.lfo (1, LSH, g.pick ({ 4, 3 }), 5, 25, DCut1);
                  g.set ("drive", g.rr (40, 70)); g.hp (30, 45);
              } },
        };
        return c;
    }

    // A little personal character on top of the style, so presets in one folder don't sound alike
    void character (Gen& g)
    {
        if (g.v["l2depth"] <= 0.0f && g.chance (0.35f))
            g.lfo (2, g.pick ({ LSine, LTri }), g.pick ({ 0, 14, 15, 1 }), 3, 12, DCut1);
        if (g.v["noise"] <= 0.0f && g.chance (0.25f)) g.set ("noise", g.rr (2, 8));
        if (g.chance (0.3f)) g.set ("drive", juce::jmin (90.0f, g.v["drive"] + g.rr (5, 15)));
        if (g.chance (0.3f)) g.set ("subwave", 1.0f - g.v["subwave"]);
    }

    std::vector<skazi::Preset> build()
    {
        std::vector<skazi::Preset> list;
        int g = 0;
        for (auto& cat : categories())
            for (int i = 0; i < cat.count; ++i, ++g)
            {
                juce::Random r ((juce::int64) g * 7919 + 31337);
                skazi::Preset p { skazi::presetName (g), cat.name, defaultValues() };
                Gen gen { r, p.values };
                cat.style (gen);
                character (gen);
                list.push_back (std::move (p));
            }
        return list;
    }
} // namespace

const std::vector<skazi::Preset>& presets()
{
    static const std::vector<skazi::Preset> list = build();
    return list;
}

skazi::Preset randomPreset (juce::Random& rng)
{
    // a random style, rolled fresh
    const auto& cats = categories();
    const auto& cat = cats[(size_t) rng.nextInt ((int) cats.size())];
    skazi::Preset p { "Random", cat.name, defaultValues() };
    juce::Random r (rng.nextInt64());
    Gen gen { r, p.values };
    cat.style (gen);
    character (gen);
    return p;
}
} // namespace subich
