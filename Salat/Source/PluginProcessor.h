#pragma once

#include "../../Common/SkaziProcessor.h"
#include "SalatEngine.h"

class SalatProcessor : public skazi::SkaziProcessor
{
public:
    SalatProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; } // delay and reverb tails

    const std::vector<skazi::Preset>& presetList() const override;
    skazi::Preset randomPreset (juce::Random& rng) const override;

    // The big button: a new random salat, and the next four steps get chopped whatever CHANCE says
    void makeASalat() { randomizeAll(); engine.burst (4); }

    salat::Settings settings() const { return salat::Settings::fromValues ([this] (const char* id) { return v (id); }); }

    salat::Engine engine;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SalatProcessor)
};
