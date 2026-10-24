#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Common/PresetFader.h"
#include <map>
#include "PumpEngine.h"

class PitaPumpProcessor : public juce::AudioProcessor
{
public:
    PitaPumpProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets (see Presets.h); index wraps around so prev/next can cycle
    void loadPreset (int index);
    void randomizeAll();
    int getCurrentPreset() const { return currentPreset; }
    const juce::String& getPresetLabel() const { return presetLabel; }

    void switchAB (bool toB);
    bool isOnB() const { return onB; }

    pump::Params readParams() const;

    // Points of the DRAW shape, kept in the saved state
    void setPoints (const std::vector<pump::Point>& pts);

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    skazi::PresetFader fader; // fades around preset changes
    pump::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyValues (const std::map<juce::String, float>& values);
    void storePointsInState();
    void loadPointsFromState();

    int currentPreset = 0;
    juce::String presetLabel { juce::String::fromUTF8 ("FACTORY \xc2\xb7 Init") };
    juce::ValueTree slotA, slotB;
    bool onB = false;
    std::vector<int> noteOns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitaPumpProcessor)
};
