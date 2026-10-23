#include "PluginEditor.h"
#include "Presets.h"

#if SHAK_HAS_LOGO
 #include "BinaryData.h"
#endif

using namespace juce;

namespace
{
    constexpr int kWidth = 1000, kHeight = 720;

    using skin::engrave;

    void unpack (uint32_t v, bool& active, bool& cont, int& source, uint32_t& mask, int& len)
    {
        active = (v & 1u) != 0;
        cont   = (v & 2u) != 0;
        source = (int) ((v >> 2) & 15u) - 1;
        mask   = (v >> 6) & 31u;
        len    = (int) ((v >> 11) & 31u);
    }
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
        const String num = suffix == "dB" ? String (v, 1) : String (roundToInt (v));
        value.setText (num + (suffix.isEmpty() ? "" : " " + suffix), dontSendNotification);
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
    g.setColour (nameColour);
    g.setFont (skin::font (10.0f));
    g.drawText (name, 0, knobSize, getWidth(), 14, Justification::centred);
}

//==============================================================================
FxCell::FxCell (AudioProcessorValueTreeState& state, int fxIndex)
    : fx (fxIndex),
      knob (state, ShaksukaProcessor::weightId (fxIndex), chop::fxName (fxIndex), skin::fxColour (fxIndex), "%", 58)
{
    knob.nameColour = skin::fxColour (fx);
    toggle.setColour (ToggleButton::tickColourId, skin::fxColour (fx));
    addAndMakeVisible (toggle);
    addAndMakeVisible (knob);
    toggleAtt = std::make_unique<ButtonAtt> (state, ShaksukaProcessor::enableId (fx), toggle);
}

void FxCell::resized()
{
    const int top = (getHeight() - (24 + 58 + 30)) / 2;
    toggle.setBounds (getWidth() / 2 - 18, top, 36, 16);
    knob.setBounds (0, top + 24, getWidth(), 58 + 30);
}

void FxCell::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = skin::fxColour (fx);
    if (hot)
    {
        g.setColour (Colour (0xff1e1012));
        g.fillRect (r);
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (c.withAlpha (0.10f - i * 0.015f));
            g.drawRect (r.reduced ((float) i), 1.5f);
        }
        g.setColour (c);
        g.drawRect (r, 1.0f);
    }
}

//==============================================================================
void WaveView::paint (Graphics& g)
{
    auto& e = proc.engine;
    auto b = getLocalBounds().toFloat();
    skin::drawInsetPanel (g, b, 6.0f);

    auto inner = b.reduced (6.0f);
    auto strip = inner.removeFromBottom (34.0f);
    inner.removeFromBottom (6.0f);
    auto wr = inner;

    const int n = jlimit (1, chop::kMaxSteps, e.uiNumSteps.load());
    const float stepW = wr.getWidth() / (float) n;

    bool active, cont; int src, len; uint32_t mask;
    std::vector<Colour> cols ((size_t) n, Colours::transparentBlack);
    std::vector<bool> starts ((size_t) n, true);
    Colour run = Colours::transparentBlack;
    for (int i = 0; i < n; ++i)
    {
        unpack (e.uiPattern[(size_t) i].load(), active, cont, src, mask, len);
        if (cont) { cols[(size_t) i] = run; starts[(size_t) i] = false; continue; }
        run = ! active ? Colours::transparentBlack
                       : (src >= 0 ? skin::fxColour (src) : Colour (0xff8b8b92));
        cols[(size_t) i] = run;
    }

    g.saveState();
    g.reduceClipRegion (wr.toNearestInt());
    g.setColour (Colour (0xff070707));
    g.fillRect (wr);

    for (int i = 0; i < n; ++i)
        if (! cols[(size_t) i].isTransparent())
        {
            g.setColour (cols[(size_t) i].withAlpha (0.14f));
            g.fillRect (wr.getX() + i * stepW, wr.getY(), stepW, wr.getHeight());
        }

    // waveform
    const float mid = wr.getCentreY();
    {
        ColourGradient grad (Colour (0xffff3b2f), 0, wr.getY(), Colour (0xffff3b2f), 0, wr.getBottom(), false);
        grad.addColour (0.5, skin::red);
        g.setGradientFill (grad);
    }
    const int w = (int) wr.getWidth();
    for (int x = 0; x < w; x += 2)
    {
        const int slot = jmin (chop::Engine::kDisplaySlots - 1, (int) ((float) x / w * chop::Engine::kDisplaySlots));
        const float a = jmin (1.0f, e.uiPeaks[(size_t) slot].load()) * wr.getHeight() * 0.45f;
        g.fillRect (wr.getX() + x, mid - a, 1.5f, jmax (1.0f, a * 2.0f));
    }

    // chop markers and slice labels
    g.setFont (skin::font (10.0f));
    for (int i = 0; i <= n; ++i)
    {
        if (i < n && ! starts[(size_t) i]) continue;
        const float x = wr.getX() + i * stepW;
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, wr.getY(), wr.getBottom());
        Path tri;
        tri.addTriangle (x - 5, wr.getY(), x + 5, wr.getY(), x, wr.getY() + 8);
        g.setColour (Colours::white);
        g.fillPath (tri);

        if (i < n)
        {
            unpack (e.uiPattern[(size_t) i].load(), active, cont, src, mask, len);
            if (active && len * stepW >= 24.0f)
            {
                g.setColour (cols[(size_t) i]);
                const String label = src >= 0 ? chop::fxShortName (src) : chop::fxShortName (chop::kNumSources + (int) std::log2 ((float) (mask & (~mask + 1))));
                g.drawText (label, (int) x + 4, (int) wr.getBottom() - 16, 40, 12, Justification::left);
            }
        }
    }

    // playhead
    const float px = wr.getX() + e.uiPhase.load() * wr.getWidth();
    for (int i = 3; i > 0; --i)
    {
        g.setColour (Colours::white.withAlpha (0.08f));
        g.fillRect (px - i * 2.0f, wr.getY(), 2.0f + i * 4.0f, wr.getHeight());
    }
    g.setColour (Colours::white);
    g.fillRect (px - 1.0f, wr.getY(), 2.0f, wr.getHeight());

    // info line
    auto& s = proc.apvts;
    const bool free = s.getRawParameterValue ("free")->load() > 0.5f;
    static const char* grooves[] = { "STRAIGHT", "SWING", "SHUFFLE", "DRUNK", "PUSH" };
    const int groove = (int) s.getRawParameterValue ("groove")->load();
    String info = free ? "FREE " + String (roundToInt (s.getRawParameterValue ("freems")->load())) + " MS"
                       : "SYNC " + chop::gridNames()[(int) s.getRawParameterValue ("grid")->load()];
    const String dot = String::fromUTF8 ("  \xc2\xb7  ");
    info << dot << grooves[groove];
    if (groove == chop::Swing || groove == chop::Drunk || groove == chop::Push)
        info << " " << roundToInt (s.getRawParameterValue ("swing")->load()) << "%";
    info << dot << String (e.uiBpm.load(), 1) << " BPM";
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (wr.getX() + 6, wr.getY() + 12, 320, 18, 4);
    g.setFont (skin::font (11.0f));
    g.setColour (skin::textDim);
    g.drawText ("OUTPUT" + dot, (int) wr.getX() + 12, (int) wr.getY() + 12, 80, 18, Justification::left);
    g.setColour (skin::red2);
    g.drawText (info, (int) wr.getX() + 72, (int) wr.getY() + 12, 250, 18, Justification::left);
    g.restoreState();

    // pattern strip
    const int cur = e.uiStep.load();
    const float cw = strip.getWidth() / (float) n;
    const float gap = cw > 6.0f ? 3.0f : 1.0f;
    for (int i = 0; i < n; ++i)
    {
        unpack (e.uiPattern[(size_t) i].load(), active, cont, src, mask, len);
        auto cell = Rectangle<float> (strip.getX() + i * cw, strip.getY(), cw - gap, strip.getHeight());
        const auto col = cols[(size_t) i];
        g.setColour (col.isTransparent() ? Colour (0xff1a1a1d) : col);
        g.fillRoundedRectangle (cell, 3.0f);

        if (starts[(size_t) i] && active && cw >= 22.0f && src >= 0)
        {
            g.setColour (Colours::black);
            g.setFont (skin::font (9.5f));
            g.drawText (chop::fxShortName (src), cell.withTrimmedBottom (8), Justification::centred);
        }
        // processor dots along the bottom
        if (starts[(size_t) i] && mask != 0 && cw >= 12.0f)
        {
            const float d = jmin (5.0f, (cell.getWidth() - 4) / 5.0f);
            for (int k = 0; k < 5; ++k)
                if (mask & (1u << k))
                {
                    g.setColour (src >= 0 ? Colours::black.withAlpha (0.7f) : skin::fxColour (chop::kNumSources + k));
                    g.fillEllipse (cell.getX() + 2 + k * d, cell.getBottom() - d - 2, d - 1, d - 1);
                }
        }
        if (i == cur)
        {
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawRoundedRectangle (cell.expanded (2.0f), 4.0f, 3.0f);
            g.setColour (Colours::white);
            g.drawRoundedRectangle (cell, 3.0f, 2.0f);
        }
    }
}

//==============================================================================
ShaksukaEditor::ShaksukaEditor (ShaksukaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p)
{
    setLookAndFeel (&lnf);

#if SHAK_HAS_LOGO
    logo = ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
#endif

    addAndMakeVisible (wave);

    for (auto* b : { &undoBtn, &redoBtn, &aBtn, &bBtn, &diceBtn, &syncBtn, &freeBtn, &prevBtn, &nextBtn, &randomBtn })
        addAndMakeVisible (b);
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    aBtn.onClick    = [this] { proc.switchAB (false); };
    bBtn.onClick    = [this] { proc.switchAB (true); };
    diceBtn.onClick = [this] { proc.randomizeSeed(); };
    prevBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() - 1); };
    nextBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() + 1); };
    randomBtn.onClick = [this] { proc.randomizeAll(); };
    diceBtn.setTooltip ("New random pattern, same settings");
    randomBtn.setTooltip ("Random everything: grid, groove, chaos and effects (always on the grid)");
    randomBtn.setToggleState (true, dontSendNotification); // always drawn lit red

    auto setFree = [this] (bool f)
    {
        if (auto* prm = proc.apvts.getParameter ("free"))
            prm->setValueNotifyingHost (f ? 1.0f : 0.0f);
    };
    syncBtn.onClick = [setFree] { setFree (false); };
    freeBtn.onClick = [setFree] { setFree (true); };

    // one submenu per category; item IDs are preset index + 1
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

    setupCombo (gridBox, "grid", gridAtt);
    setupCombo (barsBox, "bars", barsAtt);
    setupCombo (grooveBox, "groove", grooveAtt);

    seed.setSliderStyle (Slider::LinearBar);
    seed.setColour (Slider::trackColourId, skin::red.withAlpha (0.35f));
    seed.setColour (Slider::backgroundColourId, Colour (0xff0f0f10));
    seed.setColour (Slider::textBoxTextColourId, skin::white);
    seedAtt = std::make_unique<SliderAtt> (proc.apvts, "seed", seed);
    seed.textFromValueFunction = [] (double v) { return "#" + String ((int) v).paddedLeft ('0', 4); };
    seed.setTooltip ("Pattern seed: drag to pick a pattern");
    seed.updateText();
    addAndMakeVisible (seed);

    freeMs   = &addKnob ("freems", "FREE MS", skin::red, "ms");
    lenVar   = &addKnob ("lenvar", "LENGTH VAR", skin::red, "%");
    swing    = &addKnob ("swing", "SWING", skin::white, "%");
    humanize = &addKnob ("humanize", "HUMANIZE", skin::white, "%");
    chaos    = &addKnob ("chaos", "CHAOS", skin::red2, "%", 110);
    stack    = &addKnob ("stack", "STACK", skin::red2, "FX");
    evolve   = &addKnob ("evolve", "EVOLVE", skin::red2, "%");
    mix      = &addKnob ("mix", "MIX", skin::white, "%");
    gain     = &addKnob ("gain", "GAIN", skin::white, "dB");

    for (int i = 0; i < chop::NumFx; ++i)
        addAndMakeVisible (*fxCells.emplace_back (std::make_unique<FxCell> (proc.apvts, i)));

    addMouseListener (this, true);
    setSize (kWidth, kHeight);
    startTimerHz (30);
}

ShaksukaEditor::~ShaksukaEditor()
{
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

Knob& ShaksukaEditor::addKnob (const String& id, const String& name, Colour c, const String& suffix, int size)
{
    auto& k = knobs.emplace_back (std::make_unique<Knob> (proc.apvts, id, name, c, suffix, size));
    addAndMakeVisible (*k);
    return *k;
}

void ShaksukaEditor::setupCombo (ComboBox& box, const String& paramId, std::unique_ptr<ComboAtt>& att)
{
    if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        box.addItemList (c->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<ComboAtt> (proc.apvts, paramId, box);
}

void ShaksukaEditor::mouseDown (const MouseEvent&)
{
    proc.undo.beginNewTransaction();
}

//==============================================================================
void ShaksukaEditor::renderFaceplate()
{
    faceplate = skin::renderBrushedMetal (getWidth(), getHeight());
}

void ShaksukaEditor::drawLogo (Graphics& g, Rectangle<float> r)
{
    skin::drawLogoBadge (g, r, logo);
}

void ShaksukaEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    skin::drawInsetPanel (g, r.toFloat());
    g.setColour (skin::white);
    g.setFont (skin::font (12.0f));
    g.drawText (title, r.getX() + 12, r.getY() + 8, 120, 20, Justification::left);
}

void ShaksukaEditor::paint (Graphics& g)
{
    g.drawImageAt (faceplate, 0, 0);

    drawLogo (g, { 40.0f, 12.0f, 150.0f, 46.0f });

    const auto tf = skin::titleFont (34.0f);
    const float shakW = GlyphArrangement::getStringWidth (tf, "SHAK");
    engrave (g, "SHAK", { 204, 8, (int) shakW + 4, 38 }, tf, Colour (0xff26282c), Justification::left);
    engrave (g, "SUKA", { 204 + (int) shakW, 8, 120, 38 }, tf, skin::red, Justification::left);
    engrave (g, "BY ASHER SWISSA", { 205, 42, 200, 14 }, skin::font (10.5f), Colour (0xff3b3e43), Justification::left);

    drawPanel (g, slicingArea, "SLICING");
    drawPanel (g, grooveArea, "GROOVE");
    drawPanel (g, chaosArea, "CHAOS");
    drawPanel (g, outputArea, "OUTPUT");
    skin::drawInsetPanel (g, rackArea.toFloat());

    g.setFont (skin::font (10.0f));
    g.setColour (skin::textDim);
    g.drawText ("GRID", gridBox.getBounds().translated (0, -14).withHeight (12), Justification::left);
    g.drawText ("BARS", barsBox.getBounds().translated (0, -14).withHeight (12), Justification::left);
    g.drawText ("TYPE", grooveBox.getBounds().translated (0, -14).withHeight (12), Justification::left);

    // thin dividers between the effect cells
    g.setColour (skin::line);
    for (size_t i = 1; i < fxCells.size(); ++i)
        g.drawVerticalLine (fxCells[i]->getX() - 1, (float) rackArea.getY() + 6, (float) rackArea.getBottom() - 6);

    engrave (g, String::fromUTF8 ("SHAKSUKA v1.0  \xc2\xb7  VST3 / AU  \xc2\xb7  MAC & PC"), { 40, 694, 400, 18 },
             skin::font (10.0f), Colour (0xff4a4d52), Justification::centredLeft);
    engrave (g, "SKAZI", { 760, 694, 200, 18 }, skin::font (11.0f), Colour (0xff4a4d52), Justification::centredRight);

    Random r (7);
    for (auto pt : { Point<float> (17, 17), Point<float> ((float) kWidth - 17, 17),
                     Point<float> (17, (float) kHeight - 17), Point<float> ((float) kWidth - 17, (float) kHeight - 17) })
        skin::drawScrew (g, pt, 7.5f, r.nextFloat() * MathConstants<float>::pi);
}

void ShaksukaEditor::resized()
{
    renderFaceplate();

    // header
    undoBtn.setBounds (372, 22, 48, 26);
    redoBtn.setBounds (424, 22, 48, 26);
    aBtn.setBounds (478, 22, 28, 26);
    bBtn.setBounds (508, 22, 28, 26);
    prevBtn.setBounds (544, 22, 22, 26);
    presetBox.setBounds (568, 22, 178, 26);
    nextBtn.setBounds (748, 22, 22, 26);
    randomBtn.setBounds (776, 22, 72, 26);
    diceBtn.setBounds (852, 22, 46, 26);
    seed.setBounds (902, 22, 64, 26);

    wave.setBounds (20, 70, 960, 232);

    slicingArea = { 20, 312, 264, 188 };
    grooveArea  = { 294, 312, 211, 188 };
    chaosArea   = { 515, 312, 285, 188 };
    outputArea  = { 810, 312, 170, 188 };
    rackArea    = { 20, 510, 960, 178 };

    auto sx = slicingArea.getX(), sy = slicingArea.getY();
    syncBtn.setBounds (sx + 90, sy + 8, 52, 22);
    freeBtn.setBounds (sx + 146, sy + 8, 52, 22);
    gridBox.setBounds (sx + 12, sy + 56, 84, 24);
    barsBox.setBounds (sx + 12, sy + 112, 84, 24);
    freeMs->setBounds (sx + 108, sy + 50, 70, 84);
    lenVar->setBounds (sx + 184, sy + 50, 70, 84);

    auto gx = grooveArea.getX(), gy = grooveArea.getY();
    grooveBox.setBounds (gx + 12, gy + 56, 92, 24);
    humanize->setBounds (gx + 22, gy + 92, 70, 84);
    swing->setBounds (gx + 122, gy + 50, 70, 84);

    auto cx = chaosArea.getX(), cy = chaosArea.getY();
    chaos->setBounds (cx + 14, cy + 30, 120, 140);
    stack->setBounds (cx + 140, cy + 56, 66, 84);
    evolve->setBounds (cx + 210, cy + 56, 66, 84);

    auto ox = outputArea.getX(), oy = outputArea.getY();
    mix->setBounds (ox + 12, oy + 56, 70, 84);
    gain->setBounds (ox + 88, oy + 56, 70, 84);

    auto rack = rackArea.reduced (4, 6);
    const float cw = rack.getWidth() / (float) fxCells.size();
    for (size_t i = 0; i < fxCells.size(); ++i)
        fxCells[i]->setBounds (rack.getX() + (int) (i * cw), rack.getY(), (int) cw - 1, rack.getHeight());
}

void ShaksukaEditor::timerCallback()
{
    wave.repaint();

    const int src = proc.engine.uiSource.load();
    const uint32_t mask = proc.engine.uiMask.load();
    for (int i = 0; i < (int) fxCells.size(); ++i)
        fxCells[(size_t) i]->setHot (i < chop::kNumSources ? src == i : (mask & (1u << (i - chop::kNumSources))) != 0);

    const bool free = proc.apvts.getRawParameterValue ("free")->load() > 0.5f;
    syncBtn.setToggleState (! free, dontSendNotification);
    freeBtn.setToggleState (free, dontSendNotification);
    freeMs->setAlpha (free ? 1.0f : 0.4f);
    gridBox.setAlpha (free ? 0.4f : 1.0f);
    if (presetBox.getText() != proc.getPresetLabel() && ! presetBox.isPopupActive())
        presetBox.setText (proc.getPresetLabel(), dontSendNotification);
    aBtn.setToggleState (! proc.isOnB(), dontSendNotification);
    bBtn.setToggleState (proc.isOnB(), dontSendNotification);
}
