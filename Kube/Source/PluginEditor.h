#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace kube { class PsyLook; struct Ui; }

// Neon psychedelic tabbed editor: OSC / WAVE, FILTER / ENV, MOD / VOICE, FX RACK, with a live scope and keyboard.
class KubeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KubeEditor (KubeProcessor&);
    ~KubeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();

    KubeProcessor& proc;
    std::unique_ptr<kube::PsyLook> look; // outlives the components that draw with it
    std::unique_ptr<kube::Ui> ui;
    float spin = 0.0f;
    int shownVoices = -1, frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KubeEditor)
};
