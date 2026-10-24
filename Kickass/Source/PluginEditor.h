#pragma once

#include "PluginProcessor.h"
#include "Skin.h"
#include "WaveformDisplay.h"
#include "KickLibrary.h"

/** Rotary knob with its label above and value below, bound to one parameter. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text, bool big = false);
    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** The whole front panel, laid out at a fixed design size. */
class KickPanel : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    static constexpr int designWidth = 1280, designHeight = 660;

    explicit KickPanel (KickassProcessor&);
    ~KickPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // Drop a WAV/AIFF anywhere on the panel to use it as the click layer.
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }

private:
    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    Knob& knob (const juce::String& id, const juce::String& text, bool big = false);
    void drawLogo (juce::Graphics&, juce::Rectangle<int>);
    void drawMeter (juce::Graphics&, juce::Rectangle<int>);
    void chooseClickSample();
    void showSampleMenu();
    void loadKick (const juce::File&);
    void stepKick (int delta);
    void showPresetMenu();
    void savePresetAs();

    KickassProcessor& processor;
    skin::LookAndFeel lnf;

    std::map<juce::String, std::unique_ptr<Knob>> knobs;
    WaveformDisplay display;
    juce::TextButton hitBtn { "HIT" }, loadBtn { "LOAD" }, saveBtn { "SAVE" }, keyTrackBtn { "KEY TRACK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyTrackAttachment;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::vector<Section> sections;
    juce::Rectangle<int> logoArea, presetArea, ledArea, meterArea, sampleArea, keyboardFrame;

    kick::Params shownParams;
    kick::ClickSamplePtr shownSample;
    juce::String shownPresetName;

    juce::SharedResourcePointer<KickLibrary> library;
    std::vector<KickLibrary::Entry> libraryEntries;
    int shownLibraryVersion = -1;
    bool shownScanning = false;
    bool firstRender = true, dragHover = false;
    int lastHitCount = 0;
    float flash = 0.0f, meterLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickPanel)
};

/** Hosts the panel and scales it, so the window can be resized to fit any screen. */
class KickassEditor : public juce::AudioProcessorEditor
{
public:
    explicit KickassEditor (KickassProcessor&);
    void resized() override;

private:
    KickPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickassEditor)
};
