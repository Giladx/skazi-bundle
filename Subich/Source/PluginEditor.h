#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "../../Common/SkaziUI.h"

// Live output waveform, locked to the note so it stands still
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (SubichProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    SubichProcessor& proc;
};

// Combined response of both filters at their current (modulated) cutoffs
class FilterView : public juce::Component
{
public:
    explicit FilterView (SubichProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    SubichProcessor& proc;
};

// ADSR drawing for one envelope
class EnvView : public juce::Component
{
public:
    EnvView (SubichProcessor& p, juce::String prefix, juce::Colour c) : proc (p), pre (std::move (prefix)), colour (c) {}
    void paint (juce::Graphics& g) override;
private:
    SubichProcessor& proc;
    juce::String pre;
    juce::Colour colour;
};

// A hardware-style wheel: drag up and down. The PITCH wheel springs back to the middle.
class PerfWheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    PerfWheel (juce::String label, bool bipolar, std::function<float()> current, std::function<void (float)> moved)
        : name (std::move (label)), centred (bipolar), read (std::move (current)), write (std::move (moved)) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override { dragging = true; mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    // follow the engine (incoming MIDI wheels) unless the mouse is holding the wheel
    void refresh() { if (dragging) return; const float v = read(); if (std::abs (v - value) > 0.002f) { value = v; repaint(); } }

private:
    juce::String name;
    bool centred, dragging = false;
    float value = 0.0f;
    std::function<float()> read;
    std::function<void (float)> write;
};

class SubichEditor : public skazi::EditorBase
{
public:
    explicit SubichEditor (SubichProcessor&);

protected:
    void layout() override;
    void tick() override;

private:
    SubichProcessor& sp;
    ScopeView scope;
    FilterView filter;
    EnvView ampEnv, filtEnv;
    juce::MidiKeyboardComponent keys;
    PerfWheel pitchWheel, modWheel;
    juce::TextButton panicBtn { "PANIC" }, randomBtn { "RANDOM" };
    skazi::Knob* glideBig;

    skazi::Knob *o1oct, *o1semi, *o1fine, *o1shape, *o1level;
    skazi::Knob *o2oct, *o2semi, *o2detune, *o2shape, *o2level;
    skazi::Knob *sublevel, *noise;
    skazi::Knob *f1[5], *f2[5];
    skazi::Knob *amp[4], *fenv[4];
    skazi::Knob *l1hz, *l1depth, *l2hz, *l2depth;
    skazi::Knob *glide, *velo, *bend, *drive, *vol;
    skazi::ChoiceRow *o1wave, *o2wave, *subwave, *suboct, *routing, *l1shape, *l2shape, *mode;
    skazi::ParamToggle *o2sync, *o2fm, *l1sync, *l2sync;
    juce::ComboBox *f1type, *f2type, *l1rate, *l2rate, *l1dest, *l2dest;
    skazi::Meter* outMeter;
};
