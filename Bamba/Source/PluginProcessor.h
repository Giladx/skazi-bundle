#pragma once

#include "../../Common/SkaziProcessor.h"
#include "BambaEngine.h"

class BambaProcessor : public skazi::SkaziProcessor
{
public:
    BambaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 6.0; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    bamba::Settings settings() const { return bamba::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    bamba::Engine engine;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BambaProcessor)
};
