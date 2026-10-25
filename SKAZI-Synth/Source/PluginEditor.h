#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "AcidLookAndFeel.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with caption and APVTS attachment
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent, bool bipolar = false);
    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Slider slider;

private:
    juce::String text;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Row (or grid) of buttons bound to a choice / int parameter
class ChoiceButtons : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceButtons (APVTS& state, const juce::String& paramId, juce::StringArray labels, juce::Colour accent, int rows = 1);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    int getIndex() const;

private:
    juce::RangedAudioParameter* param;
    juce::StringArray labels;
    juce::Colour colour;
    int rows;
};

class MainView;

// The editor hosts one fixed-size MainView (1340 x 1000) and scales it, so the
// window can be resized freely while keeping its proportions. Rendering goes
// through OpenGL for smooth 60 fps displays.
class AcidanceEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcidanceEditor (AcidanceProcessor&);
    ~AcidanceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void showPresetBrowser();
    void parentHierarchyChanged() override;

    static constexpr int baseWidth = 1340, baseHeight = 1000;

private:
    AcidanceProcessor& proc;
    AcidLookAndFeel lnf;
    std::unique_ptr<MainView> view;
    juce::OpenGLContext glContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidanceEditor)
};
