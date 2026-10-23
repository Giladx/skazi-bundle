#include "PluginEditor.h"
#include "Presets.h"

#if SHAK_HAS_LOGO
 #include "BinaryData.h"
#endif

using namespace juce;

namespace
{
    constexpr int kWidth = 1000, kHeight = 630;
    using skin::engrave;

    float curvePower (const pump::Params& p) { return std::pow (2.0f, (0.5f - p.curve) * 4.0f); }
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& state, const String& paramId, const String& n,
            Colour accent, const String& sfx, int size)
    : name (n), suffix (sfx), knobSize (size)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderAtt> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    value.setJustificationType (Justification::centred);
    value.setFont (skin::font (11.5f));
    value.setColour (Label::textColourId, skin::white);
    value.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (value);

    auto update = [this]
    {
        const double v = slider.getValue();
        String num = (suffix == "dB" || suffix == "ms") ? String (v, 1) : String (roundToInt (v));
        if (suffix == "dB" && v > 0.0)
            num = "+" + num;
        value.setText (num + (suffix.isEmpty() ? "" : (suffix == "deg" ? String::fromUTF8 ("\xc2\xb0") : " " + suffix)),
                       dontSendNotification);
    };
    slider.onValueChange = update;
    update();
}

void Knob::resized()
{
    slider.setBounds ((getWidth() - knobSize) / 2, 0, knobSize, knobSize);
    value.setBounds (0, knobSize + 14, getWidth(), 15);
}

void Knob::paint (Graphics& g)
{
    g.setColour (skin::textDim);
    g.setFont (skin::font (10.0f));
    g.drawText (name, 0, knobSize, getWidth(), 14, Justification::centred);
}

//==============================================================================
void CurveButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (selected ? Colour (0xff1e1012) : skin::panel2);
    g.fillRoundedRectangle (r, 5.0f);
    if (selected)
        for (int i = 1; i <= 3; ++i)
        {
            g.setColour (skin::red.withAlpha (0.12f));
            g.drawRoundedRectangle (r.reduced ((float) i * 2), 5.0f, 2.0f);
        }
    g.setColour (selected ? skin::red2 : (isMouseOver() ? skin::textDim : skin::line));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    pump::Params p;
    p.shape = shape;
    p.depth = 1.0f;
    const auto pts = proc.engine.getPoints();
    auto area = r.reduced (18.0f, 6.0f).withTrimmedBottom (13.0f);
    Path path;
    for (int i = 0; i <= 40; ++i)
    {
        const float x = (float) i / 40.0f;
        const float y = area.getBottom() - pump::gainAt (p, juce::jmin (x, 0.9999f), 1.0e9f, pts.data(), (int) pts.size()) * area.getHeight();
        if (i == 0) path.startNewSubPath (area.getX() + x * area.getWidth(), y);
        else        path.lineTo (area.getX() + x * area.getWidth(), y);
    }
    g.setColour (selected ? skin::red2 : skin::textDim);
    g.strokePath (path, PathStrokeType (1.8f));

    g.setColour (selected ? skin::white : skin::textDim);
    g.setFont (skin::font (9.0f));
    g.drawText (pump::shapeName (shape), r.removeFromBottom (15.0f), Justification::centred);
}

void CurveButton::mouseUp (const MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    if (auto* prm = proc.apvts.getParameter ("shape"))
    {
        proc.undo.beginNewTransaction();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) shape));
    }
}

//==============================================================================
void Meter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0b0c));
    g.fillRoundedRectangle (r, 3.0f);
    const float v = jmax (0.0f, shown);
    auto in = r.reduced (1.5f);
    if (isReduction)
    {
        const float h = jlimit (0.0f, 1.0f, 1.0f - v) * in.getHeight();
        g.setGradientFill (ColourGradient (skin::red, 0, in.getY(), Colour (0xffff5a1f), 0, in.getY() + h + 1, false));
        g.fillRect (in.withHeight (h));
    }
    else
    {
        const float db = Decibels::gainToDecibels (v, -60.0f);
        const float h = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * in.getHeight();
        ColourGradient grad (Colour (0xff22e3a5), 0, in.getBottom(), skin::red, 0, in.getY(), false);
        grad.addColour (0.7, Colour (0xffffe14d));
        g.setGradientFill (grad);
        g.fillRect (in.withTop (in.getBottom() - h));
    }
    g.setColour (skin::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

//==============================================================================
Rectangle<float> PumpDisplay::curveArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedTop (10.0f).withTrimmedBottom (6.0f);
}

Point<float> PumpDisplay::toScreen (const pump::Point& pt, int cycle, const pump::Params& p) const
{
    const auto ca = curveArea();
    const float x = (cycle + pt.x * jmax (0.1f, p.release)) / pump::Engine::kCyclesShown;
    const float gain = 1.0f - p.depth * (1.0f - std::pow (pt.y, curvePower (p)));
    return { ca.getX() + x * ca.getWidth(), ca.getBottom() - gain * ca.getHeight() };
}

pump::Point PumpDisplay::fromScreen (Point<float> pos, const pump::Params& p) const
{
    const auto ca = curveArea();
    float xc = (pos.x - ca.getX()) / ca.getWidth() * pump::Engine::kCyclesShown;
    xc -= std::floor (xc);
    float x = jlimit (0.0f, 1.0f, xc / jmax (0.1f, p.release));

    // snap to the musical grid inside one cycle
    const double s = pump::snapBeats ((int) proc.apvts.getRawParameterValue ("snap")->load()) / pump::rateBeats (p.rate);
    if (s > 1.0 / 64.0 && s < 1.0)
        x = (float) (std::round (x / s) * s);

    const float gain = jlimit (0.0f, 1.0f, (ca.getBottom() - pos.y) / ca.getHeight());
    const float g = p.depth > 0.01f ? jlimit (0.0f, 1.0f, 1.0f - (1.0f - gain) / p.depth) : gain;
    return { jlimit (0.0f, 1.0f, x), std::pow (g, 1.0f / curvePower (p)) };
}

void PumpDisplay::paint (Graphics& g)
{
    auto& e = proc.engine;
    const auto p = proc.readParams();
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 6.0f);
    const auto inner = b.reduced (8.0f);
    g.setColour (Colour (0xff070707));
    g.fillRoundedRectangle (inner, 4.0f);

    const auto ca = curveArea();
    const int cycles = pump::Engine::kCyclesShown;
    const float cw = ca.getWidth() / cycles;

    // grid: cycle lines, snap subdivisions, 25% level lines
    const double s = pump::snapBeats ((int) proc.apvts.getRawParameterValue ("snap")->load()) / pump::rateBeats (p.rate);
    const int subs = (s > 1.0 / 32.0 && s < 1.0) ? (int) std::round (1.0 / s) : 1;
    for (int c = 0; c <= cycles; ++c)
    {
        for (int k = 0; k < subs && c < cycles; ++k)
        {
            g.setColour (Colours::white.withAlpha (k == 0 ? 0.33f : 0.08f));
            g.drawVerticalLine ((int) (ca.getX() + c * cw + k * cw / subs), inner.getY(), inner.getBottom());
        }
    }
    for (int j = 1; j < 4; ++j)
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) (ca.getY() + ca.getHeight() * j / 4.0f), ca.getX(), ca.getRight());
    }

    // output audio
    const int w = (int) ca.getWidth();
    g.setColour (skin::red.withAlpha (0.33f));
    for (int x = 0; x < w; x += 3)
    {
        const int slot = jmin (pump::Engine::kDisplaySlots - 1, x * pump::Engine::kDisplaySlots / jmax (1, w));
        const float a = jmin (1.0f, e.uiPeaks[(size_t) slot].load()) * ca.getHeight() * 0.48f;
        g.fillRect (ca.getX() + x, ca.getCentreY() - a, 2.0f, a * 2.0f);
    }

    // the curve
    const auto pts = e.getPoints();
    const double cycleMs = pump::rateBeats (p.rate) * 60000.0 / e.uiBpm.load();
    Path line, fill;
    for (int x = 0; x <= w; ++x)
    {
        float xc = (float) x / w * cycles;
        xc = jmin (xc - std::floor (xc), 0.9999f);
        if (x == w) xc = 0.9999f;
        const float gain = pump::gainAt (p, xc, (float) (xc * cycleMs), pts.data(), (int) pts.size());
        const float y = ca.getBottom() - gain * ca.getHeight();
        if (x == 0) { line.startNewSubPath (ca.getX(), y); fill.startNewSubPath (ca.getX(), ca.getBottom()); }
        line.lineTo (ca.getX() + x, y);
        fill.lineTo (ca.getX() + x, y);
    }
    fill.lineTo (ca.getRight(), ca.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (skin::red2.withAlpha (0.25f), 0, ca.getY(), skin::red2.withAlpha (0.02f), 0, ca.getBottom(), false));
    g.fillPath (fill);
    for (int i = 3; i > 0; --i)
    {
        g.setColour (skin::red2.withAlpha (0.10f));
        g.strokePath (line, PathStrokeType (2.5f + i * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (skin::red2);
    g.strokePath (line, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));

    if (p.shape == pump::Draw)
        for (int c = 0; c < cycles; ++c)
            for (auto& pt : pts)
            {
                const auto sp = toScreen (pt, c, p);
                g.setColour (Colours::white.withAlpha (c == 0 ? 1.0f : 0.45f));
                g.fillEllipse (sp.x - 4.5f, sp.y - 4.5f, 9.0f, 9.0f);
            }

    // playhead and the live gain dot
    const float px = ca.getX() + e.uiPos.load() * ca.getWidth();
    for (int i = 3; i > 0; --i)
    {
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillRect (px - i * 2.0f, inner.getY(), 2.0f + i * 4.0f, inner.getHeight());
    }
    g.setColour (Colours::white);
    g.fillRect (px - 1.0f, inner.getY(), 2.0f, inner.getHeight());
    const float gy = ca.getBottom() - e.uiGain.load() * ca.getHeight();
    g.fillEllipse (px - 6.0f, gy - 6.0f, 12.0f, 12.0f);

    // info line
    static const char* trig[] = { "SYNC", "MIDI", "AUDIO" };
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    const float pre = proc.apvts.getRawParameterValue ("pregain")->load();
    String info = String (trig[p.trigger]) + " " + pump::rateNames()[p.rate] + dot + "DEPTH " + String (roundToInt (p.depth * 100)) + "%"
                + dot + "PRE-AMP " + (pre > 0 ? "+" : "") + String (pre, 1) + " dB";
    if (p.autoGain)
        info << " (AUTO +" << String (e.uiAutoDb.load(), 1) << ")";
    info << dot << String (e.uiBpm.load(), 1) << " BPM";
    if (p.shape == pump::Draw)
        info << dot << "CLICK TO ADD, DRAG TO MOVE, RIGHT-CLICK TO DELETE";
    g.setFont (skin::font (11.0f));
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), info) + 16.0f;
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (inner.getX() + 6, inner.getY() + 6, tw, 18, 4);
    g.setColour (skin::red2);
    g.drawText (info, (int) inner.getX() + 14, (int) inner.getY() + 6, (int) tw, 18, Justification::left);
}

void PumpDisplay::mouseDown (const MouseEvent& ev)
{
    const auto p = proc.readParams();
    if (p.shape != pump::Draw)
        return;

    auto pts = proc.engine.getPoints();
    int hit = -1;
    float best = 12.0f;
    for (int c = 0; c < pump::Engine::kCyclesShown; ++c)
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const float d = toScreen (pts[(size_t) i], c, p).getDistanceFrom (ev.position);
            if (d < best) { best = d; hit = i; }
        }

    if (ev.mods.isPopupMenu())
    {
        if (hit > 0 && hit < (int) pts.size() - 1)
        {
            pts.erase (pts.begin() + hit);
            proc.setPoints (pts);
        }
        return;
    }

    if (hit < 0 && (int) pts.size() < pump::kMaxPoints)
    {
        auto np = fromScreen (ev.position, p);
        np.x = jlimit (0.001f, 0.999f, np.x);
        auto it = std::upper_bound (pts.begin() + 1, pts.end() - 1, np, [] (const pump::Point& a, const pump::Point& b) { return a.x < b.x; });
        hit = (int) (pts.insert (it, np) - pts.begin());
        proc.setPoints (pts);
    }
    dragIndex = hit;
}

void PumpDisplay::mouseDrag (const MouseEvent& ev)
{
    if (dragIndex < 0)
        return;
    const auto p = proc.readParams();
    auto pts = proc.engine.getPoints();
    if (dragIndex >= (int) pts.size())
        return;

    auto np = fromScreen (ev.position, p);
    const int last = (int) pts.size() - 1;
    if (dragIndex == 0)         np.x = 0.0f;
    else if (dragIndex == last) np.x = 1.0f;
    else np.x = jlimit (pts[(size_t) dragIndex - 1].x + 0.001f, pts[(size_t) dragIndex + 1].x - 0.001f, np.x);
    pts[(size_t) dragIndex] = np;
    proc.setPoints (pts);
    repaint();
}

//==============================================================================
PitaPumpEditor::PitaPumpEditor (PitaPumpProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p),
      inMeter (p.engine.uiIn, false), grMeter (p.engine.uiGain, true), outMeter (p.engine.uiOut, false)
{
    setLookAndFeel (&lnf);
#if SHAK_HAS_LOGO
    logo = ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
#endif

    addAndMakeVisible (display);
    for (int i = 0; i < pump::NumShapes; ++i)
        addAndMakeVisible (*shapeButtons.emplace_back (std::make_unique<CurveButton> (proc, i)));

    for (auto* b : { &undoBtn, &redoBtn, &aBtn, &bBtn, &prevBtn, &nextBtn, &randomBtn, &syncBtn, &midiBtn, &audioBtn, &autoBtn })
        addAndMakeVisible (b);
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    aBtn.onClick    = [this] { proc.switchAB (false); };
    bBtn.onClick    = [this] { proc.switchAB (true); };
    prevBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() - 1); };
    nextBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() + 1); };
    randomBtn.onClick = [this] { proc.randomizeAll(); };
    randomBtn.setToggleState (true, dontSendNotification); // always drawn lit red
    randomBtn.setTooltip ("Random pump: shape, rate, depth and feel (always on the grid)");
    syncBtn.onClick  = [this] { setChoice ("trigger", pump::Sync); };
    midiBtn.onClick  = [this] { setChoice ("trigger", pump::Midi); };
    audioBtn.onClick = [this] { setChoice ("trigger", pump::Audio); };
    syncBtn.setTooltip ("Pump in time with the song");
    midiBtn.setTooltip ("Restart the curve on every MIDI note");
    audioBtn.setTooltip ("Restart the curve on every hit of the sidechain input (route your kick to it)");
    autoBtn.setClickingTogglesState (true);
    autoBtn.setTooltip ("Boost a quiet signal automatically before it pumps (up to +24 dB)");
    autoAtt = std::make_unique<ButtonAtt> (proc.apvts, "auto", autoBtn);

    const auto& list = presets::all();
    for (auto& cat : presets::categories())
    {
        PopupMenu sub;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].category == cat)
                sub.addItem ((int) i + 1, list[i].name);
        presetBox.getRootMenu()->addSubMenu (cat + "  (" + String (sub.getNumItems()) + ")", sub);
    }
    presetBox.setText (proc.getPresetLabel(), dontSendNotification);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            proc.loadPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    setupCombo (rateBox, "rate", rateAtt);
    setupCombo (snapBox, "snap", snapAtt);

    phase   = &addKnob ("phase", "PHASE", skin::white, "deg", 44);
    swing   = &addKnob ("swing", "SWING", skin::white, "%", 44);
    thresh  = &addKnob ("thresh", "THRESH", skin::white, "dB", 44);
    depth   = &addKnob ("depth", "DEPTH", skin::red2, "%", 100);
    curve   = &addKnob ("curve", "CURVE", skin::red2, "%", 48);
    attack  = &addKnob ("attack", "ATTACK", skin::red2, "ms", 48);
    release = &addKnob ("release", "RELEASE", skin::red2, "%", 48);
    smooth  = &addKnob ("smooth", "SMOOTH", skin::red2, "ms", 48);
    pregain = &addKnob ("pregain", "PRE GAIN", Colour (0xff22e3a5), "dB", 96);
    mix     = &addKnob ("mix", "MIX", skin::white, "%", 50);
    gain    = &addKnob ("gain", "GAIN", skin::white, "dB", 50);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (grMeter);
    addAndMakeVisible (outMeter);

    addMouseListener (this, true);
    setSize (kWidth, kHeight);
    startTimerHz (30);
}

PitaPumpEditor::~PitaPumpEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

Knob& PitaPumpEditor::addKnob (const String& id, const String& name, Colour c, const String& suffix, int size)
{
    auto& k = knobs.emplace_back (std::make_unique<Knob> (proc.apvts, id, name, c, suffix, size));
    addAndMakeVisible (*k);
    return *k;
}

void PitaPumpEditor::setupCombo (ComboBox& box, const String& paramId, std::unique_ptr<ComboAtt>& att)
{
    if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<ComboAtt> (proc.apvts, paramId, box);
}

void PitaPumpEditor::setChoice (const String& id, int index)
{
    if (auto* prm = proc.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
}

void PitaPumpEditor::mouseDown (const MouseEvent&)
{
    proc.undo.beginNewTransaction();
}

void PitaPumpEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    skin::drawInsetPanel (g, r.toFloat());
    g.setColour (skin::white);
    g.setFont (skin::font (12.0f));
    g.drawText (title, r.getX() + 12, r.getY() + 8, 120, 20, Justification::left);
}

void PitaPumpEditor::paint (Graphics& g)
{
    g.drawImageAt (faceplate, 0, 0);
    skin::drawLogoBadge (g, { 40.0f, 12.0f, 150.0f, 46.0f }, logo);

    const auto tf = skin::titleFont (34.0f);
    const float pitaW = GlyphArrangement::getStringWidth (tf, "PITA");
    engrave (g, "PITA", { 204, 8, (int) pitaW + 4, 38 }, tf, Colour (0xff26282c), Justification::left);
    engrave (g, "PUMP", { 204 + (int) pitaW, 8, 120, 38 }, tf, skin::red, Justification::left);
    engrave (g, String::fromUTF8 ("SIDECHAIN LFO TOOL  \xc2\xb7  BY ASHER SWISSA"), { 205, 42, 260, 14 },
             skin::font (10.5f), Colour (0xff3b3e43), Justification::left);

    drawPanel (g, triggerArea, "TRIGGER");
    drawPanel (g, shapeArea, "SHAPE");
    drawPanel (g, preampArea, "PRE-AMP");
    drawPanel (g, outputArea, "OUTPUT");

    g.setFont (skin::font (10.0f));
    g.setColour (skin::textDim);
    g.drawText ("RATE", rateBox.getBounds().translated (0, -14).withHeight (12), Justification::left);
    g.drawText ("GRID", snapBox.getBounds().translated (0, -14).withHeight (12), Justification::left);
    for (auto [m, label] : { std::pair<Meter*, const char*> { &inMeter, "IN" }, { &grMeter, "GR" }, { &outMeter, "OUT" } })
        g.drawText (label, m->getX() - 10, m->getBottom() + 3, m->getWidth() + 20, 12, Justification::centred);

    engrave (g, String::fromUTF8 ("PITA PUMP v1.0  \xc2\xb7  VST3 / AU  \xc2\xb7  MAC & PC"), { 40, 594, 400, 18 },
             skin::font (10.0f), Colour (0xff4a4d52), Justification::centredLeft);
    engrave (g, "SKAZI", { 760, 594, 200, 18 }, skin::font (11.0f), Colour (0xff4a4d52), Justification::centredRight);

    Random r (11);
    for (auto pt : { Point<float> (17, 17), Point<float> ((float) kWidth - 17, 17),
                     Point<float> (17, (float) kHeight - 17), Point<float> ((float) kWidth - 17, (float) kHeight - 17) })
        skin::drawScrew (g, pt, 7.5f, r.nextFloat() * MathConstants<float>::pi);
}

void PitaPumpEditor::resized()
{
    faceplate = skin::renderBrushedMetal (getWidth(), getHeight());

    undoBtn.setBounds (400, 22, 48, 26);
    redoBtn.setBounds (452, 22, 48, 26);
    aBtn.setBounds (506, 22, 28, 26);
    bBtn.setBounds (536, 22, 28, 26);
    prevBtn.setBounds (572, 22, 22, 26);
    presetBox.setBounds (596, 22, 240, 26);
    nextBtn.setBounds (838, 22, 22, 26);
    randomBtn.setBounds (866, 22, 100, 26);

    display.setBounds (20, 70, 960, 236);

    const float bw = (960.0f - 9 * 5.0f) / pump::NumShapes;
    for (int i = 0; i < pump::NumShapes; ++i)
        shapeButtons[(size_t) i]->setBounds (20 + (int) (i * (bw + 5.0f)), 314, (int) bw, 48);

    triggerArea = { 20, 372, 250, 190 };
    shapeArea   = { 280, 372, 290, 190 };
    preampArea  = { 580, 372, 190, 190 };
    outputArea  = { 780, 372, 200, 190 };

    auto tx = triggerArea.getX(), ty = triggerArea.getY();
    syncBtn.setBounds (tx + 86, ty + 8, 48, 22);
    midiBtn.setBounds (tx + 138, ty + 8, 44, 22);
    audioBtn.setBounds (tx + 186, ty + 8, 54, 22);
    rateBox.setBounds (tx + 12, ty + 56, 82, 24);
    snapBox.setBounds (tx + 12, ty + 112, 82, 24);
    phase->setBounds (tx + 104, ty + 36, 66, 76);
    swing->setBounds (tx + 176, ty + 36, 66, 76);
    thresh->setBounds (tx + 104, ty + 110, 66, 76);

    auto sx = shapeArea.getX(), sy = shapeArea.getY();
    depth->setBounds (sx + 8, sy + 38, 118, 130);
    curve->setBounds (sx + 132, sy + 30, 74, 78);
    attack->setBounds (sx + 208, sy + 30, 74, 78);
    release->setBounds (sx + 132, sy + 108, 74, 78);
    smooth->setBounds (sx + 208, sy + 108, 74, 78);

    auto px = preampArea.getX(), py = preampArea.getY();
    autoBtn.setBounds (px + 86, py + 8, 52, 22);
    pregain->setBounds (px + 8, py + 40, 120, 126);
    inMeter.setBounds (px + 150, py + 44, 14, 112);

    auto ox = outputArea.getX(), oy = outputArea.getY();
    mix->setBounds (ox + 4, oy + 48, 66, 80);
    gain->setBounds (ox + 70, oy + 48, 66, 80);
    grMeter.setBounds (ox + 148, oy + 44, 14, 112);
    outMeter.setBounds (ox + 172, oy + 44, 14, 112);
}

void PitaPumpEditor::timerCallback()
{
    display.repaint();
    inMeter.update();
    grMeter.update();
    outMeter.update();

    const int shape = (int) proc.apvts.getRawParameterValue ("shape")->load();
    for (int i = 0; i < (int) shapeButtons.size(); ++i)
        shapeButtons[(size_t) i]->setSelected (i == shape);

    const int trig = (int) proc.apvts.getRawParameterValue ("trigger")->load();
    syncBtn.setToggleState (trig == pump::Sync, dontSendNotification);
    midiBtn.setToggleState (trig == pump::Midi, dontSendNotification);
    audioBtn.setToggleState (trig == pump::Audio, dontSendNotification);
    phase->setAlpha (trig == pump::Sync ? 1.0f : 0.4f);
    swing->setAlpha (trig == pump::Sync ? 1.0f : 0.4f);
    thresh->setAlpha (trig == pump::Audio ? 1.0f : 0.4f);

    if (presetBox.getText() != proc.getPresetLabel() && ! presetBox.isPopupActive())
        presetBox.setText (proc.getPresetLabel(), dontSendNotification);
    aBtn.setToggleState (! proc.isOnB(), dontSendNotification);
    bBtn.setToggleState (proc.isOnB(), dontSendNotification);
}
