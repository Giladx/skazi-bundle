#pragma once

// BULLY editor: trippy rainbow backdrop, glass panels, polished metal knobs with neon value rings.
// Drag any LFO / ENV / CHAOS / MACRO tag onto a knob to modulate it; the white ring shows the range.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace bui
{
const juce::Colour pink { 0xffff2bd6 }, cyan { 0xff00f0ff }, green { 0xff39ff14 }, yellow { 0xffffe600 },
                   orange { 0xffff6a00 }, purple { 0xff8a3bff }, red { 0xffff1f4b }, text { 0xffeeeaff }, dim { 0xffa79fcf };
juce::Font font (float size, bool bold = true);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.5f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return font (13.0f, false); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
};

// Slider that also handles right-click (mod menu) and ALT-drag (mod amount)
class ModSlider : public juce::Slider
{
public:
    std::function<void()> onPopup;
    std::function<void (float)> onModDrag; // vertical drag in pixels while ALT is held
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
private:
    bool modDrag = false;
    float lastY = 0.0f;
};

// Metal knob with name and value, a drop target for modulation sources
class Knob : public juce::Component, public juce::DragAndDropTarget
{
public:
    Knob (BullyProcessor&, const juce::String& paramId, const juce::String& name, juce::Colour accent, int size);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void refreshMods();               // called by the editor timer
    static int heightFor (int size) { return size + 24; }

    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("src:"); }
    void itemDragEnter (const SourceDetails&) override { hot = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { hot = false; repaint(); }
    void itemDropped (const SourceDetails& d) override;

    ModSlider slider;
    juce::String paramId;
private:
    void showModMenu();
    juce::String valueText() const;
    BullyProcessor& proc;
    juce::String name;
    int knobSize, dest = -1;
    bool exactDest = false, hot = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    float shownLo = -1, shownHi = -1, shownNow = -1;
};

// A tag you can click (select) and drag onto knobs (modulation source)
class SourceTag : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceTag (int src, juce::String label, juce::Colour c);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool selected = false;
    float level = 0.0f; // live value, drawn in the dot
    std::function<void()> onClick;
    int source;
private:
    juce::String label;
    juce::Colour colour;
    bool dragging = false;
};

// Bound controls
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (BullyProcessor&, const juce::String& id);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class ParamButton : public juce::TextButton
{
public:
    ParamButton (BullyProcessor&, const juce::String& id, const juce::String& text, juce::Colour on);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// One button per choice of a choice parameter
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (BullyProcessor&, const juce::String& id, juce::Colour c, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void update();
private:
    BullyProcessor& proc;
    juce::String id;
    juce::StringArray items;
    juce::Colour colour;
    int sel = -1;
};

// Displays
class WaveView : public juce::Component
{
public:
    WaveView (BullyProcessor& p, int osc) : proc (p), o (osc) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    BullyProcessor& proc;
    int o;
    float lastPos = -1, lastW[4] {}; int lastTab = -1;
};

class FilterView : public juce::Component
{
public:
    FilterView (BullyProcessor& p, int f) : proc (p), fi (f) {}
    void paint (juce::Graphics&) override;
    void update();
private:
    BullyProcessor& proc;
    int fi;
    float last[5] { -1, -1, -1, -1, -1 };
};

class LfoView : public juce::Component
{
public:
    explicit LfoView (BullyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    int index = 0;
private:
    BullyProcessor& proc;
};

class EnvView : public juce::Component
{
public:
    explicit EnvView (BullyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    int index = 0;
private:
    BullyProcessor& proc;
};

class ChaosView : public juce::Component
{
public:
    explicit ChaosView (BullyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
    int index = 0;
private:
    BullyProcessor& proc;
    std::array<juce::Point<float>, 160> trail;
    int head = 0, filled = 0;
};

class Wheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    Wheel (juce::String name, bool bipolar, std::function<float()> read, std::function<void (float)> write);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { dragging = true; mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    juce::String name;
    bool centred, dragging = false;
    float value = 0.0f;
    std::function<float()> read;
    std::function<void (float)> write;
};

class Meter : public juce::Component
{
public:
    explicit Meter (std::atomic<float>& s) : src (s) {}
    void paint (juce::Graphics&) override;
    void update() { const float v = src.load(); if (std::abs (v - shown) > 0.003f) { shown = v; repaint(); } }
private:
    std::atomic<float>& src;
    float shown = 0.0f;
};

// A page of panels. Draws its glass panels from a cached image; owns its controls.
class Page : public juce::Component
{
public:
    explicit Page (BullyProcessor& p) : proc (p) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics& g) override;
    void resized() override { layer = {}; }
    virtual void tick() {}

    struct Panel { juce::Rectangle<int> r; juce::String title; juce::Colour colour; };
    std::vector<Panel> panels;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<ChoiceButtons*> choiceRows;

protected:
    Knob& knob (const juce::String& id, const juce::String& name, juce::Colour c, int size, int x, int y, int cell = 0);
    template <typename T> T& own (std::unique_ptr<T> c) { auto& r = *c; addAndMakeVisible (r); owned.push_back (std::move (c)); return r; }
    ParamCombo& combo (const juce::String& id, juce::Rectangle<int> r);
    ParamButton& toggle (const juce::String& id, const juce::String& text, juce::Colour c, juce::Rectangle<int> r);
    ChoiceButtons& choices (const juce::String& id, juce::Colour c, juce::Rectangle<int> r, juce::StringArray labels = {});
    void label (const juce::String& text, juce::Rectangle<int> r, juce::Colour c = dim, float size = 10.0f, juce::Justification j = juce::Justification::centredLeft);
    void tickKnobs();
    BullyProcessor& proc;
private:
    juce::Image layer;
};
} // namespace bui

class BullyEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit BullyEditor (BullyProcessor&);
    ~BullyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int i);
    void refreshPresetBox();

    BullyProcessor& proc;
    bui::Look look;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Image backdrop, logo;
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<bui::Page>> pages;
    int page = 0;
    juce::TextButton undoBtn { "UNDO" }, redoBtn { "REDO" }, prevBtn { "<" }, nextBtn { ">" }, randomBtn { "FULL RANDOM" }, abBtn { "A" };
    juce::ComboBox presetBox;
    juce::String shownPreset;
    std::unique_ptr<bui::Knob> master;
    bui::Meter meter;
};
