#include "PluginEditor.h"

using namespace juce;
using namespace subich;

namespace
{
    // Magnitude of the engine's state-variable filters at w = f / cutoff (same Q mapping as the engine)
    float filterMag (int type, double w, float res)
    {
        const float q = type == Acid ? 0.6f + res * res * 10.0f : 0.5f + res * res * 12.0f;
        const double k = 1.0 / q, kb = type == Acid ? 1.0 / (0.6 + res * 2.0) : 1.6;
        auto den = [w] (double kk) { return std::sqrt ((1.0 - w * w) * (1.0 - w * w) + kk * kk * w * w); };
        switch (type)
        {
            case LP24: case Acid: return (float) (1.0 / (den (k) * den (kb)));
            case LP12:  return (float) (1.0 / den (k));
            case HP12:  return (float) (w * w / den (k));
            case BP:    return (float) (k * w / den (k));
            case Notch: return (float) (std::abs (1.0 - w * w) / den (k));
            default:    return 1.0f;
        }
    }

    void darkScreen (Graphics& g, Rectangle<float> b)
    {
        skin::drawInsetPanel (g, b, 6.0f);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (b.reduced (7.0f), 4.0f);
    }

    void infoTag (Graphics& g, Rectangle<float> inner, const String& text)
    {
        g.setFont (skin::font (10.5f));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (inner.getX() + 6, inner.getY() + 6, tw, 17, 4);
        g.setColour (skin::red2);
        g.drawText (text, (int) inner.getX() + 13, (int) inner.getY() + 6, (int) tw, 17, Justification::left);
    }
}

//==============================================================================
void ScopeView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    darkScreen (g, b);
    auto in = b.reduced (7.0f);
    for (int i = 1; i < 8; ++i)
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) (in.getX() + in.getWidth() * i / 8), in.getY(), in.getBottom());
    }
    g.drawHorizontalLine ((int) in.getCentreY(), in.getX(), in.getRight());

    auto& e = proc.engine;
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float freq = e.uiFreq.load();
    const int period = (int) (sr / jmax (20.0f, freq > 0 ? freq : 100.0f));
    const int window = jlimit (64, Engine::kScopeSize / 2, period * 2);
    const int N = Engine::kScopeSize, end = e.scopeWrite.load();
    auto at = [&] (int i) { return e.scope[(size_t) (((i % N) + N) % N)].load(); };

    int start = end - window;
    for (int i = end - window; i > end - window - jmin (period, N / 2 - 2); --i)
        if (at (i - 1) < 0.0f && at (i) >= 0.0f) { start = i; break; }

    float peak = 0.05f;
    for (int i = 0; i < window; ++i) peak = jmax (peak, std::abs (at (start + i)));
    Path p;
    for (int i = 0; i < window; ++i)
    {
        const float x = in.getX() + in.getWidth() * i / (window - 1);
        const float y = in.getCentreY() - at (start + i) / peak * in.getHeight() * 0.42f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    for (int i = 3; i > 0; --i)
    {
        g.setColour (skin::red2.withAlpha (0.1f));
        g.strokePath (p, PathStrokeType (2.0f + i * 3.0f));
    }
    g.setColour (skin::red2);
    g.strokePath (p, PathStrokeType (2.0f));

    static const char* modes[] = { "MONO", "LEGATO", "POLY" };
    const int note = e.uiNote.load();
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    String info = "OUTPUT";
    if (note >= 0) info << dot << MidiMessage::getMidiNoteName (note, true, true, 3) << " " << String (freq, 1) << " Hz";
    info << dot << modes[(int) proc.v ("mode")];
    infoTag (g, in, info);
}

void FilterView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    darkScreen (g, b);
    auto in = b.reduced (7.0f);
    auto xFor = [&] (double f) { return in.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * in.getWidth(); };
    g.setFont (skin::font (9.0f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) xFor (f), in.getY(), in.getBottom());
        if (f == 100.0 || f == 1000.0 || f == 10000.0)
        {
            g.setColour (skin::textDim);
            g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) xFor (f) + 3, (int) in.getBottom() - 14, 30, 12, Justification::left);
        }
    }

    const int t1 = (int) proc.v ("f1type"), t2 = (int) proc.v ("f2type");
    const float r1 = proc.v ("f1res") * 0.01f, r2 = proc.v ("f2res") * 0.01f;
    const float c1 = proc.engine.uiCut1.load(), c2 = proc.engine.uiCut2.load();
    const bool parallel = proc.v ("routing") > 0.5f;
    auto yFor = [&] (float mag) { const float db = jlimit (-48.0f, 24.0f, Decibels::gainToDecibels (mag, -60.0f)); return in.getY() + in.getHeight() * (24.0f - db) / 72.0f; };

    Path line, fill;
    const int w = (int) in.getWidth();
    for (int x = 0; x <= w; ++x)
    {
        const double f = 20.0 * std::pow (1000.0, (double) x / w);
        const float m1 = filterMag (t1, f / c1, r1), m2 = filterMag (t2, f / c2, r2);
        const float y = yFor (parallel ? 0.5f * (m1 + m2) : m1 * m2);
        if (x == 0) { line.startNewSubPath (in.getX(), y); fill.startNewSubPath (in.getX(), in.getBottom()); }
        line.lineTo (in.getX() + x, y);
        fill.lineTo (in.getX() + x, y);
    }
    fill.lineTo (in.getRight(), in.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (skin::red2.withAlpha (0.25f), 0, in.getY(), skin::red2.withAlpha (0.02f), 0, in.getBottom(), false));
    g.fillPath (fill);
    g.setColour (skin::red2);
    g.strokePath (line, PathStrokeType (2.2f));

    static const char* types[] = { "LP24", "LP12", "HP12", "BP", "NOTCH", "ACID" };
    infoTag (g, in, String ("FILTER 1 ") + types[t1] + String::fromUTF8 (" \xe2\x86\x92 ") + "FILTER 2 " + types[t2]
                        + String::fromUTF8 ("  \xc2\xb7  ") + (parallel ? "PARALLEL" : "SERIAL"));
}

void EnvView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (b, 3.0f);
    const float a = proc.v ((pre + "a").toRawUTF8()), d = proc.v ((pre + "d").toRawUTF8()),
                s = proc.v ((pre + "s").toRawUTF8()) * 0.01f, r = proc.v ((pre + "r").toRawUTF8());
    const float hold = 300.0f, total = a + d + hold + r;
    auto in = b.reduced (3.0f);
    auto X = [&] (float t) { return in.getX() + t / total * in.getWidth(); };
    auto Y = [&] (float lv) { return in.getBottom() - lv * in.getHeight(); };
    Path p;
    p.startNewSubPath (X (0), Y (0));
    p.lineTo (X (a), Y (1));
    for (int i = 1; i <= 16; ++i) { const float t = i / 16.0f; p.lineTo (X (a + d * t), Y (s + (1 - s) * std::exp (-4.6f * t))); }
    p.lineTo (X (a + d + hold), Y (s));
    for (int i = 1; i <= 16; ++i) { const float t = i / 16.0f; p.lineTo (X (a + d + hold + r * t), Y (s * std::exp (-4.6f * t))); }
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (5.0f));
    g.setColour (colour);
    g.strokePath (p, PathStrokeType (1.8f));
}

//==============================================================================
void PerfWheel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto slot = b.withTrimmedTop (14.0f);
    g.setColour (skin::textDim);
    g.setFont (skin::font (9.0f));
    g.drawText (name, b.withHeight (12.0f), Justification::centred);

    // recessed slot with a rubber wheel whose ribs roll with the value
    g.setColour (Colours::black);
    g.fillRoundedRectangle (slot, 4.0f);
    auto wheel = slot.reduced (4.0f, 3.0f);
    g.setColour (Colour (0xff050505));
    g.fillRoundedRectangle (wheel, 3.0f);
    ColourGradient shine (Colour (0x00ffffff), wheel.getX(), wheel.getY(), Colour (0x00ffffff), wheel.getX(), wheel.getBottom(), false);
    shine.addColour (0.5, Colour (0x40ffffff));
    g.setGradientFill (shine);
    g.fillRoundedRectangle (wheel, 3.0f);
    const float roll = (centred ? value : value * 2.0f - 1.0f) * 0.5f;
    for (int i = -8; i <= 8; ++i)
    {
        const float a = (i / 8.0f + roll) * MathConstants<float>::halfPi;
        if (std::abs (a) >= MathConstants<float>::halfPi) continue;
        const float y = wheel.getCentreY() + std::sin (a) * wheel.getHeight() * 0.5f;
        g.setColour (Colours::white.withAlpha (0.12f * std::cos (a)));
        g.drawHorizontalLine ((int) y, wheel.getX() + 2, wheel.getRight() - 2);
    }
    // position marker (and the centre notch on the pitch wheel)
    const float pos = centred ? 0.5f - value * 0.5f : 1.0f - value;
    const float my = wheel.getY() + pos * wheel.getHeight();
    g.setColour (skin::red2);
    g.fillRect (wheel.getX() + 1, my - 2.0f, wheel.getWidth() - 2, 4.0f);
    if (centred)
    {
        g.setColour (Colours::white.withAlpha (0.3f));
        g.drawHorizontalLine ((int) wheel.getCentreY(), slot.getX(), wheel.getX());
    }
}

void PerfWheel::mouseDrag (const MouseEvent& e)
{
    auto slot = getLocalBounds().toFloat().withTrimmedTop (14.0f).reduced (4.0f, 3.0f);
    const float t = jlimit (0.0f, 1.0f, (e.position.y - slot.getY()) / slot.getHeight());
    value = centred ? (0.5f - t) * 2.0f : 1.0f - t;
    write (value);
    repaint();
}

void PerfWheel::mouseUp (const MouseEvent&)
{
    dragging = false;
    if (centred) { value = 0.0f; write (0.0f); repaint(); }
}

//==============================================================================
SubichEditor::SubichEditor (SubichProcessor& p)
    : EditorBase (p, "SUB", "ICH", String::fromUTF8 ("BASS + SUB BASS SYNTH  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("SUBICH v1.0  \xc2\xb7  1000 BASS PRESETS  \xc2\xb7  VST3 / AU  \xc2\xb7  INSTRUMENT")),
      sp (p), scope (p), filter (p), ampEnv (p, "a", Colour (0xffffe14d)), filtEnv (p, "f", Colour (0xffffb000)),
      keys (p.keyboard, MidiKeyboardComponent::horizontalKeyboard),
      pitchWheel ("PITCH", true, [&p] { return p.engine.uiBend.load(); },
                  [&p] (float v) { p.screenPitch.store (v); p.pitchMoved.store (true); }),
      modWheel ("MOD", false, [&p] { return p.engine.uiMod.load(); },
                [&p] (float v) { p.screenMod.store (v); p.modMoved.store (true); })
{
    namespace ic = skazi::icons;
    const std::vector<skazi::ChoiceRow::Icon> waveIcons { ic::sine, ic::tri, ic::saw, ic::square, ic::pulse, ic::fm, ic::noise };
    const std::vector<skazi::ChoiceRow::Icon> lfoIcons { ic::sine, ic::tri, ic::saw, ic::square, ic::sh };
    const auto red = skin::red2, orange = Colour (0xffff5a1f), green = Colour (0xff22e3a5), grey = Colour (0xffc0c0c0);

    addAndMakeVisible (scope);
    addAndMakeVisible (filter);
    addAndMakeVisible (ampEnv);
    addAndMakeVisible (filtEnv);

    o1wave = &addChoiceRow ("o1wave", waveIcons);
    o1oct = &addKnob ("o1oct", "OCT", red, "oct", 28);  o1semi = &addKnob ("o1semi", "SEMI", red, "st", 28);
    o1fine = &addKnob ("o1fine", "FINE", red, "ct", 28); o1shape = &addKnob ("o1shape", "SHAPE", red, "%", 28);
    o1level = &addKnob ("o1level", "LEVEL", red, "%", 28);

    o2wave = &addChoiceRow ("o2wave", waveIcons, orange);
    o2sync = &addToggle ("o2sync", "SYNC"); o2fm = &addToggle ("o2fm", "FM");
    o2sync->setTooltip ("Hard-sync OSC 2 to OSC 1");
    o2fm->setTooltip ("OSC 2 frequency-modulates OSC 1 (LEVEL sets the FM depth)");
    o2oct = &addKnob ("o2oct", "OCT", orange, "oct", 28); o2semi = &addKnob ("o2semi", "SEMI", orange, "st", 28);
    o2detune = &addKnob ("o2detune", "DETUNE", orange, "ct", 28); o2shape = &addKnob ("o2shape", "SHAPE", orange, "%", 28);
    o2level = &addKnob ("o2level", "LEVEL", orange, "%", 28);

    subwave = &addChoiceRow ("subwave", { ic::sine, ic::tri }, green);
    suboct = &addChoiceRow ("suboct", {}, green);
    sublevel = &addKnob ("sublevel", "LEVEL", green, "%", 36);
    noise = &addKnob ("noise", "NOISE", green, "%", 36);

    f1type = &addCombo ("f1type");
    f2type = &addCombo ("f2type");
    routing = &addChoiceRow ("routing");
    const char* fn[] = { "cut", "res", "env", "drive", "key" };
    const char* fl[] = { "CUTOFF", "RESO", "ENV", "DRIVE", "KEY" };
    const char* fs[] = { "Hz", "%", "%", "%", "%" };
    for (int i = 0; i < 5; ++i)
    {
        f1[i] = &addKnob (String ("f1") + fn[i], fl[i], skin::white, fs[i], 28);
        f2[i] = &addKnob (String ("f2") + fn[i], fl[i], grey, fs[i], 28);
    }

    const char* en[] = { "A", "D", "S", "R" };
    const char* es[] = { "ms", "ms", "%", "ms" };
    for (int i = 0; i < 4; ++i)
    {
        amp[i] = &addKnob (String ("a") + String (en[i]).toLowerCase(), en[i], Colour (0xffffe14d), es[i], 28);
        fenv[i] = &addKnob (String ("f") + String (en[i]).toLowerCase(), en[i], Colour (0xffffb000), es[i], 28);
    }

    l1shape = &addChoiceRow ("l1shape", lfoIcons, Colour (0xff7aa2ff));
    l2shape = &addChoiceRow ("l2shape", lfoIcons, Colour (0xffc084fc));
    l1sync = &addToggle ("l1sync", "SYNC"); l2sync = &addToggle ("l2sync", "SYNC");
    l1rate = &addCombo ("l1rate"); l2rate = &addCombo ("l2rate");
    l1dest = &addCombo ("l1dest"); l2dest = &addCombo ("l2dest");
    l1hz = &addKnob ("l1hz", "RATE", Colour (0xff7aa2ff), "Hz", 32); l1depth = &addKnob ("l1depth", "DEPTH", Colour (0xff7aa2ff), "%", 32);
    l2hz = &addKnob ("l2hz", "RATE", Colour (0xffc084fc), "Hz", 32); l2depth = &addKnob ("l2depth", "DEPTH", Colour (0xffc084fc), "%", 32);
    l1hz->setFormatter ([] (double v) { return String (v, 2) + " Hz"; });
    l2hz->setFormatter ([] (double v) { return String (v, 2) + " Hz"; });

    mode = &addChoiceRow ("mode");
    glide = &addKnob ("glide", "GLIDE", skin::white, "ms", 28); velo = &addKnob ("velo", "VELO", skin::white, "%", 28);
    bend = &addKnob ("bend", "BEND", skin::white, "st", 28);    drive = &addKnob ("drive", "DRIVE", skin::white, "%", 28);
    vol = &addKnob ("vol", "VOL", skin::white, "dB", 30);
    outMeter = &addMeter (p.engine.uiOut);

    keys.setAvailableRange (24, 71);
    keys.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe9eaec));
    keys.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff111113));
    keys.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff6b6e73));
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, skin::red);
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, skin::red.withAlpha (0.3f));
    keys.setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.3f));
    keys.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, skin::panel2);
    addAndMakeVisible (keys);

    // performance strip
    addAndMakeVisible (pitchWheel);
    addAndMakeVisible (modWheel);
    pitchWheel.setTooltip ("Pitch bend (springs back). Range is set by BEND in VOICE");
    modWheel.setTooltip ("Mod wheel: fades LFO 1 in up to full depth (also follows CC1 from your keyboard)");
    glideBig = &addKnob ("glide", "GLIDE", skin::white, "ms", 34);
    panicBtn.setTooltip ("Stop every note right now");
    panicBtn.onClick = [&p, this]
    {
        p.keyboard.allNotesOff (0);
        p.panicRequested.store (true);
        keys.repaint();
    };
    randomBtn.setToggleState (true, dontSendNotification); // drawn lit red
    randomBtn.setTooltip ("Random bass sound");
    randomBtn.onClick = [&p] { p.randomizeAll(); };
    addAndMakeVisible (panicBtn);
    addAndMakeVisible (randomBtn);

    setSize (1000, 684);
}

void SubichEditor::layout()
{
    scope.setBounds (20, 68, 570, 150);
    filter.setBounds (598, 68, 382, 150);

    auto knobRow = [] (std::initializer_list<skazi::Knob*> ks, int x, int y, int size, int cell)
    {
        for (auto* k : ks) { put (*k, x, y, size, cell); x += cell; }
    };

    // row 1: OSC 1, OSC 2, SUB, FILTER 1, FILTER 2
    const int y1 = 226, h1 = 160;
    // oscillators: wave buttons, then pitch knobs on one line and shape/level on a second
    panel ({ 20, y1, 200, h1 }, "OSC 1");
    o1wave->setBounds (28, y1 + 28, 184, 22);
    panel ({ 228, y1, 200, h1 }, "OSC 2");
    o2sync->setBounds (296, y1 + 6, 46, 20);
    o2fm->setBounds (346, y1 + 6, 36, 20);
    o2wave->setBounds (236, y1 + 28, 184, 22);

    knobRow ({ o1oct, o1semi, o1fine }, 30, y1 + 52, 28, 60);
    knobRow ({ o1shape, o1level }, 60, y1 + 104, 28, 60);
    knobRow ({ o2oct, o2semi, o2detune }, 238, y1 + 52, 28, 60);
    knobRow ({ o2shape, o2level }, 268, y1 + 104, 28, 60);

    panel ({ 436, y1, 130, h1 }, "SUB");
    subwave->setBounds (444, y1 + 28, 60, 22);
    suboct->setBounds (510, y1 + 28, 48, 22);
    knobRow ({ sublevel, noise }, 442, y1 + 62, 36, 59);

    panel ({ 574, y1, 200, h1 }, "FILTER 1");
    f1type->setBounds (650, y1 + 5, 116, 21);
    knobRow ({ f1[0], f1[1], f1[2] }, 580, y1 + 34, 28, 63);
    knobRow ({ f1[3], f1[4] }, 612, y1 + 98, 28, 63);

    panel ({ 782, y1, 198, h1 }, "FILTER 2");
    f2type->setBounds (858, y1 + 5, 114, 21);
    knobRow ({ f2[0], f2[1], f2[2] }, 788, y1 + 34, 28, 63);
    knobRow ({ f2[3], f2[4] }, 790, y1 + 98, 28, 63);
    routing->setBounds (912, y1 + 108, 62, 40);

    // row 2: AMP ENV, FILTER ENV, LFO 1, LFO 2, VOICE, OUT
    const int y2 = 394, h2 = 162;
    panel ({ 20, y2, 170, h2 }, "AMP ENV");
    ampEnv.setBounds (28, y2 + 28, 154, 40);
    knobRow ({ amp[0], amp[1], amp[2], amp[3] }, 24, y2 + 78, 28, 41);

    panel ({ 198, y2, 170, h2 }, "FILTER ENV");
    filtEnv.setBounds (206, y2 + 28, 154, 40);
    knobRow ({ fenv[0], fenv[1], fenv[2], fenv[3] }, 202, y2 + 78, 28, 41);

    auto lfoPanel = [&] (int x, const String& title, skazi::ChoiceRow* shape, skazi::ParamToggle* sync, juce::ComboBox* rate,
                         skazi::Knob* hz, skazi::Knob* depth, juce::ComboBox* dest)
    {
        panel ({ x, y2, 196, h2 }, title);
        sync->setBounds (x + 60, y2 + 6, 46, 20);
        shape->setBounds (x + 8, y2 + 30, 180, 22);
        rate->setBounds (x + 12, y2 + 70, 76, 22);
        put (*hz, x + 4, y2 + 60, 32, 80);
        put (*depth, x + 100, y2 + 60, 32, 80);
        label ({ x + 12, y2 + 124, 30, 12 }, "TO");
        dest->setBounds (x + 36, y2 + 120, 152, 22);
    };
    lfoPanel (376, "LFO 1", l1shape, l1sync, l1rate, l1hz, l1depth, l1dest);
    lfoPanel (580, "LFO 2", l2shape, l2sync, l2rate, l2hz, l2depth, l2dest);

    panel ({ 784, y2, 132, h2 }, "VOICE");
    mode->setBounds (790, y2 + 28, 120, 22);
    knobRow ({ glide, velo }, 790, y2 + 56, 28, 60);
    knobRow ({ bend, drive }, 790, y2 + 108, 28, 60);

    panel ({ 924, y2, 56, h2 }, "OUT");
    put (*vol, 924, y2 + 28, 30, 56);
    outMeter->setBounds (946, y2 + 92, 12, 62);

    // performance strip: wheels, glide, panic, random, then the keyboard
    panel ({ 20, 564, 236, 76 }, {});
    pitchWheel.setBounds (28, 567, 34, 70);
    modWheel.setBounds (68, 567, 34, 70);
    put (*glideBig, 104, 568, 34, 56);
    panicBtn.setBounds (166, 572, 82, 28);
    randomBtn.setBounds (166, 606, 82, 28);
    keys.setBounds (264, 564, 716, 76);
    keys.setKeyWidth (716.0f / 28.0f);
    keys.setLowestVisibleKey (24);
}

void SubichEditor::tick()
{
    scope.repaint();
    filter.repaint();
    ampEnv.repaint();
    filtEnv.repaint();
    const bool s1 = sp.v ("l1sync") > 0.5f, s2 = sp.v ("l2sync") > 0.5f;
    l1rate->setVisible (s1); l1hz->setVisible (! s1);
    l2rate->setVisible (s2); l2hz->setVisible (! s2);
    pitchWheel.refresh();
    modWheel.refresh();
}
