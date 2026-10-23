#pragma once

#include "../../Common/SkaziProcessor.h"
#include "ZhugEngine.h"

class ZhugProcessor : public skazi::SkaziProcessor
{
public:
    ZhugProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    zhug::Settings settings() const { return zhug::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    zhug::Engine engine;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZhugProcessor)
};
