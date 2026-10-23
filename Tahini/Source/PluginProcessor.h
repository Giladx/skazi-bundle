#pragma once

#include "../../Common/SkaziProcessor.h"
#include "TahiniEngine.h"

class TahiniProcessor : public skazi::SkaziProcessor
{
public:
    TahiniProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    tahini::Settings settings() const { return tahini::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    tahini::Engine engine;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TahiniProcessor)
};
