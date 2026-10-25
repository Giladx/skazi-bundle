#include "PluginEditor.h"

using namespace juce;
using namespace salat;

namespace
{
    // None, stutter, reverse, gate, tape stop, hold
    const Colour kEvent[NumEvents] = { Colour (0xff2a2a2e), Colour (0xffff2a1f), Colour (0xffffb000),
                                       Colour (0xff7aa2ff), Colour (0xff22e3a5), Colour (0xfff4f4f4) };
    const char* const kEventName[NumEvents] = { "", "STUTTER", "REVERSE", "GATE", "TAPE STOP", "HOLD" };

    Rectangle<float> screen (Graphics& g, Rectangle<float> b)
    {
        skin::drawInsetPanel (g, b, 6.0f);
        auto in = b.reduced (7.0f);
        g.setColour (Colour (0xff070707));
        g.fillRoundedRectangle (in, 4.0f);
        return in;
    }

    void tag (Graphics& g, Rectangle<float> in, const String& text)
    {
        g.setFont (skin::font (10.5f));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (in.getX() + 6, in.getY() + 6, tw, 17, 4);
        g.setColour (skin::red2);
        g.drawText (text, (int) in.getX() + 13, (int) in.getY() + 6, (int) tw, 17, Justification::left);
    }

    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
}

void ChopView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    auto& e = proc.engine;

    static const char* const steps[] = { "1/4", "1/8", "1/16", "1/32" };
    static const char* const pos[] = { "FIRST", "MIDDLE", "LAST" };
    tag (g, in, ! s.chopOn ? String ("CHOP OFF")
                : s.hold   ? String ("HOLD") + dot + steps[s.chopStep] + " / " + String (s.repeats)
                           : String ("CHOP ") + steps[s.chopStep] + dot + String (roundToInt (s.chance * 100)) + "%" + dot + pos[s.chopPos]);

    // 16 steps, the newest (the one playing now) on the right
    auto grid = in.reduced (10.0f, 0.0f).withTrimmedTop (30.0f).withHeight (34.0f);
    const int count = e.historyCount.load();
    const float cw = grid.getWidth() / Engine::kHistory;
    for (int j = 0; j < Engine::kHistory; ++j)
    {
        const int idx = count - Engine::kHistory + j;
        const int ev = idx >= 0 ? e.history[(size_t) (idx % Engine::kHistory)].load() : None;
        auto cell = Rectangle<float> (grid.getX() + j * cw, grid.getY(), cw, grid.getHeight()).reduced (2.0f);
        const bool now = j == Engine::kHistory - 1 && count > 0;
        g.setColour (ev == None ? Colour (0xff141416) : kEvent[ev].withAlpha (now ? 0.9f : 0.55f));
        g.fillRoundedRectangle (cell, 3.0f);
        if (now)
        {
            g.setColour (Colours::white.withAlpha (0.25f));
            g.fillRect (cell.withWidth (cell.getWidth() * e.uiStepPhase.load()).withTrimmedTop (cell.getHeight() - 4.0f));
            g.setColour (Colours::white.withAlpha (0.8f));
            g.drawRoundedRectangle (cell, 3.0f, 1.2f);
        }
    }

    // output over the last two seconds, coloured by what the chopper was doing
    auto sc = in.reduced (10.0f, 0.0f).withTrimmedTop (72.0f).withTrimmedBottom (22.0f);
    const float bw = sc.getWidth() / Engine::kScope, mid = sc.getCentreY(), half = sc.getHeight() * 0.5f;
    const int w = e.scopeWrite.load();
    for (int j = 0; j < Engine::kScope; ++j)
    {
        const int k = (w + j) % Engine::kScope;
        const float pk = jlimit (0.0f, 1.0f, e.scope[(size_t) k].load());
        const int ev = jlimit (0, NumEvents - 1, e.scopeEvent[(size_t) k].load());
        g.setColour (ev == None ? Colours::white.withAlpha (0.35f) : kEvent[ev]);
        g.fillRect (sc.getX() + j * bw, mid - pk * half, jmax (1.0f, bw - 0.6f), jmax (1.0f, pk * half * 2.0f));
    }

    // legend
    g.setFont (skin::font (9.0f));
    float x = in.getX() + 12.0f;
    for (int ev = Stutter; ev < NumEvents; ++ev)
    {
        g.setColour (kEvent[ev]);
        g.fillEllipse (x, in.getBottom() - 13.0f, 7.0f, 7.0f);
        g.setColour (skin::textDim);
        g.drawText (kEventName[ev], (int) x + 10, (int) in.getBottom() - 16, 70, 12, Justification::left);
        x += 22.0f + GlyphArrangement::getStringWidth (g.getCurrentFont(), kEventName[ev]);
    }
}

void FilterView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    auto X = [&] (double f) { return in.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * in.getWidth(); };
    const float top = in.getY() + 28.0f, bot = in.getBottom() - 16.0f;
    auto Y = [&] (float db) { return jmap (jlimit (-30.0f, 24.0f, db), 24.0f, -30.0f, top, bot); };

    g.setFont (skin::font (9.0f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) X (f), in.getY(), in.getBottom());
    }
    for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
    {
        g.setColour (skin::textDim);
        g.drawText (t, (int) X (f) + 3, (int) in.getBottom() - 14, 30, 12, Justification::left);
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) Y (0.0f), in.getX(), in.getRight());

    static const char* const types[] = { "LOW PASS", "BAND PASS", "HIGH PASS", "NOTCH" };
    if (! s.filterOn)
    {
        tag (g, in, "FILTER OFF");
        return;
    }

    const float fc = proc.engine.uiCutoff.load();
    if (s.lfoDepth > 0.0f)
    {
        const float lo = X (jmax (20.0, s.cutoff * std::exp2 (-(double) s.lfoDepth)));
        const float hi = X (jmin (20000.0, s.cutoff * std::exp2 ((double) s.lfoDepth)));
        g.setColour (skin::red2.withAlpha (0.08f));
        g.fillRect (lo, in.getY(), hi - lo, in.getHeight());
    }

    Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, i / 200.0f);
        const float db = Decibels::gainToDecibels (filterMagnitude (s.filterType, fc, s.reso, f), -60.0f);
        const float px = in.getX() + i / 200.0f * in.getWidth(), py = Y (db);
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    Path fill (p);
    fill.lineTo (in.getRight(), bot);
    fill.lineTo (in.getX(), bot);
    fill.closeSubPath();
    g.setColour (skin::red2.withAlpha (0.12f));
    g.fillPath (fill);
    for (int i = 3; i > 0; --i)
    {
        g.setColour (skin::red2.withAlpha (0.1f));
        g.strokePath (p, PathStrokeType (2.0f + i * 3.0f));
    }
    g.setColour (skin::red2);
    g.strokePath (p, PathStrokeType (2.2f));

    tag (g, in, String (types[s.filterType]) + dot + skazi::formatValue (fc, "Hz") + dot + "RES " + String (roundToInt (s.reso * 100)) + "%");
}

void RiseView::paint (Graphics& g)
{
    auto in = screen (g, getLocalBounds().toFloat());
    const auto s = proc.settings();
    const float raw = proc.engine.uiRiseProgress.load();
    const bool waiting = raw < 0.0f;
    const float progress = jmax (0.0f, raw);
    const float semis = proc.engine.uiRiseSemis.load();
    const int bars = riseBars (s.riseLen);

    // bar ticks, one per bar of the rise
    auto track = in.reduced (12.0f, 0.0f).withTrimmedTop (28.0f).withTrimmedBottom (10.0f);
    g.setColour (Colour (0xff141416));
    g.fillRoundedRectangle (track, 4.0f);
    for (int b = 1; b < bars; ++b)
    {
        g.setColour (Colours::white.withAlpha (b % 4 == 0 ? 0.18f : 0.07f));
        g.drawVerticalLine ((int) (track.getX() + track.getWidth() * b / bars), track.getY(), track.getBottom());
    }

    const auto col = s.riseSemis >= 0.0f ? Colour (0xff22e3a5) : Colour (0xff7aa2ff);
    if (s.riseOn)
    {
        auto fill = track.withWidth (jmax (2.0f, track.getWidth() * progress));
        g.setGradientFill (ColourGradient (col.withAlpha (0.25f), fill.getX(), 0, col, fill.getRight(), 0, false));
        g.fillRoundedRectangle (fill, 4.0f);
    }

    const String sign = semis >= 0.05f ? "+" : "";
    const String text = ! s.riseOn ? String ("RISE OFF") + dot + "STARTS AND ENDS ON A BAR LINE" + dot + "NOT CHANGED BY RANDOM"
                      : waiting ? String ("WAITING FOR THE NEXT BAR") + dot + String (bars) + " BARS TO " + (s.riseSemis > 0 ? "+" : "") + String (roundToInt (s.riseSemis)) + " st"
                      : progress >= 1.0f ? String ("AT THE TOP") + dot + sign + String (semis, 1) + " st" + dot + "HOLDING"
                                         : String ("RISING") + dot + sign + String (semis, 1) + " st" + dot + "BAR "
                                               + String (jmin (bars, 1 + (int) (progress * bars))) + " OF " + String (bars);
    tag (g, in, text);
}

void SalatButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float glow = jmax (flash, over ? 0.35f : 0.0f);
    if (glow > 0.0f)
    {
        g.setColour (skin::red.withAlpha (0.25f * glow + 0.1f));
        g.fillRoundedRectangle (r.expanded (3.0f), 12.0f);
    }
    ColourGradient grad (skin::red2.brighter (0.25f * flash), r.getX(), r.getY(), Colour (0xff8a0400), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (down ? r.reduced (1.5f) : r, 10.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillRoundedRectangle (r.reduced (5.0f).withHeight (r.getHeight() * 0.35f), 8.0f);
    g.setColour (skin::red2.brighter (0.4f));
    g.drawRoundedRectangle (r, 10.0f, 1.2f);

    g.setColour (Colours::white.withAlpha (0.85f));
    g.setFont (skin::font (13.0f));
    g.drawText ("MAKE A", r.withHeight (r.getHeight() * 0.42f).withTrimmedTop (10.0f), Justification::centredBottom);
    g.setColour (Colours::white);
    g.setFont (skin::titleFont (46.0f));
    g.drawText ("SALAT", r.withTrimmedTop (r.getHeight() * 0.36f), Justification::centredTop);
}

//==============================================================================
SalatEditor::SalatEditor (SalatProcessor& p)
    : EditorBase (p, "SA", "LAT", String::fromUTF8 ("GLITCH MULTI-FX  \xc2\xb7  BY ASHER SWISSA"),
                  String::fromUTF8 ("SALAT v1.0  \xc2\xb7  TEMPO-SYNCED CHOPPER + 7 FX  \xc2\xb7  VST3 / AU  \xc2\xb7  EFFECT")),
      sp (p), chopView (p), filterView (p), riseView (p)
{
    setSubtitleSize (15.0f);

    addAndMakeVisible (chopView);
    addAndMakeVisible (filterView);
    addAndMakeVisible (riseView);

    addAndMakeVisible (salatBtn);
    salatBtn.setTooltip ("Throws a random mix of effects together and chops the next four steps");
    salatBtn.onClick = [this] { sp.makeASalat(); salatBtn.flash = 1.0f; };

    addAndMakeVisible (holdBtn);
    holdBtn.setTooltip ("Hold to loop the current slice");
    holdBtn.onStateChange = [this]
    {
        const bool d = holdBtn.isDown();
        if (d == holdDown) return;
        holdDown = d;
        if (auto* prm = sp.apvts.getParameter ("hold"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (d ? 1.0f : 0.0f);
            prm->endChangeGesture();
        }
    };

    const auto red = skin::red2, amber = Colour (0xffffb000), blue = Colour (0xff7aa2ff), green = Colour (0xff22e3a5);

    chopOn = &addToggle ("chopOn", "ON");
    posRow = &addChoiceRow ("chopPos");
    stepRow = &addChoiceRow ("chopStep");
    repRow = &addChoiceRow ("chopRep");
    chance = &addKnob ("chance", "CHANCE", skin::white, "%", 34);
    wStut = &addKnob ("wStut", "STUTTER", kEvent[Stutter], "%", 34);
    wRev = &addKnob ("wRev", "REVERSE", kEvent[Reverse], "%", 34);
    wGate = &addKnob ("wGate", "GATE", kEvent[Gate], "%", 34);
    wTape = &addKnob ("wTape", "TAPE STOP", kEvent[Tape], "%", 34);

    driveOn = &addToggle ("drvOn", "ON");
    driveRow = &addChoiceRow ("drvMode");
    drive = &addKnob ("drive", "DRIVE", red, "dB", 40);

    crushOn = &addToggle ("crOn", "ON");
    bits = &addKnob ("bits", "BITS", amber, "bit", 34);
    bits->setFormatter ([] (double v) { return String (v, 1) + " bit"; });
    down = &addKnob ("down", "RATE", amber, "x", 34);
    down->setFormatter ([] (double v) { return v < 1.5 ? String ("FULL") : "1/" + String (roundToInt (v)); });

    filterOn = &addToggle ("fltOn", "ON");
    filterRow = &addChoiceRow ("fltType");
    cutoff = &addKnob ("cutoff", "CUTOFF", red, "Hz", 34);
    reso = &addKnob ("reso", "RES", red, "%", 34);
    lfoDepth = &addKnob ("lfoDepth", "LFO", red, "oct", 34);
    lfoDepth->setFormatter ([] (double v) { return v < 0.01 ? String ("OFF") : String::fromUTF8 ("\xc2\xb1") + String (v, 1) + " oct"; });
    lfoRate = &addCombo ("lfoRate");
    lfoShape = &addCombo ("lfoShape");

    ringOn = &addToggle ("ringOn", "ON");
    ringFreq = &addKnob ("ringFreq", "FREQ", blue, "Hz", 34);
    ringMix = &addKnob ("ringMix", "MIX", blue, "%", 34);

    modOn = &addToggle ("modOn", "ON");
    modRow = &addChoiceRow ("modType");
    modRate = &addKnob ("modRate", "RATE", blue, "Hz", 34);
    modRate->setFormatter ([] (double v) { return String (v, 2) + " Hz"; });
    modDepth = &addKnob ("modDepth", "DEPTH", blue, "%", 34);
    modFb = &addKnob ("modFb", "FDBK", blue, "%", 34);
    modMix = &addKnob ("modMix", "MIX", blue, "%", 34);

    delayOn = &addToggle ("dlyOn", "ON");
    dlyTime = &addCombo ("dlyTime");
    dlyFb = &addKnob ("dlyFb", "FDBK", green, "%", 34);
    dlyTone = &addKnob ("dlyTone", "TONE", green, "%", 34);
    dlyMix = &addKnob ("dlyMix", "MIX", green, "%", 34);

    reverbOn = &addToggle ("revOn", "ON");
    revSize = &addKnob ("revSize", "SIZE", green, "%", 34);
    revDamp = &addKnob ("revDamp", "DAMP", green, "%", 34);
    revMix = &addKnob ("revMix", "MIX", green, "%", 34);

    inGain = &addKnob ("in", "INPUT", skin::white, "dB", 34);
    mix = &addKnob ("mix", "MIX", skin::white, "%", 34);
    outGain = &addKnob ("out", "OUTPUT", skin::white, "dB", 34);
    outMeter = &addMeter (p.engine.uiOut);
    grMeter = &addMeter (p.engine.uiReduction, true);

    riseOn = &addToggle ("riseOn", "ON");
    riseOn->setTooltip ("Starts the pitch rise from this moment. Automate it in your DAW at the start of the build");
    riseLenRow = &addChoiceRow ("riseLen", {}, green);
    riseAmt = &addKnob ("riseAmt", "AMOUNT", green, "st", 34);
    riseAmt->setFormatter ([] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1) + " st"; });

    // 12 / 24 / 48: one, two or four octaves in one click, keeping the direction (up or down)
    for (int q = 0; q < 3; ++q)
    {
        auto& b = riseQuick[q];
        const int semis = q == 0 ? 12 : q == 1 ? 24 : 48;
        b.setTooltip ("Rise " + String (semis / 12) + (semis == 12 ? " octave" : " octaves"));
        b.setClickingTogglesState (false);
        b.onClick = [this, semis]
        {
            if (auto* prm = sp.apvts.getParameter ("riseAmt"))
            {
                const float now = sp.v ("riseAmt");
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (now < 0.0f ? (float) -semis : (float) semis));
                prm->endChangeGesture();
            }
        };
        addAndMakeVisible (b);
    }

    setSize (1200, 680);
}

void SalatEditor::layout()
{
    chopView.setBounds (20, 68, 580, 160);
    filterView.setBounds (608, 68, 340, 160);
    salatBtn.setBounds (956, 68, 224, 112);
    holdBtn.setBounds (956, 188, 224, 40);

    auto toggle = [] (skazi::ParamToggle& t, int x, int y) { t.setBounds (x, y + 6, 40, 20); };

    int y = 236;
    const int h = 150;
    panel ({ 20, y, 470, h }, "CHOP");
    toggle (*chopOn, 72, y);
    label ({ 186, y + 6, 70, 20 }, "POSITION");
    posRow->setBounds (250, y + 6, 230, 20);
    label ({ 30, y + 33, 40, 20 }, "STEP");
    stepRow->setBounds (64, y + 32, 170, 22);
    label ({ 246, y + 33, 50, 20 }, "REPEAT");
    repRow->setBounds (292, y + 32, 188, 22);
    int kx = 32;
    for (auto* k : { chance, wStut, wRev, wGate, wTape }) { put (*k, kx, y + 66, 34, 70); kx += 90; }

    panel ({ 498, y, 200, h }, "DRIVE");
    toggle (*driveOn, 560, y);
    driveRow->setBounds (508, y + 32, 180, 22);
    put (*drive, 498 + 64, y + 62, 40, 72);

    panel ({ 706, y, 160, h }, "CRUSH");
    toggle (*crushOn, 768, y);
    put (*bits, 712, y + 50, 34, 72);
    put (*down, 786, y + 50, 34, 72);

    panel ({ 874, y, 306, h }, "FILTER");
    toggle (*filterOn, 938, y);
    filterRow->setBounds (884, y + 32, 286, 22);
    put (*cutoff, 878, y + 66, 34, 66);
    put (*reso, 942, y + 66, 34, 66);
    put (*lfoDepth, 1006, y + 66, 34, 66);
    lfoRate->setBounds (1080, y + 66, 92, 22);
    lfoShape->setBounds (1080, y + 94, 92, 22);
    label ({ 1082, y + 120, 92, 14 }, "LFO RATE / SHAPE");

    y = 394;
    panel ({ 20, y, 160, h }, "RING");
    toggle (*ringOn, 70, y);
    put (*ringFreq, 26, y + 46, 34, 72);
    put (*ringMix, 100, y + 46, 34, 72);

    panel ({ 188, y, 300, h }, "MOD");
    toggle (*modOn, 236, y);
    modRow->setBounds (290, y + 6, 188, 20);
    kx = 194;
    for (auto* k : { modRate, modDepth, modFb, modMix }) { put (*k, kx, y + 46, 34, 70); kx += 72; }

    panel ({ 496, y, 270, h }, "DELAY");
    toggle (*delayOn, 552, y);
    label ({ 610, y + 6, 40, 20 }, "TIME");
    dlyTime->setBounds (650, y + 5, 106, 22);
    kx = 508;
    for (auto* k : { dlyFb, dlyTone, dlyMix }) { put (*k, kx, y + 46, 34, 76); kx += 84; }

    panel ({ 774, y, 200, h }, "REVERB");
    toggle (*reverbOn, 842, y);
    kx = 778;
    for (auto* k : { revSize, revDamp, revMix }) { put (*k, kx, y + 46, 34, 64); kx += 64; }

    panel ({ 982, y, 198, h }, "OUTPUT");
    kx = 984;
    for (auto* k : { inGain, mix, outGain }) { put (*k, kx, y + 46, 34, 54); kx += 52; }
    outMeter->setBounds (1146, y + 30, 10, 96);
    grMeter->setBounds (1162, y + 30, 10, 96);
    label ({ 1141, y + 128, 24, 12 }, "OUT");
    label ({ 1161, y + 128, 20, 12 }, "GR");

    // RISE: long pitch build-ups, left alone by presets, RANDOM and MAKE A SALAT
    y = 552;
    panel ({ 20, y, 1160, 86 }, "RISE");
    toggle (*riseOn, 70, y);
    label ({ 30, y + 34, 60, 20 }, "LENGTH");
    riseLenRow->setBounds (88, y + 33, 300, 22);
    put (*riseAmt, 398, y + 18, 34, 70);
    label ({ 476, y + 12, 150, 16 }, "RANGE (SEMITONES)");
    for (int q = 0; q < 3; ++q)
        riseQuick[q].setBounds (480 + q * 50, y + 33, 44, 22);
    riseView.setBounds (640, y + 6, 532, 74);
}

void SalatEditor::tick()
{
    chopView.repaint();
    filterView.repaint();
    riseView.repaint();
    const float amt = std::abs (sp.v ("riseAmt"));
    for (int q = 0; q < 3; ++q)
        riseQuick[q].setToggleState (std::abs (amt - (q == 0 ? 12.0f : q == 1 ? 24.0f : 48.0f)) < 0.05f, dontSendNotification);
    if (salatBtn.flash > 0.0f)
    {
        salatBtn.flash = jmax (0.0f, salatBtn.flash - 0.06f);
        salatBtn.repaint();
    }
    holdBtn.setToggleState (sp.v ("hold") > 0.5f, dontSendNotification);

    auto dim = [this] (const char* id, std::initializer_list<Component*> comps)
    {
        const float a = sp.v (id) > 0.5f ? 1.0f : 0.4f;
        for (auto* c : comps) c->setAlpha (a);
    };
    dim ("chopOn", { posRow, stepRow, repRow, chance, wStut, wRev, wGate, wTape });
    dim ("drvOn", { driveRow, drive });
    dim ("crOn", { bits, down });
    dim ("fltOn", { filterRow, cutoff, reso, lfoDepth, lfoRate, lfoShape });
    dim ("ringOn", { ringFreq, ringMix });
    dim ("modOn", { modRow, modRate, modDepth, modFb, modMix });
    dim ("dlyOn", { dlyTime, dlyFb, dlyTone, dlyMix });
    dim ("revOn", { revSize, revDamp, revMix });
}
