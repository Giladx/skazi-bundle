#pragma once

#include "../../Common/SkaziProcessor.h"
#include "KubeEngine.h"

class KubeProcessor : public skazi::SkaziProcessor
{
public:
    KubeProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 6.0; } // release, delay and reverb tails

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    // Loaded wavetables travel inside the saved state, so a project opens with them even if the .wav moved
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Loads a Serum-style .wav into an oscillator's USER slot and switches the oscillator to it (message thread)
    bool loadUserTable (int osc, const juce::File& file, juce::String& error);
    const kube::Wavetable* userTable (int osc) const { return user[(size_t) osc].get(); }

    kube::Settings settings() const { return kube::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    kube::Engine engine;
    juce::MidiKeyboardState keyboardState; // the on-screen keyboard

private:
    void setUserTable (int osc, std::unique_ptr<kube::Wavetable> table);

    std::array<std::unique_ptr<kube::Wavetable>, 2> user;
    std::vector<std::unique_ptr<kube::Wavetable>> retired; // replaced tables stay alive: the audio thread may still be reading one

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KubeProcessor)
};
