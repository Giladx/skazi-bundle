#pragma once

#include "../../Common/SkaziProcessor.h"
#include "Engine.h"
#include "Arp.h"

class BullyProcessor : public skazi::SkaziProcessor
{
public:
    BullyProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 8.0; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    // Mod matrix helpers for drag-and-drop: route `src` to the knob's parameter (returns the slot, -1 if full)
    int addModulation (int src, const juce::String& paramId, float amount = 35.0f);
    void removeModulation (int slot);
    // slots that modulate this knob, with their amounts (in %)
    std::vector<std::pair<int, float>> modsFor (const juce::String& paramId) const;
    void setParam (const juce::String& id, float value);

    bully::Engine engine;
    bully::Arp arp;
    juce::MidiKeyboardState keyboard;

    // Set by the on-screen wheels and PANIC; turned into MIDI at the start of the next block
    std::atomic<float> screenPitch { 0.0f }, screenMod { 0.0f };
    std::atomic<bool> pitchMoved { false }, modMoved { false }, panicRequested { false };

private:
    std::vector<std::atomic<float>*> raw;
    std::vector<float> values;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BullyProcessor)
};
