#include "PluginEditor.h"
#include "Presets.h"
#if SKAZI_HAS_LOGO
 #include "BinaryData.h"
#endif

using namespace juce;
using namespace bully;

namespace bui
{
Font font (float size, bool bold) { return Font (FontOptions (size, bold ? Font::bold : Font::plain)); }

namespace
{
    const Colour glass { 0xd40a0618 }, glassHi { 0xe0140c2c }, line { 0xff3a2c66 };

    ColourGradient rainbow (float x0, float y0, float x1, float y1)
    {
        ColourGradient g (Colour (0xffff00c8), x0, y0, Colour (0xff7a2bff), x1, y1, false);
        g.addColour (0.25, Colour (0xffffe600));
        g.addColour (0.5, Colour (0xff39ff7a));
        g.addColour (0.75, Colour (0xff00e5ff));
        return g;
    }

    String fmt (double v, const String& suffix)
    {
        if (suffix == "Hz") return v >= 1000.0 ? String (v / 1000.0, v >= 10000.0 ? 1 : 2) + "k" : String (v, v < 10 ? 2 : 0) + " Hz";
        if (suffix == "ms") return v >= 1000.0 ? String (v / 1000.0, 2) + " s" : String (v, v < 10.0 ? 1 : 0) + " ms";
        if (suffix == "dB") return (v > 0.05 ? "+" : "") + String (v, 1) + " dB";
        if (suffix == "ct") return (v > 0.0 ? "+" : "") + String (roundToInt (v)) + " ct";
        if (suffix == "%") return String (roundToInt (v)) + "%";
        return String (roundToInt (v));
    }

    void glowPath (Graphics& g, const Path& p, Colour c, float w)
    {
        g.setColour (c.withAlpha (0.18f)); g.strokePath (p, PathStrokeType (w + 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.35f)); g.strokePath (p, PathStrokeType (w + 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c); g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void screen (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colour (0xff05030c));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::white.withAlpha (0.04f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8), r.getY() + 2, r.getBottom() - 2);
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4), r.getX() + 2, r.getRight() - 2);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }
}

//==============================================================================
Look::Look()
{
    setColour (ComboBox::backgroundColourId, Colour (0xff120a26));
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::arrowColourId, cyan);
    setColour (ComboBox::outlineColourId, line);
    setColour (PopupMenu::backgroundColourId, Colour (0xff120a26));
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::highlightedBackgroundColourId, pink.withAlpha (0.8f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::textColourOffId, text);
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (TooltipWindow::backgroundColourId, Colour (0xf0120a26));
    setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe9edf2));
    setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff151a20));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff8a93a0));
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, cyan.withAlpha (0.85f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, pink.withAlpha (0.3f));
    setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.3f));
}

void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const float r = jmin (w, h) * 0.5f - 4.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float ang = start + pos * (end - start);
    auto arc = [&] (float a0, float a1, float rad) { Path p; p.addCentredArc (cx, cy, rad, rad, 0.0f, a0, a1, true); return p; };

    // track + neon value ring
    g.setColour (accent.withAlpha (0.18f));
    g.strokePath (arc (start, end, r), PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    if (pos > 0.001f)
    {
        const auto p = arc (start, ang, r);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (ColourGradient (accent, cx - r, cy + r, Colours::white, cx + r, cy - r, false));
        g.strokePath (p, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    // modulation range (white) and the live modulated position
    const float lo = (float) s.getProperties().getWithDefault ("modLo", -1.0), hi = (float) s.getProperties().getWithDefault ("modHi", -1.0);
    if (lo >= 0.0f && hi > lo + 0.001f)
    {
        const auto p = arc (start + lo * (end - start), start + hi * (end - start), r + 3.6f);
        g.setColour (cyan.withAlpha (0.35f));
        g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white);
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    const float now = (float) s.getProperties().getWithDefault ("modNow", -1.0);
    if (now >= 0.0f)
    {
        const float a = start + jlimit (0.0f, 1.0f, now) * (end - start);
        const auto pt = Point<float> (cx + (r + 3.6f) * std::sin (a), cy - (r + 3.6f) * std::cos (a));
        g.setColour (cyan.withAlpha (0.5f)); g.fillEllipse (pt.x - 4, pt.y - 4, 8, 8);
        g.setColour (Colours::white); g.fillEllipse (pt.x - 2.2f, pt.y - 2.2f, 4.4f, 4.4f);
    }

    // polished metal knob: shadow, knurled edge, silver dome, neon rim, pointer
    const float kr = r * 0.72f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - kr - 1, cy - kr + 1.5f, kr * 2 + 2, kr * 2 + 2);
    g.setColour (Colour (0xff9aa0a8));
    g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (Colour (0xff3a3e44));
    const int teeth = jmax (18, (int) (kr * 2.4f));
    for (int i = 0; i < teeth; ++i)
    {
        const float a = MathConstants<float>::twoPi * i / teeth;
        g.drawLine (cx + (kr - 2.2f) * std::sin (a), cy - (kr - 2.2f) * std::cos (a), cx + kr * std::sin (a), cy - kr * std::cos (a), 1.1f);
    }
    const float dr = kr - 2.3f;
    ColourGradient dome (Colours::white, cx - dr * 0.35f, cy - dr * 0.45f, Colour (0xff4b5058), cx + dr * 0.6f, cy + dr * 0.9f, true);
    dome.addColour (0.35, Colour (0xffd9dde2));
    dome.addColour (0.7, Colour (0xff8f959d));
    g.setGradientFill (dome);
    g.fillEllipse (cx - dr, cy - dr, dr * 2, dr * 2);
    g.setColour (accent.withAlpha (0.35f));
    g.drawEllipse (cx - kr - 1.8f, cy - kr - 1.8f, kr * 2 + 3.6f, kr * 2 + 3.6f, 2.4f);
    g.setColour (accent);
    g.drawEllipse (cx - kr - 1.2f, cy - kr - 1.2f, kr * 2 + 2.4f, kr * 2 + 2.4f, 0.9f);
    g.setColour (Colour (0xff1a1c20));
    g.drawLine (cx + kr * 0.15f * std::sin (ang), cy - kr * 0.15f * std::cos (ang), cx + (kr - 2.8f) * std::sin (ang), cy - (kr - 2.8f) * std::cos (ang), jmax (1.8f, kr * 0.16f));
}

void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    auto r = Rectangle<float> ((float) x, y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (Colour (0xff05030c));
    g.fillRoundedRectangle (r, 3.0f);
    const float mid = r.getCentreX(), px = jlimit (r.getX(), r.getRight(), pos);
    const auto c = s.findColour (Slider::trackColourId);
    g.setColour (c);
    g.fillRoundedRectangle (Rectangle<float> (jmin (mid, px), r.getY(), std::abs (px - mid), r.getHeight()), 3.0f);
    g.setColour (Colours::white);
    g.fillEllipse (px - 6, r.getCentreY() - 6, 12, 12);
    g.setColour (c);
    g.drawEllipse (px - 6, r.getCentreY() - 6, 12, 12, 1.5f);
}

void Look::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Colour (0xff120a26));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? pink : line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path tri;
    const float ax = w - 10.0f, ay = h * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (cyan);
    g.fillPath (tri);
}

void Look::positionComboBoxText (ComboBox& box, Label& l)
{
    l.setBounds (4, 0, box.getWidth() - 18, box.getHeight());
    l.setFont (font (jmin (12.0f, box.getHeight() * 0.62f)));
    l.setColour (Label::textColourId, text);
}

void Look::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto c = b.findColour (TextButton::buttonOnColourId);
    if (b.getToggleState())
    {
        g.setColour (c.withAlpha (0.3f));
        g.fillRoundedRectangle (r.expanded (1.5f), 5.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (down ? Colour (0xff24183f) : Colour (0xff150c2c));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (over ? c : line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void Look::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? Colours::black : text);
    g.setFont (font (jmin (12.5f, b.getHeight() * 0.55f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 0), Justification::centred, 1);
}

void Look::drawTooltip (Graphics& g, const String& t, int w, int h)
{
    g.setColour (Colour (0xf0120a26));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
    g.setColour (pink);
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 4.0f, 1.0f);
    g.setColour (text);
    g.setFont (font (12.0f, false));
    g.drawFittedText (t, 6, 2, w - 12, h - 4, Justification::centredLeft, 3);
}

//==============================================================================
void ModSlider::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onPopup) onPopup(); return; }
    if (e.mods.isAltDown() && onModDrag) { modDrag = true; lastY = e.position.y; return; }
    Slider::mouseDown (e);
}
void ModSlider::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (modDrag) { onModDrag (lastY - e.position.y); lastY = e.position.y; return; }
    Slider::mouseDrag (e);
}
void ModSlider::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (modDrag) { modDrag = false; return; }
    Slider::mouseUp (e);
}

//==============================================================================
Knob::Knob (BullyProcessor& p, const String& id, const String& n, Colour accent, int size)
    : paramId (id), proc (p), name (n), knobSize (size)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    if (auto* prm = p.apvts.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        slider.setTooltip (prm->getName (64) + "  -  drag a mod source here, ALT-drag sets its amount, right-click removes");
    }
    dest = destForParam (id);
    exactDest = dest > 0 && destInfo()[(size_t) dest].second == id.toStdString();
    slider.onValueChange = [this] { repaint(); };
    slider.onPopup = [this] { showModMenu(); };
    slider.onModDrag = [this] (float dy)
    {
        auto mods = proc.modsFor (paramId);
        if (mods.empty()) return;
        const int s = mods.back().first;
        const auto id2 = "x" + String (s + 1) + "amt";
        proc.setParam (id2, jlimit (-100.0f, 100.0f, mods.back().second + dy * 0.5f));
    };
}

void Knob::resized()
{
    slider.setBounds ((getWidth() - knobSize - 8) / 2, 0, knobSize + 8, knobSize + 8);
}

String Knob::valueText() const
{
    auto* prm = proc.apvts.getParameter (paramId);
    if (auto* c = dynamic_cast<AudioParameterChoice*> (prm)) return c->getCurrentChoiceName();
    auto* rp = dynamic_cast<RangedAudioParameter*> (prm);
    const double v = slider.getValue();
    return fmt (v, rp != nullptr ? rp->getLabel() : String());
}

void Knob::paint (Graphics& g)
{
    const int top = knobSize + 7;
    g.setFont (font (knobSize >= 40 ? 10.5f : 9.0f));
    g.setColour (dim);
    g.drawText (name, 0, top - 2, getWidth(), 11, Justification::centred);
    g.setColour (text);
    g.drawText (valueText(), 0, top + 8, getWidth(), 11, Justification::centred);
}

void Knob::paintOverChildren (Graphics& g)
{
    if (! hot) return;
    g.setColour (cyan.withAlpha (0.25f));
    g.fillEllipse (slider.getBounds().toFloat().reduced (2));
    g.setColour (cyan);
    g.drawEllipse (slider.getBounds().toFloat().reduced (2), 2.0f);
}

void Knob::refreshMods()
{
    float lo = -1, hi = -1, now = -1;
    if (exactDest)
    {
        const auto mods = proc.modsFor (paramId);
        if (! mods.empty())
        {
            const float base = (float) slider.valueToProportionOfLength (slider.getValue());
            float up = 0, down = 0;
            for (auto& m : mods) (m.second >= 0 ? up : down) += m.second * 0.01f;
            lo = jlimit (0.0f, 1.0f, base + down);
            hi = jlimit (0.0f, 1.0f, base + up);
            now = jlimit (0.0f, 1.0f, base + proc.engine.uiDestMod[(size_t) dest].load());
        }
    }
    if (lo != shownLo || hi != shownHi || std::abs (now - shownNow) > 0.002f)
    {
        shownLo = lo; shownHi = hi; shownNow = now;
        slider.getProperties().set ("modLo", lo);
        slider.getProperties().set ("modHi", hi);
        slider.getProperties().set ("modNow", now);
        slider.repaint();
    }
}

void Knob::itemDropped (const SourceDetails& d)
{
    hot = false;
    repaint();
    const int src = d.description.toString().fromFirstOccurrenceOf ("src:", false, false).getIntValue();
    if (proc.addModulation (src, paramId) < 0 && dest > 0)
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "BULLY", "All 16 matrix slots are in use. Free one on the MATRIX page.");
}

void Knob::showModMenu()
{
    PopupMenu m;
    const auto mods = proc.modsFor (paramId);
    m.addSectionHeader (name + " modulation");
    if (mods.empty()) m.addItem (-1, "Drag an LFO / ENV / CHAOS / MACRO tag onto this knob", false, false);
    for (auto& md : mods)
    {
        const int src = (int) proc.v (("x" + String (md.first + 1) + "src").toRawUTF8());
        m.addItem (md.first + 1, "Remove " + srcNames()[src] + "  (" + String (roundToInt (md.second)) + "%)");
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&slider), [this] (int r) { if (r > 0) proc.removeModulation (r - 1); });
}

//==============================================================================
SourceTag::SourceTag (int src, String l, Colour c) : source (src), label (std::move (l)), colour (c)
{
    setTooltip ("Click to show it, drag onto any knob to modulate it");
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void SourceTag::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (selected ? colour.withAlpha (0.9f) : Colour (0xff150c2c));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (selected ? colour : line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (selected ? Colours::black : text);
    g.setFont (font (jmin (11.0f, getHeight() * 0.6f)));
    g.drawText (label, r.withTrimmedRight (10).toNearestInt(), Justification::centred);
    const auto dot = Point<float> (r.getRight() - 7.0f, r.getCentreY());
    g.setColour (colour.withAlpha (0.25f + 0.5f * level));
    g.fillEllipse (dot.x - 4.5f, dot.y - 4.5f, 9, 9);
    g.setColour (selected ? Colours::white : colour);
    g.fillEllipse (dot.x - 2.5f, dot.y - 2.5f, 5, 5);
}

void SourceTag::mouseDown (const MouseEvent&) { dragging = false; }
void SourceTag::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        Image img (Image::ARGB, getWidth(), getHeight(), true);
        Graphics g (img);
        paint (g);
        c->startDragging ("src:" + String (source), this, ScaledImage (img));
    }
}
void SourceTag::mouseUp (const MouseEvent&) { if (! dragging && onClick) onClick(); }

//==============================================================================
ParamCombo::ParamCombo (BullyProcessor& p, const String& id)
{
    if (auto* c = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (id))) addItemList (c->choices, 1);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id, *this);
}

ParamButton::ParamButton (BullyProcessor& p, const String& id, const String& t, Colour on) : TextButton (t)
{
    setClickingTogglesState (true);
    setColour (TextButton::buttonOnColourId, on);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id, *this);
    if (auto* prm = p.apvts.getParameter (id)) setTooltip (prm->getName (64));
}

ChoiceButtons::ChoiceButtons (BullyProcessor& p, const String& i, Colour c, StringArray labels) : proc (p), id (i), colour (c)
{
    if (labels.isEmpty())
        if (auto* ch = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (id))) labels = ch->choices;
    items = labels;
}

void ChoiceButtons::update()
{
    const int s = (int) proc.v (id.toRawUTF8());
    if (s != sel) { sel = s; repaint(); }
}

void ChoiceButtons::paint (Graphics& g)
{
    const int n = items.size();
    if (n == 0) return;
    const float gap = 3.0f, w = (getWidth() - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
    {
        auto r = Rectangle<float> (i * (w + gap), 0, w, (float) getHeight()).reduced (0.5f);
        const bool on = i == sel;
        g.setColour (on ? colour : Colour (0xff150c2c));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? colour : line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (on ? Colours::black : text);
        g.setFont (font (jmin (11.0f, getHeight() * 0.55f)));
        g.drawFittedText (items[i], r.toNearestInt().reduced (2, 0), Justification::centred, 1);
    }
}

void ChoiceButtons::mouseUp (const MouseEvent& e)
{
    if (items.isEmpty() || ! getLocalBounds().contains (e.getPosition())) return;
    const int i = jlimit (0, items.size() - 1, e.x * items.size() / jmax (1, getWidth()));
    proc.undo.beginNewTransaction();
    proc.setParam (id, (float) i);
}

//==============================================================================
void WaveView::update()
{
    const auto& p = Ix::get().osc[o];
    const float pos = proc.v (paramDefs()[(size_t) p.pos].id) + proc.engine.uiDestMod[(size_t) (DOsc0 + o * kOscDests + OPos)].load() * 100.0f;
    const int tab = (int) proc.v (paramDefs()[(size_t) p.tab].id);
    const float w[4] = { proc.v (paramDefs()[(size_t) p.w1t].id), proc.v (paramDefs()[(size_t) p.w1].id),
                         proc.v (paramDefs()[(size_t) p.w2t].id), proc.v (paramDefs()[(size_t) p.on].id) };
    if (std::abs (pos - lastPos) > 0.2f || tab != lastTab || std::memcmp (w, lastW, sizeof (w)) != 0)
    {
        lastPos = pos; lastTab = tab; std::memcpy (lastW, w, sizeof (w));
        repaint();
    }
}

void WaveView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    screen (g, b);
    const bool on = lastW[3] > 0.5f;
    const auto& bank = WaveBank::get();
    const int frames = 14;
    const float depth = 26.0f, w = b.getWidth() - depth - 16.0f, h = b.getHeight() * 0.32f;
    const float pos = jlimit (0.0f, 1.0f, lastPos * 0.01f);
    for (int f = frames - 1; f >= 0; --f)
    {
        const float t = (float) f / (frames - 1);
        const float x0 = b.getX() + 8.0f + depth * t, y0 = b.getCentreY() + 10.0f - depth * 0.9f * t;
        Path p;
        for (int i = 0; i <= 64; ++i)
        {
            const float x = i / 64.0f;
            const float y = y0 - bank.preview (jmax (0, lastTab), t, x) * h;
            if (i == 0) p.startNewSubPath (x0 + x * w, y); else p.lineTo (x0 + x * w, y);
        }
        const float hue = std::fmod (0.83f + t * 0.6f, 1.0f);
        g.setColour (Colour::fromHSV (hue, 0.9f, 1.0f, 1.0f).withAlpha (on ? 0.22f + 0.3f * (1 - t) : 0.08f));
        g.strokePath (p, PathStrokeType (1.1f));
    }
    // the playing frame
    const float x0 = b.getX() + 8.0f + depth * pos, y0 = b.getCentreY() + 10.0f - depth * 0.9f * pos;
    Path p;
    for (int i = 0; i <= 128; ++i)
    {
        const float x = i / 128.0f;
        const float y = y0 - bank.preview (jmax (0, lastTab), pos, x) * h;
        if (i == 0) p.startNewSubPath (x0 + x * w, y); else p.lineTo (x0 + x * w, y);
    }
    if (on) glowPath (g, p, Colour::fromHSV (std::fmod (0.83f + pos * 0.6f, 1.0f), 0.8f, 1.0f, 1.0f), 1.8f);
    g.setColour (on ? Colours::white : dim);
    g.strokePath (p, PathStrokeType (on ? 1.4f : 1.0f));
    g.setFont (font (10.0f));
    g.setColour (on ? yellow : dim);
    g.drawText (on ? "WAVETABLE  " + String (roundToInt (pos * 255)) + " / 256" : "OFF", b.reduced (8, 4).toNearestInt(), Justification::topRight);
}

//==============================================================================
namespace
{
    // rough magnitude of each filter type at frequency f (Hz)
    float filterMag (int type, float f, float cut, float res, float var)
    {
        const double w = f / cut, k = 2.0 - 1.96 * res;
        auto den = [w] (double kk) { return std::sqrt ((1 - w * w) * (1 - w * w) + kk * kk * w * w); };
        switch (type)
        {
            case FLp12: return (float) (1.0 / den (k));
            case FLp24: case FDirty: return (float) (1.0 / (den (1.85) * den (k)));
            case FHp12: return (float) (w * w / den (k));
            case FHp24: return (float) (w * w * w * w / (den (1.85) * den (k)));
            case FBp12: return (float) ((0.4 + 0.6 * k) * w / den (k));
            case FNotch: return (float) (std::abs (1 - w * w) / den (k));
            case FPeak: return (float) (1.0 + 0.6 * res * 4.0 * w / den (0.6));
            case FCombP: case FCombM:
            {
                const double fb = res * 0.97, ph = MathConstants<double>::twoPi * w;
                const double s = type == FCombP ? 1.0 : -1.0;
                return (float) ((1 - fb * 0.6) / std::sqrt (1 + fb * fb - 2 * s * fb * std::cos (ph)));
            }
            case FFormant:
            {
                double sum = 0;
                for (double fr : { 1.0, 1.9, 3.4 }) { const double ww = f / (cut * fr * std::exp2 (var - 0.3)); sum += 0.6 / std::sqrt ((1 - ww * ww) * (1 - ww * ww) * 40 + ww * ww); }
                return (float) sum;
            }
            case FPhaser: return (float) (0.5 + 0.5 * std::abs (std::cos (MathConstants<double>::pi * std::log2 (w) * 2.0)));
            case FRing: return (float) (std::abs (std::log2 (w)) < 0.15 ? 1.4 : 0.7);
            default: return 1.0f;
        }
    }
}

void FilterView::update()
{
    const auto& p = Ix::get().filt[fi];
    const int base = fi == 0 ? DF1Cut : DF2Cut;
    const float cutN = toNorm (p.cut, proc.v (paramDefs()[(size_t) p.cut].id)) + proc.engine.uiDestMod[(size_t) base].load();
    const float now[5] = { cutN, proc.v (paramDefs()[(size_t) p.res].id), proc.v (paramDefs()[(size_t) p.type].id),
                           proc.v (paramDefs()[(size_t) p.var].id), proc.v (paramDefs()[(size_t) p.on].id) };
    if (std::memcmp (now, last, sizeof (now)) != 0) { std::memcpy (last, now, sizeof (now)); repaint(); }
}

void FilterView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    screen (g, b);
    auto in = b.reduced (4.0f);
    const auto& p = Ix::get().filt[fi];
    const float cut = fromNorm (p.cut, jlimit (0.0f, 1.0f, last[0]));
    const float res = last[1] * 0.01f, var = last[3] * 0.01f;
    const int type = (int) last[2];
    const bool on = last[4] > 0.5f;
    Path line, fill;
    const int w = (int) in.getWidth();
    for (int x = 0; x <= w; ++x)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) x / w);
        const float db = jlimit (-36.0f, 18.0f, Decibels::gainToDecibels (filterMag (type, f, cut, res, var), -60.0f));
        const float y = in.getY() + in.getHeight() * (18.0f - db) / 54.0f;
        if (x == 0) { line.startNewSubPath (in.getX(), y); fill.startNewSubPath (in.getX(), in.getBottom()); }
        line.lineTo (in.getX() + x, y);
        fill.lineTo (in.getX() + x, y);
    }
    fill.lineTo (in.getRight(), in.getBottom());
    fill.closeSubPath();
    const auto c = fi == 0 ? cyan : green;
    g.setGradientFill (ColourGradient (c.withAlpha (on ? 0.35f : 0.08f), 0, in.getY(), pink.withAlpha (0.02f), 0, in.getBottom(), false));
    g.fillPath (fill);
    if (on) glowPath (g, line, c, 1.6f);
    else { g.setColour (dim.withAlpha (0.5f)); g.strokePath (line, PathStrokeType (1.0f)); }
    g.setColour (on ? text : dim);
    g.setFont (font (10.0f));
    g.drawText (on ? fmt (cut, "Hz") : "OFF", in.toNearestInt().reduced (4, 2), Justification::topRight);
}

//==============================================================================
void LfoView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    screen (g, b);
    auto in = b.reduced (8.0f, 10.0f);
    const auto& p = Ix::get().lfo[index];
    const int shape = (int) proc.v (paramDefs()[(size_t) p.shp].id);
    static const float sh[8] = { 0.7f, 0.2f, 0.9f, 0.4f, 0.1f, 0.6f, 0.3f, 0.8f };
    auto val = [&] (float ph)
    {
        const int seg = (int) (ph * 8) % 8;
        switch (shape)
        {
            case LSine: return 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * ph);
            case LTri: return ph < 0.5f ? 2 * ph : 2 - 2 * ph;
            case LSawUp: return ph;
            case LSawDown: return 1 - ph;
            case LSquare: return ph < 0.5f ? 1.0f : 0.0f;
            case LPulse: return ph < 0.2f ? 1.0f : 0.0f;
            case LSH: return sh[seg];
            case LSmooth: { const float u = ph * 8 - (int) (ph * 8); return sh[seg] + (sh[(seg + 1) % 8] - sh[seg]) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * u)); }
            case LStairs: return std::floor (ph * 4) / 3.0f;
            default: { const float s = std::sin (MathConstants<float>::pi * ph); return s * s; }
        }
    };
    Path path;
    for (int i = 0; i <= 200; ++i)
    {
        const float ph = i / 200.0f;
        const float y = in.getBottom() - val (shape == LSH || shape == LSmooth ? ph : ph) * in.getHeight();
        if (i == 0) path.startNewSubPath (in.getX(), y); else path.lineTo (in.getX() + ph * in.getWidth(), y);
    }
    ColourGradient cg (pink, in.getX(), 0, yellow, in.getRight(), 0, false);
    cg.addColour (0.5, cyan);
    g.setGradientFill (cg);
    g.strokePath (path, PathStrokeType (2.0f));
    const float ph = proc.engine.uiLfoPhase[(size_t) index].load(), v = proc.engine.uiLfo[(size_t) index].load();
    const float x = in.getX() + jlimit (0.0f, 1.0f, ph) * in.getWidth(), y = in.getBottom() - v * in.getHeight();
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
    g.setColour (cyan.withAlpha (0.4f)); g.fillEllipse (x - 7, y - 7, 14, 14);
    g.setColour (Colours::white); g.fillEllipse (x - 3.5f, y - 3.5f, 7, 7);
}

void EnvView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    screen (g, b);
    auto in = b.reduced (8.0f, 8.0f);
    const auto& p = Ix::get().env[index];
    const float a = proc.v (paramDefs()[(size_t) p.a].id), d = proc.v (paramDefs()[(size_t) p.d].id),
                s = proc.v (paramDefs()[(size_t) p.s].id) * 0.01f, r = proc.v (paramDefs()[(size_t) p.r].id);
    auto squash = [] (float ms) { return std::sqrt (jmax (0.0f, ms)) + 2.0f; };
    const float A = squash (a), D = squash (d), H = 25.0f, R = squash (r), total = A + D + H + R;
    auto X = [&] (float t) { return in.getX() + t / total * in.getWidth(); };
    auto Y = [&] (float lv) { return in.getBottom() - lv * in.getHeight(); };
    Path path, fill;
    path.startNewSubPath (X (0), Y (0));
    path.lineTo (X (A), Y (1));
    for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f; path.lineTo (X (A + D * t), Y (s + (1 - s) * std::exp (-4.6f * t))); }
    path.lineTo (X (A + D + H), Y (s));
    for (int i = 1; i <= 24; ++i) { const float t = i / 24.0f; path.lineTo (X (A + D + H + R * t), Y (s * std::exp (-4.6f * t))); }
    fill = path;
    fill.lineTo (X (total), Y (0));
    fill.closeSubPath();
    const Colour cs[4] = { cyan, pink, yellow, green };
    g.setGradientFill (ColourGradient (cs[index].withAlpha (0.3f), 0, in.getY(), cs[index].withAlpha (0.0f), 0, in.getBottom(), false));
    g.fillPath (fill);
    glowPath (g, path, cs[index], 1.6f);
    const float lv = proc.engine.uiEnv[(size_t) index].load();
    g.setColour (Colours::white.withAlpha (0.7f));
    g.fillRect (in.getRight() - 4, Y (lv), 4.0f, lv * in.getHeight());
}

void ChaosView::tick()
{
    trail[(size_t) head] = { proc.engine.uiChaosX[(size_t) index].load(), proc.engine.uiChaosY[(size_t) index].load() };
    head = (head + 1) % (int) trail.size();
    filled = jmin (filled + 1, (int) trail.size());
    repaint();
}

void ChaosView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    screen (g, b);
    auto in = b.reduced (6.0f);
    for (int i = 1; i < filled; ++i)
    {
        const int a = (head - filled + i - 1 + (int) trail.size()) % (int) trail.size(), c = (a + 1) % (int) trail.size();
        const float t = (float) i / filled;
        auto P = [&] (Point<float> p) { return Point<float> (in.getCentreX() + p.x * in.getWidth() * 0.48f, in.getCentreY() - p.y * in.getHeight() * 0.48f); };
        g.setColour (Colour::fromHSV (std::fmod (0.5f + t * 0.5f, 1.0f), 0.9f, 1.0f, t));
        g.drawLine (Line<float> (P (trail[(size_t) a]), P (trail[(size_t) c])), 1.0f + t);
    }
}

//==============================================================================
Wheel::Wheel (String n, bool bipolar, std::function<float()> r, std::function<void (float)> w)
    : name (std::move (n)), centred (bipolar), read (std::move (r)), write (std::move (w)) {}

void Wheel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto slot = b.withTrimmedBottom (13.0f);
    g.setColour (Colour (0xff05070a));
    g.fillRoundedRectangle (slot, 5.0f);
    g.setColour (Colour (0xff3a3f46));
    g.drawRoundedRectangle (slot.reduced (0.5f), 5.0f, 1.0f);
    const float pos = centred ? 0.5f - value * 0.5f : 1.0f - value;
    const float cy = slot.getY() + 6 + pos * (slot.getHeight() - 12);
    auto cap = Rectangle<float> (slot.getX() + 2, cy - 7, slot.getWidth() - 4, 14);
    g.setColour (pink.withAlpha (0.4f));
    g.fillRoundedRectangle (cap.expanded (2), 4);
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (i % 2 ? Colour (0xff8f959d) : Colour (0xfff2f4f7));
        g.fillRect (cap.getX(), cap.getY() + i * cap.getHeight() / 5, cap.getWidth(), cap.getHeight() / 5);
    }
    g.setColour (dim);
    g.setFont (font (9.0f));
    g.drawText (name, b.removeFromBottom (12).toNearestInt(), Justification::centred);
}

void Wheel::mouseDrag (const MouseEvent& e)
{
    auto slot = getLocalBounds().toFloat().withTrimmedBottom (13.0f).reduced (0, 6);
    const float t = jlimit (0.0f, 1.0f, (e.position.y - slot.getY()) / slot.getHeight());
    value = centred ? (0.5f - t) * 2.0f : 1.0f - t;
    write (value);
    repaint();
}
void Wheel::mouseUp (const MouseEvent&)
{
    dragging = false;
    if (centred) { value = 0.0f; write (0.0f); repaint(); }
}
void Wheel::refresh()
{
    if (dragging) return;
    const float v = read();
    if (std::abs (v - value) > 0.002f) { value = v; repaint(); }
}

void Meter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff05030c));
    g.fillRoundedRectangle (r, 3.0f);
    const float h = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (shown, -60.0f) + 60.0f) / 60.0f) * (r.getHeight() - 2);
    ColourGradient c (green, 0, r.getBottom(), red, 0, r.getY(), false);
    c.addColour (0.55, cyan);
    c.addColour (0.8, yellow);
    g.setGradientFill (c);
    g.fillRoundedRectangle (r.reduced (1).withTop (r.getBottom() - 1 - h), 2.0f);
}

//==============================================================================
void Page::paint (Graphics& g)
{
    if (layer.getWidth() != getWidth() || layer.getHeight() != getHeight())
    {
        layer = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
        Graphics lg (layer);
        for (auto& p : panels)
        {
            auto r = p.r.toFloat();
            DropShadow (Colours::black.withAlpha (0.6f), 14, { 0, 4 }).drawForRectangle (lg, p.r);
            lg.setGradientFill (ColourGradient (glassHi, 0, r.getY(), glass, 0, r.getBottom(), false));
            lg.fillRoundedRectangle (r, 11.0f);
            auto rb = rainbow (r.getX(), r.getY(), r.getRight(), r.getBottom());
            lg.setGradientFill (rb);
            lg.drawRoundedRectangle (r.reduced (0.5f), 11.0f, 1.4f);
            lg.setColour (Colours::white.withAlpha (0.05f));
            lg.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 10, r.getRight() - 10);
            if (p.title.isNotEmpty())
            {
                lg.setColour (p.colour.withAlpha (0.4f));
                lg.fillEllipse (r.getX() + 7, r.getY() + 7, 12, 12);
                lg.setColour (p.colour);
                lg.fillEllipse (r.getX() + 9.5f, r.getY() + 9.5f, 7, 7);
                lg.setColour (text);
                lg.setFont (font (12.5f));
                lg.drawText (p.title, (int) r.getX() + 24, (int) r.getY() + 4, 200, 18, Justification::centredLeft);
            }
        }
    }
    g.drawImageAt (layer, 0, 0);
}

Knob& Page::knob (const String& id, const String& name, Colour c, int size, int x, int y, int cell)
{
    auto& k = *knobs.emplace_back (std::make_unique<Knob> (proc, id, name, c, size));
    addAndMakeVisible (k);
    const int w = cell > 0 ? cell : size + 22;
    k.setBounds (x, y, w, Knob::heightFor (size));
    return k;
}

ParamCombo& Page::combo (const String& id, Rectangle<int> r)
{
    auto& c = own (std::make_unique<ParamCombo> (proc, id));
    c.setBounds (r);
    return c;
}

ParamButton& Page::toggle (const String& id, const String& t, Colour c, Rectangle<int> r)
{
    auto& b = own (std::make_unique<ParamButton> (proc, id, t, c));
    b.setBounds (r);
    return b;
}

ChoiceButtons& Page::choices (const String& id, Colour c, Rectangle<int> r, StringArray labels)
{
    auto& cb = own (std::make_unique<ChoiceButtons> (proc, id, c, labels));
    cb.setBounds (r);
    choiceRows.push_back (&cb);
    return cb;
}

void Page::label (const String& t, Rectangle<int> r, Colour c, float size, Justification j)
{
    struct L : Component
    {
        String t; Colour c; float s; Justification j;
        L (String a, Colour b, float sz, Justification jj) : t (std::move (a)), c (b), s (sz), j (jj) { setInterceptsMouseClicks (false, false); }
        void paint (Graphics& g) override { g.setColour (c); g.setFont (font (s)); g.drawFittedText (t, getLocalBounds(), j, 4); }
    };
    auto& l = own (std::make_unique<L> (t, c, size, j));
    l.setBounds (r);
}

void Page::tickKnobs()
{
    for (auto& k : knobs) k->refreshMods();
    for (auto* c : choiceRows) c->update();
}

//==============================================================================
// MAIN page
namespace
{
    String sid (const char* prefix, int n, const char* what) { return String (prefix) + String (n) + what; }

    class MainPage : public Page
    {
    public:
        explicit MainPage (BullyProcessor& p) : Page (p),
            lfoView (p), envView (p), chaosView (p),
            keys (p.keyboard, MidiKeyboardComponent::horizontalKeyboard),
            pitch ("PITCH", true, [&p] { return p.engine.uiBend.load(); }, [&p] (float v) { p.screenPitch.store (v); p.pitchMoved.store (true); }),
            mod ("MOD", false, [&p] { return p.engine.uiMod.load(); }, [&p] (float v) { p.screenMod.store (v); p.modMoved.store (true); })
        {
            const Colour oscC[3] = { pink, yellow, orange };
            for (int o = 0; o < kOsc; ++o)
            {
                const int x = 10 + o * 305, y = 0, w = 298;
                const String L = String::charToString ((juce_wchar) ('A' + o)), P = "o" + String::charToString ((juce_wchar) ('a' + o));
                panels.push_back ({ { x, y, w, 262 }, "OSC " + L, oscC[o] });
                toggle (P + "on", "ON", oscC[o], { x + 82, y + 5, 34, 18 });
                combo (P + "tab", { x + w - 150, y + 5, 142, 19 });
                auto& wv = own (std::make_unique<WaveView> (p, o));
                wv.setBounds (x + 8, y + 28, w - 16, 92);
                waves.push_back (&wv);
                label ("WARP", { x + 10, y + 124, 40, 18 });
                combo (P + "w1t", { x + 46, y + 124, 118, 18 });
                combo (P + "w2t", { x + 170, y + 124, 118, 18 });
                const int c = 41, ks = 28;
                int kx = x + 6;
                for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "pos", "WT POS" }, { "w1", "WARP 1" }, { "w2", "WARP 2" }, { "oct", "OCT" }, { "semi", "SEMI" }, { "fine", "FINE" }, { "lvl", "LEVEL" } })
                { knob (P + id, n, oscC[o], ks, kx, y + 148, c); kx += c; }
                kx = x + 6 + c / 2;
                for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "uni", "UNISON" }, { "det", "DETUNE" }, { "bld", "BLEND" }, { "phs", "PHASE" }, { "rnd", "RAND" }, { "pan", "PAN" } })
                { knob (P + id, n, oscC[o], ks, kx, y + 204, c); kx += c; }
            }

            // LFO panel
            {
                const int x = 925, y = 0, w = 305;
                panels.push_back ({ { x, y, w, 262 }, "LFO", cyan });
                for (int l = 0; l < kLfo; ++l)
                {
                    auto& t = own (std::make_unique<SourceTag> (SLfo1 + l, "LFO " + String (l + 1), cyan));
                    t.setBounds (x + 8 + (l % 5) * 58, y + 26 + (l / 5) * 21, 55, 18);
                    t.onClick = [this, l] { selectLfo (l); };
                    lfoTags.push_back (&t);
                }
                addAndMakeVisible (lfoView);
                lfoView.setBounds (x + 8, y + 70, w - 16, 82);
                lfoArea = { x, y, w, 262 };
                label ("DRAG THE GLOWING DOT ONTO ANY KNOB TO MODULATE IT", { x + 8, y + 245, w - 16, 14 }, cyan, 9.0f);
            }
            // NOISE
            {
                const int x = 10, y = 270, w = 205;
                panels.push_back ({ { x, y, w, 196 }, "NOISE", green });
                toggle ("non", "ON", green, { x + 82, y + 5, 34, 18 });
                combo ("ntype", { x + w - 86, y + 5, 78, 19 });
                knob ("ntone", "TONE", green, 40, x + 8, y + 40, 62);
                knob ("nlvl", "LEVEL", green, 40, x + 72, y + 40, 62);
                knob ("npan", "PAN", green, 40, x + 136, y + 40, 62);
                label ("ANALOG CHARACTER", { x + 10, y + 112, w - 20, 14 }, pink, 10.0f);
                int cx = x + 4;
                for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "drift", "DRIFT" }, { "sat", "SAT" }, { "boost", "BOOST" }, { "spread", "SPREAD" } })
                { knob (id, n, pink, 26, cx, y + 130, 49); cx += 49; }
            }
            // FILTERS
            for (int f = 0; f < 2; ++f)
            {
                const int x = 223 + f * 350, y = 270, w = 344;
                const auto c = f == 0 ? cyan : green;
                const String P = "f" + String (f + 1);
                panels.push_back ({ { x, y, w, 196 }, "FILTER " + String (f + 1), c });
                toggle (P + "on", "ON", c, { x + 92, y + 5, 34, 18 });
                combo (P + "type", { x + w - 130, y + 5, 122, 19 });
                auto& fv = own (std::make_unique<FilterView> (p, f));
                fv.setBounds (x + 8, y + 28, w - 16, 56);
                filters.push_back (&fv);
                knob (P + "cut", "CUTOFF", c, 54, x + 6, y + 88, 80);
                int kx = x + 86;
                for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "res", "RES" }, { "drv", "DRIVE" }, { "var", "VAR" }, { "mix", "MIX" }, { "key", "KEY" } })
                { knob (P + id, n, c, 30, kx, y + 96, 50); kx += 50; }
                label ("INPUT", { x + 10, y + 170, 40, 18 });
                const char* src[] = { "oaf", "obf", "ocf", "nf" };
                const char* names[] = { "A", "B", "C", "N" };
                for (int s = 0; s < 4; ++s)
                    toggle (String (src[s]) + String (f + 1), names[s], c, { x + 50 + s * 30, y + 170, 27, 18 });
                if (f == 1) choices ("froute", green, { x + w - 150, y + 170, 142, 18 });
            }
            // ENVELOPES
            {
                const int x = 925, y = 270, w = 305;
                panels.push_back ({ { x, y, w, 196 }, "ENV", pink });
                for (int e = 0; e < kEnv; ++e)
                {
                    auto& t = own (std::make_unique<SourceTag> (SEnv1 + e, "ENV " + String (e + 1), pink));
                    if (e == 0) t.setTooltip ("ENV 1 is the amp envelope. Click to show it, drag onto a knob to modulate it");
                    t.setBounds (x + 62 + e * 60, y + 5, 56, 18);
                    t.onClick = [this, e] { selectEnv (e); };
                    envTags.push_back (&t);
                }
                addAndMakeVisible (envView);
                envView.setBounds (x + 8, y + 28, w - 16, 76);
                envArea = { x, y, w, 196 };
            }
            // MACROS
            {
                const int x = 10, y = 474, w = 300;
                panels.push_back ({ { x, y, w, 156 }, "MACROS", yellow });
                for (int m = 0; m < kMacro; ++m)
                {
                    auto& t = own (std::make_unique<SourceTag> (SMacro1 + m, "M" + String (m + 1), yellow));
                    t.setBounds (x + 16 + m * 71, y + 30, 50, 18);
                    macroTags.push_back (&t);
                    knob ("m" + String (m + 1), "MACRO " + String (m + 1), yellow, 40, x + 8 + m * 71, y + 56, 66);
                }
            }
            // CHAOS
            {
                const int x = 318, y = 474, w = 250;
                panels.push_back ({ { x, y, w, 156 }, "CHAOS", purple });
                for (int c = 0; c < kChaos; ++c)
                {
                    auto& t = own (std::make_unique<SourceTag> (SChaos1 + c, "CHAOS " + String (c + 1), purple.brighter (0.4f)));
                    t.setBounds (x + 80 + c * 76, y + 5, 72, 18);
                    t.onClick = [this, c] { selectChaos (c); };
                    chaosTags.push_back (&t);
                }
                addAndMakeVisible (chaosView);
                chaosView.setBounds (x + 8, y + 28, 132, 120);
                chaosArea = { x, y, w, 156 };
            }
            // KEYS
            {
                const int x = 576, y = 474, w = 654;
                panels.push_back ({ { x, y, w, 156 }, "KEYS", cyan });
                combo ("vmode", { x + 80, y + 5, 84, 19 });
                auto& panic = own (std::make_unique<TextButton> ("PANIC"));
                panic.setBounds (x + w - 70, y + 5, 62, 19);
                panic.setColour (TextButton::buttonOnColourId, red);
                panic.onClick = [&p, this] { p.keyboard.allNotesOff (0); p.panicRequested.store (true); keys.repaint(); };
                addAndMakeVisible (pitch); addAndMakeVisible (mod);
                pitch.setBounds (x + 8, y + 32, 28, 112);
                mod.setBounds (x + 38, y + 32, 28, 112);
                pitch.setTooltip ("Pitch bend (springs back); range is BEND on the GLOBAL page");
                mod.setTooltip ("Mod wheel (also CC1). Use MOD WHEEL as a source on the MATRIX page");
                knob ("glide", "GLIDE", green, 36, x + 68, y + 50, 58);
                keys.setAvailableRange (24, 96);
                keys.setLowestVisibleKey (36);
                keys.setKeyWidth (17.0f);
                keys.setBounds (x + 128, y + 32, w - 138, 114);
                addAndMakeVisible (keys);
            }
            selectLfo (0);
            selectEnv (0);
            selectChaos (0);
        }

        void tick() override
        {
            tickKnobs();
            for (auto* w : waves) w->update();
            for (auto* f : filters) f->update();
            lfoView.repaint();
            envView.repaint();
            chaosView.tick();
            pitch.refresh();
            mod.refresh();
            for (int l = 0; l < kLfo; ++l) { lfoTags[(size_t) l]->level = proc.engine.uiLfo[(size_t) l].load(); lfoTags[(size_t) l]->repaint(); }
            for (int e = 0; e < kEnv; ++e) { envTags[(size_t) e]->level = proc.engine.uiEnv[(size_t) e].load(); envTags[(size_t) e]->repaint(); }
            for (int m = 0; m < kMacro; ++m) { macroTags[(size_t) m]->level = proc.v (("m" + String (m + 1)).toRawUTF8()) * 0.01f; macroTags[(size_t) m]->repaint(); }
            const bool sync = proc.v (sid ("l", lfoIndex + 1, "sync").toRawUTF8()) > 0.5f;
            if (lfoRate != nullptr) lfoRate->setVisible (sync);
            if (lfoHz != nullptr) lfoHz->setVisible (! sync);
        }

    private:
        // the LFO / ENV / CHAOS panels show one of several sources: rebuild their controls on selection
        struct Group { std::vector<Component*> items; };
        void clearGroup (Group& g)
        {
            for (auto* c : g.items)
            {
                knobs.erase (std::remove_if (knobs.begin(), knobs.end(), [c] (auto& k) { return k.get() == c; }), knobs.end());
                choiceRows.erase (std::remove (choiceRows.begin(), choiceRows.end(), c), choiceRows.end());
                owned.erase (std::remove_if (owned.begin(), owned.end(), [c] (auto& o) { return o.get() == c; }), owned.end());
            }
            g.items.clear();
        }
        Component* lastAdded() { return owned.back().get(); }

        void selectLfo (int l)
        {
            lfoIndex = l;
            for (int i = 0; i < kLfo; ++i) { lfoTags[(size_t) i]->selected = i == l; lfoTags[(size_t) i]->repaint(); }
            lfoView.index = l;
            clearGroup (lfoGroup);
            const auto P = "l" + String (l + 1);
            const int x = lfoArea.getX(), y = lfoArea.getY(), w = lfoArea.getWidth();
            lfoGroup.items.push_back (&choices (P + "mode", cyan, { x + 8, y + 158, 150, 18 }));
            lfoGroup.items.push_back (&toggle (P + "sync", "BPM", cyan, { x + 164, y + 158, 44, 18 }));
            lfoGroup.items.push_back (&combo (P + "shp", { x + 214, y + 158, w - 222, 18 }));
            lfoRate = &combo (P + "rate", { x + 10, y + 196, 66, 20 });
            lfoGroup.items.push_back (lfoRate);
            lfoHz = &knob (P + "hz", "RATE", cyan, 30, x + 8, y + 182, 70);
            lfoGroup.items.push_back (lfoHz);
            lfoGroup.items.push_back (&knob (P + "smo", "SMOOTH", cyan, 30, x + 84, y + 182, 70));
            lfoGroup.items.push_back (&knob (P + "phs", "PHASE", cyan, 30, x + 160, y + 182, 70));
            auto& rateLabel = own (std::make_unique<Label> (String(), "RATE"));
            rateLabel.setFont (font (9.0f)); rateLabel.setColour (Label::textColourId, dim); rateLabel.setJustificationType (Justification::centred);
            rateLabel.setBounds (x + 10, y + 218, 66, 12);
            lfoGroup.items.push_back (&rateLabel);
            auto& tip = own (std::make_unique<Label> (String(), "FREE runs on, TRIG restarts on each note, ENV runs once"));
            tip.setFont (font (9.0f, false)); tip.setColour (Label::textColourId, dim); tip.setJustificationType (Justification::topLeft);
            tip.setBounds (x + 236, y + 184, w - 244, 50);
            lfoGroup.items.push_back (&tip);
        }

        void selectEnv (int e)
        {
            for (int i = 0; i < kEnv; ++i) { envTags[(size_t) i]->selected = i == e; envTags[(size_t) i]->repaint(); }
            envView.index = e;
            clearGroup (envGroup);
            const auto P = "e" + String (e + 1);
            const int x = envArea.getX(), y = envArea.getY();
            int kx = x + 10;
            for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "a", "ATTACK" }, { "d", "DECAY" }, { "s", "SUSTAIN" }, { "r", "RELEASE" } })
            { envGroup.items.push_back (&knob (P + id, n, pink, 34, kx, y + 112, 72)); kx += 72; }
        }

        void selectChaos (int c)
        {
            for (int i = 0; i < kChaos; ++i) { chaosTags[(size_t) i]->selected = i == c; chaosTags[(size_t) i]->repaint(); }
            chaosView.index = c;
            clearGroup (chaosGroup);
            const auto P = "c" + String (c + 1);
            const int x = chaosArea.getX(), y = chaosArea.getY();
            chaosGroup.items.push_back (&combo (P + "type", { x + 146, y + 30, 96, 18 }));
            chaosGroup.items.push_back (&knob (P + "rate", "RATE", purple.brighter (0.4f), 28, x + 144, y + 60, 50));
            chaosGroup.items.push_back (&knob (P + "smo", "SMOOTH", purple.brighter (0.4f), 28, x + 194, y + 60, 50));
        }

        LfoView lfoView;
        EnvView envView;
        ChaosView chaosView;
        MidiKeyboardComponent keys;
        Wheel pitch, mod;
        std::vector<WaveView*> waves;
        std::vector<FilterView*> filters;
        std::vector<SourceTag*> lfoTags, envTags, macroTags, chaosTags;
        Group lfoGroup, envGroup, chaosGroup;
        Rectangle<int> lfoArea, envArea, chaosArea;
        Component* lfoRate = nullptr;
        Component* lfoHz = nullptr;
        int lfoIndex = 0;
    };

    //==========================================================================
    class FxPage : public Page
    {
    public:
        explicit FxPage (BullyProcessor& p) : Page (p)
        {
            const int cw = 401, ch = 202, gx = 8, gy = 8;
            auto card = [&] (int i, const String& title, Colour c, const String& onId)
            {
                const auto r = Rectangle<int> (10 + (i % 3) * (cw + gx), (i / 3) * (ch + gy), cw, ch);
                panels.push_back ({ r, title, c });
                toggle (onId, "ON", c, { r.getRight() - 46, r.getY() + 5, 38, 18 });
                return r;
            };
            auto row = [&] (Rectangle<int> r, Colour c, std::initializer_list<std::pair<const char*, const char*>> ks, int y = 70)
            {
                const int n = (int) ks.size(), cell = jmin (78, (r.getWidth() - 20) / n);
                int x = r.getX() + (r.getWidth() - n * cell) / 2;
                for (auto [id, name] : ks) { knob (id, name, c, 44, x, r.getY() + y, cell); x += cell; }
            };
            auto note = [&] (Rectangle<int> r, const String& t) { label (t, { r.getX() + 12, r.getBottom() - 34, r.getWidth() - 24, 30 }, dim, 9.5f, Justification::topLeft); };

            auto r = card (0, "HYPER", pink, "hyon");
            row (r, pink, { { "hyuni", "VOICES" }, { "hydet", "DETUNE" }, { "hyrate", "RATE" }, { "hymix", "MIX" } });
            note (r, "A cloud of detuned copies spread wide: instant supersaw.");
            r = card (1, "DISTORTION", orange, "dson");
            combo ("dstype", { r.getX() + 160, r.getY() + 34, 130, 20 });
            row (r, orange, { { "dsdrv", "DRIVE" }, { "dstone", "TONE" }, { "dsmix", "MIX" } });
            note (r, "TUBE, SOFT, HARD, DIODE, FOLD, BITCRUSH, DOWNSAMPLE, RECTIFY.");
            r = card (2, "CHORUS / FLANGER / PHASER", cyan, "moon");
            choices ("motype", cyan, { r.getX() + 90, r.getY() + 34, 220, 20 });
            row (r, cyan, { { "morate", "RATE" }, { "modep", "DEPTH" }, { "mofb", "FEEDBACK" }, { "momix", "MIX" } });
            r = card (3, "FREQ SHIFTER", purple.brighter (0.4f), "fson");
            row (r, purple.brighter (0.4f), { { "fsfrq", "SHIFT" }, { "fsmix", "MIX" } });
            note (r, "Moves every partial by the same Hz: metallic, detuned, barber-pole.");
            r = card (4, "OTT", yellow, "oton");
            row (r, yellow, { { "otdep", "DEPTH" }, { "ottime", "TIME" }, { "otup", "UPWARD" }, { "otdn", "DOWNWARD" } });
            note (r, "3-band upward + downward compression. Low depth = tight and loud.");
            r = card (5, "EQ", green, "eqon");
            row (r, green, { { "eqlo", "LOW" }, { "eqmidf", "MID FREQ" }, { "eqmid", "MID" }, { "eqhi", "HIGH" } });
            r = card (6, "DELAY", cyan, "dlon");
            toggle ("dlsync", "BPM", cyan, { r.getX() + 100, r.getY() + 5, 44, 18 });
            toggle ("dlpp", "PING-PONG", cyan, { r.getX() + 150, r.getY() + 5, 84, 18 });
            combo ("dlnote", { r.getX() + 240, r.getY() + 5, 100, 18 });
            row (r, cyan, { { "dlms", "TIME" }, { "dlfb", "FEEDBACK" }, { "dltone", "TONE" }, { "dlmix", "MIX" } }, 40);
            r = card (7, "REVERB", pink, "rvon");
            row (r, pink, { { "rvsize", "SIZE" }, { "rvdec", "DECAY" }, { "rvdamp", "DAMP" }, { "rvpre", "PRE" }, { "rvwid", "WIDTH" }, { "rvmix", "MIX" } }, 40);
            r = card (8, "3-BAND SPLITTER", yellow, "spon");
            row (r, yellow, { { "splo", "LOW | MID" }, { "sphi", "MID | HIGH" } }, 40);
            note (r, "LOW band stays mono, DELAY + REVERB only touch the HIGH band: clean body, shimmering top.");
        }
        void tick() override { tickKnobs(); }
    };

    //==========================================================================
    class MatrixPage : public Page
    {
    public:
        explicit MatrixPage (BullyProcessor& p) : Page (p)
        {
            panels.push_back ({ { 10, 0, 1220, 630 }, "MOD MATRIX", cyan });
            label ("Drag LFO / ENV / CHAOS / MACRO tags onto knobs on the MAIN page, or set routings here. Amount is a share of the knob's range.",
                   { 140, 4, 1080, 18 }, dim, 10.0f);
            for (int s = 0; s < kSlots; ++s)
            {
                const int col = s / 8, rowI = s % 8;
                const int x = 24 + col * 604, y = 40 + rowI * 72;
                const String P = "x" + String (s + 1);
                label (String (s + 1), { x, y + 8, 24, 20 }, cyan, 14.0f);
                combo (P + "src", { x + 28, y + 8, 140, 22 });
                label (String::fromUTF8 ("\xe2\x86\x92"), { x + 172, y + 8, 18, 22 }, text, 14.0f);
                combo (P + "dst", { x + 192, y + 8, 160, 22 });
                auto& sl = own (std::make_unique<Slider> (Slider::LinearHorizontal, Slider::TextBoxRight));
                sl.setColour (Slider::trackColourId, s % 2 ? pink : cyan);
                sl.setColour (Slider::textBoxTextColourId, text);
                sl.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
                sl.setTextValueSuffix ("%");
                sl.setBounds (x + 362, y + 6, 200, 26);
                atts.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, P + "amt", sl));
                auto& del = own (std::make_unique<TextButton> ("X"));
                del.setBounds (x + 568, y + 8, 22, 22);
                del.setTooltip ("Clear this slot");
                del.onClick = [&p, s] { p.removeModulation (s); };
            }
        }
        void tick() override {}
    private:
        std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> atts;
    };

    //==========================================================================
    class ArpPage : public Page
    {
    public:
        explicit ArpPage (BullyProcessor& p) : Page (p)
        {
            const Rectangle<int> r (260, 60, 720, 360);
            panels.push_back ({ r, "ARPEGGIATOR", yellow });
            toggle ("arpon", "ARP ON", yellow, { r.getX() + 130, r.getY() + 5, 80, 20 });
            label ("MODE", { r.getX() + 24, r.getY() + 46, 100, 16 });
            choices ("arpmode", yellow, { r.getX() + 24, r.getY() + 64, r.getWidth() - 48, 26 });
            label ("RATE", { r.getX() + 24, r.getY() + 110, 100, 16 });
            combo ("arprate", { r.getX() + 24, r.getY() + 128, 120, 24 });
            int x = r.getX() + 190;
            for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "arpoct", "OCTAVES" }, { "arpgate", "GATE" }, { "arpswing", "SWING" } })
            { knob (id, n, yellow, 56, x, r.getY() + 110, 110); x += 120; }
            label ("Hold a chord and BULLY plays it as a pattern locked to your song tempo (or its own clock when the song is stopped). "
                   "CHORD plays all held notes on every step. Use with PLUCK presets or the ARP folder.",
                   { r.getX() + 24, r.getY() + 230, r.getWidth() - 48, 80 }, dim, 12.0f, Justification::topLeft);
        }
        void tick() override { tickKnobs(); }
    };

    //==========================================================================
    class GlobalPage : public Page
    {
    public:
        explicit GlobalPage (BullyProcessor& p) : Page (p)
        {
            const Rectangle<int> r (160, 60, 920, 300);
            panels.push_back ({ r, "VOICE", cyan });
            label ("MODE", { r.getX() + 24, r.getY() + 40, 100, 16 });
            choices ("vmode", cyan, { r.getX() + 24, r.getY() + 58, 260, 26 });
            int x = r.getX() + 310;
            for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "poly", "VOICES" }, { "glide", "GLIDE" }, { "bend", "BEND RANGE" }, { "velo", "VELOCITY" }, { "vol", "VOLUME" } })
            { knob (id, n, cyan, 52, x, r.getY() + 40, 116); x += 118; }
            label ("MONO retriggers every note (psy basses), LEGATO glides between held notes, POLY plays chords. "
                   "For a tight psy bass set the oscillator's PHASE to 0 and RAND to 0 so every hit is identical.",
                   { r.getX() + 24, r.getY() + 140, r.getWidth() - 48, 60 }, dim, 12.0f, Justification::topLeft);
            label ("BULLY 1.0  by Asher Swissa  -  SKAZI", { r.getX() + 24, r.getBottom() - 40, 500, 20 }, pink, 13.0f);

            const Rectangle<int> c (160, 372, 920, 230);
            panels.push_back ({ c, "ANALOG CHARACTER", pink });
            int x2 = c.getX() + 40;
            for (auto [id, n] : std::initializer_list<std::pair<const char*, const char*>> { { "drift", "DRIFT" }, { "sat", "SATURATION" }, { "boost", "ANALOG BOOST" }, { "spread", "VOICE SPREAD" } })
            { knob (id, n, pink, 56, x2, c.getY() + 40, 120); x2 += 130; }
            label ("DRIFT lets every oscillator wander like a real analog poly. SATURATION drives each voice. "
                   "ANALOG BOOST fattens the low end. VOICE SPREAD places every note somewhere else in the stereo field.",
                   { x2 + 10, c.getY() + 44, c.getRight() - x2 - 30, 120 }, dim, 12.0f, Justification::topLeft);
        }
        void tick() override { tickKnobs(); }
    };
}
} // namespace bui

//==============================================================================
BullyEditor::BullyEditor (BullyProcessor& p) : AudioProcessorEditor (&p), proc (p), meter (p.engine.uiOut)
{
    setLookAndFeel (&look);
#if SKAZI_HAS_LOGO
    logo = ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
#endif
    const char* names[] = { "MAIN", "FX", "MATRIX", "ARP", "GLOBAL" };
    for (int i = 0; i < 5; ++i)
    {
        auto* t = tabs.add (new TextButton (names[i]));
        t->setColour (TextButton::buttonOnColourId, bui::cyan);
        t->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (t);
    }
    pages.push_back (std::make_unique<bui::MainPage> (p));
    pages.push_back (std::make_unique<bui::FxPage> (p));
    pages.push_back (std::make_unique<bui::MatrixPage> (p));
    pages.push_back (std::make_unique<bui::ArpPage> (p));
    pages.push_back (std::make_unique<bui::GlobalPage> (p));
    for (auto& pg : pages) addChildComponent (*pg);

    for (auto* b : { &undoBtn, &redoBtn, &prevBtn, &nextBtn, &randomBtn, &abBtn }) addAndMakeVisible (b);
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    prevBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() - 1); };
    nextBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() + 1); };
    abBtn.onClick = [this] { proc.switchAB (! proc.isOnB()); };
    abBtn.setTooltip ("Compare two versions: A / B");
    randomBtn.setColour (TextButton::buttonOnColourId, bui::pink);
    randomBtn.setToggleState (true, dontSendNotification);
    randomBtn.setTooltip ("FULL RANDOM: oscillators, warps, filters, envelopes, LFOs, matrix and FX all at once");
    randomBtn.onClick = [this] { proc.randomizeAll(); };

    const auto& list = proc.presetList();
    StringArray cats;
    for (auto& pr : list) cats.addIfNotAlreadyThere (pr.category);
    for (auto& cat : cats)
    {
        PopupMenu sub;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].category == cat) sub.addItem ((int) i + 1, list[i].name);
        presetBox.getRootMenu()->addSubMenu (cat + "  (" + String (sub.getNumItems()) + ")", sub);
    }
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.loadPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    master = std::make_unique<bui::Knob> (p, "vol", "MASTER", bui::cyan, 30);
    addAndMakeVisible (*master);
    addAndMakeVisible (meter);

    setSize (1240, 712);
    showPage (0);
    const auto startPage = SystemStats::getEnvironmentVariable ("BULLY_PAGE", {}); // for screenshots: open a page directly
    if (startPage.isNotEmpty()) showPage (jlimit (0, 4, startPage.getIntValue()));
    const auto startPreset = SystemStats::getEnvironmentVariable ("BULLY_PRESET", {});
    if (startPreset.isNotEmpty()) proc.loadPreset (startPreset.getIntValue());
    startTimerHz (30);
}

BullyEditor::~BullyEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BullyEditor::showPage (int i)
{
    page = i;
    for (int k = 0; k < (int) pages.size(); ++k) pages[(size_t) k]->setVisible (k == i);
    for (int k = 0; k < tabs.size(); ++k) tabs[k]->setToggleState (k == i, dontSendNotification);
}

void BullyEditor::resized()
{
    // trippy backdrop: a blurred rainbow swirl under a dark veil (rendered small, scaled up smooth)
    const int sw = 96, sh = 56;
    Image small (Image::RGB, sw, sh, false);
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x)
        {
            const float dx = (x - sw * 0.5f) / sw, dy = (y - sh * 0.45f) / sh;
            const float a = std::atan2 (dy, dx) / MathConstants<float>::twoPi + 0.5f;
            const float swirl = std::fmod (a + std::sqrt (dx * dx + dy * dy) * 0.9f + 1.0f, 1.0f);
            small.setPixelAt (x, y, Colour::fromHSV (std::fmod (0.83f + swirl, 1.0f), 0.95f, 1.0f, 1.0f));
        }
    backdrop = Image (Image::RGB, getWidth(), getHeight(), false);
    {
        Graphics g (backdrop);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (small, 0, 0, getWidth(), getHeight(), 0, 0, sw, sh);
        g.setColour (Colour (0xff07040f).withAlpha (0.74f));
        g.fillAll();
        Random r (7);
        for (int i = 0; i < 260; ++i) // a little star dust
        {
            g.setColour (Colours::white.withAlpha (0.05f + r.nextFloat() * 0.18f));
            const float s = 0.6f + r.nextFloat() * 1.4f;
            g.fillEllipse (r.nextFloat() * getWidth(), r.nextFloat() * getHeight(), s, s);
        }
        // glass header strip
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.08f), 0, 0, Colours::white.withAlpha (0.02f), 0, 64, false));
        g.fillRect (0, 0, getWidth(), 64);
        auto rb = bui::rainbow (0, 0, (float) getWidth(), 0);
        g.setGradientFill (rb);
        g.fillRect (0, 64, getWidth(), 2);
    }

    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (318 + i * 66, 20, 62, 26);
    undoBtn.setBounds (660, 20, 46, 26);
    redoBtn.setBounds (710, 20, 46, 26);
    abBtn.setBounds (760, 20, 26, 26);
    prevBtn.setBounds (792, 20, 24, 26);
    presetBox.setBounds (820, 20, 200, 26);
    nextBtn.setBounds (1024, 20, 24, 26);
    randomBtn.setBounds (1054, 20, 104, 26);
    master->setBounds (1160, 2, 52, bui::Knob::heightFor (30));
    meter.setBounds (1216, 10, 10, 46);
    for (auto& pg : pages) pg->setBounds (0, 74, getWidth(), getHeight() - 74);
}

void BullyEditor::paint (Graphics& g)
{
    g.drawImageAt (backdrop, 0, 0);
    // SKAZI logo, then the rainbow BULLY wordmark and byline
    if (logo.isValid())
    {
        g.setColour (Colours::black);
        g.fillRoundedRectangle (12, 8, 92, 50, 7.0f);
        g.setGradientFill (bui::rainbow (12, 8, 104, 58));
        g.drawRoundedRectangle (12.5f, 8.5f, 91, 49, 7.0f, 1.2f);
        g.drawImage (logo, Rectangle<float> (16, 10, 84, 46), RectanglePlacement::centred);
    }
    GlyphArrangement ga;
    ga.addLineOfText (Font (FontOptions ("Impact", 40.0f, Font::plain)).withExtraKerningFactor (0.08f), "BULLY", 116, 42);
    Path word;
    ga.createPath (word);
    DropShadow (Colour (0xffff00c8).withAlpha (0.7f), 12, {}).drawForPath (g, word);
    g.setGradientFill (bui::rainbow (116, 0, 116 + word.getBounds().getWidth(), 0));
    g.fillPath (word);
    GlyphArrangement by;
    by.addLineOfText (bui::font (13.0f).withExtraKerningFactor (0.2f), "BY ASHER SWISSA", 117, 58);
    Path byP;
    by.createPath (byP);
    g.setGradientFill (bui::rainbow (117, 0, 117 + byP.getBounds().getWidth(), 0));
    g.fillPath (byP);
}

void BullyEditor::refreshPresetBox()
{
    const auto& label = proc.getPresetLabel();
    if (label != shownPreset && ! presetBox.isPopupActive())
    {
        shownPreset = label;
        presetBox.setText (label, dontSendNotification);
    }
}

void BullyEditor::timerCallback()
{
    refreshPresetBox();
    abBtn.setButtonText (proc.isOnB() ? "B" : "A");
    meter.update();
    master->refreshMods();
    if (page >= 0 && page < (int) pages.size()) pages[(size_t) page]->tick();
}
