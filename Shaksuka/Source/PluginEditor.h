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
    juce::Colour nameColour = skin::textDim;

private:
    juce::String name, suffix;
    int knobSize;
    std::unique_ptr<SliderAtt> att;
    juce::Label value;
};

// One effect in the rack: on/off switch, amount knob; glows while the effect is playing
class FxCell : public juce::Component
{
public:
    FxCell (juce::AudioProcessorValueTreeState& state, int fx);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void setHot (bool h) { if (h != hot) { hot = h; repaint(); } }

private:
    int fx;
    bool hot = false;
    juce::ToggleButton toggle;
    std::unique_ptr<ButtonAtt> toggleAtt;
    Knob knob;
};

// Waveform of the chopped output with slice markers, plus the step pattern strip
class WaveView : public juce::Component
{
public:
    explicit WaveView (ShaksukaProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;

private:
    ShaksukaProcessor& proc;
};

class ShaksukaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShaksukaEditor (ShaksukaProcessor&);
    ~ShaksukaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void renderFaceplate();
    void drawLogo (juce::Graphics& g, juce::Rectangle<float> r);
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title);
    Knob& addKnob (const juce::String& id, const juce::String& name, juce::Colour c, const juce::String& suffix, int size = 54);
    void setupCombo (juce::ComboBox& box, const juce::String& paramId, std::unique_ptr<ComboAtt>& att);

    ShaksukaProcessor& proc;
    skin::MetalLookAndFeel lnf;
    juce::Image faceplate, logo;

    juce::TooltipWindow tooltips { this };
    WaveView wave;
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" }, aBtn { "A" }, bBtn { "B" }, diceBtn { "DICE" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, randomBtn { "RANDOM" };
    juce::TextButton syncBtn { "SYNC" }, freeBtn { "FREE" };
    juce::ComboBox presetBox, gridBox, barsBox, grooveBox;
    std::unique_ptr<ComboAtt> gridAtt, barsAtt, grooveAtt;
    juce::Slider seed;
    std::unique_ptr<SliderAtt> seedAtt;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob *freeMs = nullptr, *lenVar = nullptr, *swing = nullptr, *humanize = nullptr, *chaos = nullptr,
         *stack = nullptr, *evolve = nullptr, *mix = nullptr, *gain = nullptr;
    std::vector<std::unique_ptr<FxCell>> fxCells;

    juce::Rectangle<int> slicingArea, grooveArea, chaosArea, outputArea, rackArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaksukaEditor)
};
