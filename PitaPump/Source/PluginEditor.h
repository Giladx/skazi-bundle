#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MetalLookAndFeel.h"

using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

// Rotary knob with its name and value underneath
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
          juce::Colour accent, const juce::String& suffix, int knobSize);
    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Slider slider;

private:
    juce::String name, suffix;
    int knobSize;
    std::unique_ptr<SliderAtt> att;
    juce::Label value;
};

// One of the shape buttons: a mini picture of the curve
class CurveButton : public juce::Component
{
public:
    CurveButton (PitaPumpProcessor& p, int shape) : proc (p), shape (shape) {}
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }

private:
    PitaPumpProcessor& proc;
    int shape;
    bool selected = false;
};

// Vertical level meter; GR meters fill from the top
class Meter : public juce::Component
{
public:
    Meter (std::atomic<float>& src, bool reduction) : source (src), isReduction (reduction) {}
    void paint (juce::Graphics& g) override;
    void update() { const float v = source.load(); if (std::abs (v - shown) > 0.003f) { shown = v; repaint(); } }

private:
    std::atomic<float>& source;
    bool isReduction;
    float shown = -1.0f;
};

// The big curve display; in DRAW mode you click to add points, drag to move, right-click to delete
class PumpDisplay : public juce::Component
{
public:
    explicit PumpDisplay (PitaPumpProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; }

private:
    juce::Rectangle<float> curveArea() const;
    juce::Point<float> toScreen (const pump::Point& pt, int cycle, const pump::Params& p) const;
    pump::Point fromScreen (juce::Point<float> pos, const pump::Params& p) const;

    PitaPumpProcessor& proc;
    int dragIndex = -1;
};

class PitaPumpEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PitaPumpEditor (PitaPumpProcessor&);
    ~PitaPumpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title);
    Knob& addKnob (const juce::String& id, const juce::String& name, juce::Colour c, const juce::String& suffix, int size = 50);
    void setupCombo (juce::ComboBox& box, const juce::String& paramId, std::unique_ptr<ComboAtt>& att);
    void setChoice (const juce::String& id, int index);

    PitaPumpProcessor& proc;
    skin::MetalLookAndFeel lnf;
    juce::TooltipWindow tooltips { this };
    juce::Image faceplate, logo;

    PumpDisplay display;
    std::vector<std::unique_ptr<CurveButton>> shapeButtons;
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" }, aBtn { "A" }, bBtn { "B" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, randomBtn { "RANDOM" };
    juce::TextButton syncBtn { "SYNC" }, midiBtn { "MIDI" }, audioBtn { "AUDIO" }, autoBtn { "AUTO" };
    std::unique_ptr<ButtonAtt> autoAtt;
    juce::ComboBox presetBox, rateBox, snapBox;
    std::unique_ptr<ComboAtt> rateAtt, snapAtt;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *phase = nullptr, *swing = nullptr, *thresh = nullptr, *depth = nullptr, *curve = nullptr, *attack = nullptr,
         *release = nullptr, *smooth = nullptr, *pregain = nullptr, *mix = nullptr, *gain = nullptr;
    Meter inMeter, grMeter, outMeter;

    juce::Rectangle<int> triggerArea, shapeArea, preampArea, outputArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitaPumpEditor)
};
