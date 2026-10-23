#pragma once

#include "../../Common/SkaziProcessor.h"
#include "SynthEngine.h"

class SubichProcessor : public skazi::SkaziProcessor
{
public:
    SubichProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 3.0; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    subich::Settings settings() const;

    subich::Engine engine;
    juce::MidiKeyboardState keyboard;

    // Set by the on-screen wheels and PANIC button; turned into MIDI at the start of the next block
    std::atomic<float> screenPitch { 0.0f }, screenMod { 0.0f }; // -1..1, 0..1
    std::atomic<bool> pitchMoved { false }, modMoved { false }, panicRequested { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubichProcessor)
};
