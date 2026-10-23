#pragma once

// Shared editor pieces for SKAZI plugins: metal frame, knobs, meters, choice buttons and the preset bar.
// Include from exactly one .cpp per plugin (the editor).
#include <juce_audio_processors/juce_audio_processors.h>
#include "MetalLookAndFeel.h"
#include "SkaziProcessor.h"
#if SHAK_HAS_LOGO
 #include "BinaryData.h"
#endif

namespace skazi
{
using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

inline juce::String formatValue (double v, const juce::String& suffix)
{
    using juce::String;
    if (suffix == "Hz")
        return v >= 1000.0 ? String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : String (juce::roundToInt (v)) + " Hz";
    if (suffix == "dB") return (v > 0.05 ? "+" : "") + String (v, 1) + " dB";
    if (suffix == "ms") return v >= 1000.0 ? String (v / 1000.0, 2) + " s" : String (v, v < 10.0 ? 1 : 0) + " ms";
    if (suffix == "s")  return String (v, 1) + " s";
    if (suffix == "st") return (v > 0.0 ? "+" : "") + String (juce::roundToInt (v)) + " st";
    if (suffix == "ct") return (v > 0.0 ? "+" : "") + String (juce::roundToInt (v)) + " ct";
    if (suffix == "oct") return (v > 0.0 ? "+" : "") + String (juce::roundToInt (v));
    if (suffix == "x")  return String (v, 2) + "x";
    if (suffix.isEmpty()) return String (juce::roundToInt (v));
    return String (juce::roundToInt (v)) + " " + suffix;
}

// Rotary knob with its name and value underneath
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& n,
          juce::Colour accent, const juce::String& sfx, int size)
        : name (n), suffix (sfx), knobSize (size)
    {
        using namespace juce;
        slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        slider.setColour (Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (slider);
        att = std::make_unique<SliderAtt> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            slider.setTooltip (p->getName (64));
        }
        value.setJustificationType (Justification::centred);
        value.setFont (skin::font (knobSize < 44 ? 10.0f : 11.5f));
        value.setColour (Label::textColourId, skin::white);
        value.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (value);
        slider.onValueChange = [this] { refreshText(); };
        refreshText();
    }

    void setFormatter (std::function<juce::String (double)> f) { formatter = std::move (f); refreshText(); }

    void resized() override
    {
        slider.setBounds ((getWidth() - knobSize) / 2, 0, knobSize, knobSize);
        value.setBounds (0, knobSize + (knobSize < 44 ? 11 : 14), getWidth(), 14);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (nameColour);
        g.setFont (skin::font (knobSize < 44 ? 9.0f : 10.0f));
        g.drawText (name, 0, knobSize - 1, getWidth(), 13, juce::Justification::centred);
    }

    static int heightFor (int knobSize) { return knobSize + (knobSize < 44 ? 26 : 30); }

    juce::Slider slider;
    juce::Colour nameColour = skin::textDim;

private:
    void refreshText()
    {
        value.setText (formatter ? formatter (slider.getValue()) : formatValue (slider.getValue(), suffix), juce::dontSendNotification);
    }

    juce::String name, suffix;
    int knobSize;
    std::unique_ptr<SliderAtt> att;
    juce::Label value;
    std::function<juce::String (double)> formatter;
};

// Vertical meter: level in dB, or gain reduction filled from the top
class Meter : public juce::Component
{
public:
    Meter (std::atomic<float>& src, bool reduction = false) : source (src), isReduction (reduction) {}
    void update() { const float v = source.load(); if (std::abs (v - shown) > 0.003f) { shown = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff0b0b0c));
        g.fillRoundedRectangle (r, 3.0f);
        auto in = r.reduced (1.5f);
        const float v = jmax (0.0f, shown);
        if (isReduction)
        {
            const float h = jlimit (0.0f, 1.0f, 1.0f - v) * in.getHeight();
            g.setGradientFill (ColourGradient (skin::red, 0, in.getY(), Colour (0xffff5a1f), 0, in.getY() + h + 1, false));
            g.fillRect (in.withHeight (h));
        }
        else
        {
            const float h = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f) * in.getHeight();
            ColourGradient grad (Colour (0xff22e3a5), 0, in.getBottom(), skin::red, 0, in.getY(), false);
            grad.addColour (0.7, Colour (0xffffe14d));
            g.setGradientFill (grad);
            g.fillRect (in.withTop (in.getBottom() - h));
        }
        g.setColour (skin::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }

private:
    std::atomic<float>& source;
    bool isReduction;
    float shown = -1.0f;
};

// A row of buttons bound to a choice parameter; each shows a little drawn icon or a text label
class ChoiceRow : public juce::Component
{
public:
    using Icon = std::function<float (float)>; // x 0..1 -> y -1..1; empty = text label

    ChoiceRow (juce::AudioProcessorValueTreeState& s, const juce::String& id, std::vector<Icon> icons = {}, juce::Colour accent = skin::red2)
        : state (s), paramId (id), shapes (std::move (icons)), colour (accent)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
            labels = c->choices;
    }

    void update()
    {
        const int sel = (int) state.getRawParameterValue (paramId)->load();
        if (sel != selected) { selected = sel; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const int n = labels.size();
        if (n == 0) return;
        const float gap = 3.0f, w = (getWidth() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
        {
            auto r = Rectangle<float> (i * (w + gap), 0, w, (float) getHeight()).reduced (0.5f);
            const bool on = i == selected;
            g.setColour (on ? Colour (0xff1e1012) : skin::panel2);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? colour : (i == hover ? skin::textDim : skin::line));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            if (i < (int) shapes.size() && shapes[(size_t) i])
            {
                auto a = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.25f);
                Path p;
                for (int k = 0; k <= 32; ++k)
                {
                    const float x = k / 32.0f;
                    const float y = a.getCentreY() - jlimit (-1.0f, 1.0f, shapes[(size_t) i] (x)) * a.getHeight() * 0.5f;
                    if (k == 0) p.startNewSubPath (a.getX() + x * a.getWidth(), y);
                    else        p.lineTo (a.getX() + x * a.getWidth(), y);
                }
                g.setColour (on ? colour : skin::textDim);
                g.strokePath (p, PathStrokeType (1.5f));
            }
            else
            {
                g.setColour (on ? skin::white : skin::textDim);
                g.setFont (skin::font (jmin (10.5f, r.getHeight() * 0.42f)));
                g.drawFittedText (labels[i], r.toNearestInt().reduced (2, 0), Justification::centred, 2);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.x);
        if (i < 0 || ! getLocalBounds().contains (e.getPosition())) return;
        if (auto* prm = state.getParameter (paramId))
        {
            if (auto* p = dynamic_cast<SkaziProcessor*> (&state.processor)) p->undo.beginNewTransaction();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        }
    }

private:
    int indexAt (int x) const { return labels.isEmpty() ? -1 : juce::jlimit (0, labels.size() - 1, x * labels.size() / juce::jmax (1, getWidth())); }

    juce::AudioProcessorValueTreeState& state;
    juce::String paramId;
    juce::StringArray labels;
    std::vector<Icon> shapes;
    juce::Colour colour;
    int selected = -1, hover = -1;
};

// A toggle button bound to a bool parameter, lit red when on
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text) : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        att = std::make_unique<ButtonAtt> (s, id, *this);
    }
private:
    std::unique_ptr<ButtonAtt> att;
};

// Undo, Redo, A, B, < preset >, RANDOM
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (SkaziProcessor& p) : proc (p)
    {
        using namespace juce;
        for (auto* b : { &undoBtn, &redoBtn, &aBtn, &bBtn, &prevBtn, &nextBtn, &randomBtn })
            addAndMakeVisible (b);
        undoBtn.onClick = [this] { proc.undo.undo(); };
        redoBtn.onClick = [this] { proc.undo.redo(); };
        aBtn.onClick = [this] { proc.switchAB (false); };
        bBtn.onClick = [this] { proc.switchAB (true); };
        prevBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() - 1); };
        nextBtn.onClick = [this] { proc.loadPreset (proc.getCurrentPreset() + 1); };
        randomBtn.onClick = [this] { proc.randomizeAll(); };
        randomBtn.setToggleState (true, dontSendNotification); // always drawn lit red
        randomBtn.setTooltip ("Random sound");

        const auto& list = proc.presetList();
        StringArray cats;
        for (auto& pr : list) cats.addIfNotAlreadyThere (pr.category);
        for (auto& cat : cats)
        {
            PopupMenu sub;
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i].category == cat)
                    sub.addItem ((int) i + 1, list[i].name);
            box.getRootMenu()->addSubMenu (cat + "  (" + String (sub.getNumItems()) + ")", sub);
        }
        box.setText (proc.getPresetLabel(), dontSendNotification);
        box.onChange = [this] { if (box.getSelectedId() > 0) proc.loadPreset (box.getSelectedId() - 1); };
        addAndMakeVisible (box);
    }

    void update()
    {
        if (box.getText() != proc.getPresetLabel() && ! box.isPopupActive())
            box.setText (proc.getPresetLabel(), juce::dontSendNotification);
        aBtn.setToggleState (! proc.isOnB(), juce::dontSendNotification);
        bBtn.setToggleState (proc.isOnB(), juce::dontSendNotification);
    }

    void resized() override
    {
        const int h = getHeight();
        int x = 0;
        auto place = [&] (juce::Component& c, int w) { c.setBounds (x, 0, w, h); x += w + 4; };
        place (undoBtn, 48); place (redoBtn, 48); x += 2;
        place (aBtn, 28); place (bBtn, 28); x += 4;
        place (prevBtn, 22);
        const int boxW = getWidth() - x - 22 - 4 - 92 - 6;
        place (box, boxW); place (nextBtn, 22); x += 2;
        place (randomBtn, 92);
    }

private:
    SkaziProcessor& proc;
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" }, aBtn { "A" }, bBtn { "B" }, prevBtn { "<" }, nextBtn { ">" }, randomBtn { "RANDOM" };
    juce::ComboBox box;
};

// Metal faceplate editor: logo, two-tone engraved title, recessed panels, footer and screws
class EditorBase : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    EditorBase (SkaziProcessor& p, juce::String titleDark, juce::String titleRed, juce::String subtitle, juce::String footerText)
        : AudioProcessorEditor (&p), proc (p), presetBar (p),
          title1 (std::move (titleDark)), title2 (std::move (titleRed)), sub (std::move (subtitle)), footer (std::move (footerText))
    {
        setLookAndFeel (&lnf);
#if SHAK_HAS_LOGO
        logo = juce::ImageCache::getFromMemory (BinaryData::skazi_logo_png, BinaryData::skazi_logo_pngSize);
#endif
        addAndMakeVisible (presetBar);
        addMouseListener (this, true);
        startTimerHz (30);
    }

    ~EditorBase() override
    {
        stopTimer();
        removeMouseListener (this);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.drawImageAt (faceplate, 0, 0);
        skin::drawLogoBadge (g, { 40.0f, 12.0f, 150.0f, 46.0f }, logo);
        const auto tf = skin::titleFont (34.0f);
        const float w1 = GlyphArrangement::getStringWidth (tf, title1);
        skin::engrave (g, title1, { 204, 8, (int) w1 + 4, 38 }, tf, Colour (0xff26282c), Justification::left);
        skin::engrave (g, title2, { 204 + (int) w1, 8, 200, 38 }, tf, skin::red, Justification::left);
        skin::engrave (g, sub, { 205, 42, 300, 14 }, skin::font (10.5f), Colour (0xff3b3e43), Justification::left);

        for (auto& p : panels)
        {
            skin::drawInsetPanel (g, p.area.toFloat());
            if (p.title.isNotEmpty())
            {
                g.setColour (skin::white);
                g.setFont (skin::font (12.0f));
                g.drawText (p.title, p.area.getX() + 10, p.area.getY() + 7, 140, 18, Justification::left);
            }
        }
        for (auto& l : labels)
        {
            g.setColour (skin::textDim);
            g.setFont (skin::font (9.5f));
            g.drawText (l.text, l.area, Justification::centredLeft);
        }
        paintExtras (g);

        const int fy = getHeight() - 34;
        skin::engrave (g, footer, { 40, fy, 500, 18 }, skin::font (10.0f), Colour (0xff4a4d52), Justification::centredLeft);
        skin::engrave (g, "SKAZI", { getWidth() - 240, fy, 200, 18 }, skin::font (11.0f), Colour (0xff4a4d52), Justification::centredRight);
        Random r (11);
        const float w = (float) getWidth(), h = (float) getHeight();
        for (auto pt : { Point<float> (17, 17), Point<float> (w - 17, 17), Point<float> (17, h - 17), Point<float> (w - 17, h - 17) })
            skin::drawScrew (g, pt, 7.5f, r.nextFloat() * MathConstants<float>::pi);
    }

    void resized() override
    {
        faceplate = skin::renderBrushedMetal (getWidth(), getHeight());
        presetBar.setBounds (getWidth() - 600, 22, 566, 26);
        panels.clear();
        labels.clear();
        layout();
    }

    void mouseDown (const juce::MouseEvent&) override { proc.undo.beginNewTransaction(); }

protected:
    virtual void layout() = 0;           // place components, add panels and labels
    virtual void tick() {}               // 30 times a second
    virtual void paintExtras (juce::Graphics&) {}

    Knob& addKnob (const juce::String& id, const juce::String& name, juce::Colour c, const juce::String& suffix, int size = 40)
    {
        auto& k = *knobs.emplace_back (std::make_unique<Knob> (proc.apvts, id, name, c, suffix, size));
        addAndMakeVisible (k);
        return k;
    }

    ChoiceRow& addChoiceRow (const juce::String& id, std::vector<ChoiceRow::Icon> icons = {}, juce::Colour c = skin::red2)
    {
        auto& row = *choiceRows.emplace_back (std::make_unique<ChoiceRow> (proc.apvts, id, std::move (icons), c));
        addAndMakeVisible (row);
        return row;
    }

    ParamToggle& addToggle (const juce::String& id, const juce::String& text)
    {
        auto& t = *toggles.emplace_back (std::make_unique<ParamToggle> (proc.apvts, id, text));
        addAndMakeVisible (t);
        return t;
    }

    juce::ComboBox& addCombo (const juce::String& id)
    {
        auto& box = *combos.emplace_back (std::make_unique<juce::ComboBox>());
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (c->choices, 1);
        addAndMakeVisible (box);
        comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, id, box));
        return box;
    }

    Meter& addMeter (std::atomic<float>& src, bool reduction = false)
    {
        auto& m = *meters.emplace_back (std::make_unique<Meter> (src, reduction));
        addAndMakeVisible (m);
        return m;
    }

    void panel (juce::Rectangle<int> area, const juce::String& title) { panels.push_back ({ area, title }); }
    void label (juce::Rectangle<int> area, const juce::String& text) { labels.push_back ({ area, text }); }

    // place a knob with its top-left corner at (x, y)
    static void put (Knob& k, int x, int y, int knobSize, int width = 0)
    {
        k.setBounds (x, y, width > 0 ? width : knobSize + 18, Knob::heightFor (knobSize));
    }

    SkaziProcessor& proc;
    skin::MetalLookAndFeel lnf;
    juce::TooltipWindow tooltips { this };

private:
    void timerCallback() override
    {
        presetBar.update();
        for (auto& m : meters) m->update();
        for (auto& r : choiceRows) r->update();
        tick();
    }

    struct Panel { juce::Rectangle<int> area; juce::String title; };
    struct Label { juce::Rectangle<int> area; juce::String text; };

    PresetBar presetBar;
    juce::Image faceplate, logo;
    juce::String title1, title2, sub, footer;
    std::vector<Panel> panels;
    std::vector<Label> labels;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<ChoiceRow>> choiceRows;
    std::vector<std::unique_ptr<ParamToggle>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::vector<std::unique_ptr<Meter>> meters;
};

// Wave icons used by several plugins
namespace icons
{
    inline float sine (float x)   { return std::sin (x * juce::MathConstants<float>::twoPi); }
    inline float tri (float x)    { return 1.0f - 4.0f * std::abs (x - 0.5f); }
    inline float saw (float x)    { return 1.0f - 2.0f * x; }
    inline float square (float x) { return x < 0.5f ? 0.8f : -0.8f; }
    inline float pulse (float x)  { return x < 0.2f ? 0.8f : -0.8f; }
    inline float fm (float x)     { return std::sin (juce::MathConstants<float>::twoPi * x + 2.0f * std::sin (2.0f * juce::MathConstants<float>::twoPi * x)); }
    inline float noise (float x)  { return std::sin (x * 97.0f) * std::cos (x * 41.0f); }
    inline float sh (float x)     { static const float v[] = { 0.6f, -0.4f, 0.9f, -0.8f, 0.2f }; return v[juce::jlimit (0, 4, (int) (x * 5))]; }
}
} // namespace skazi
