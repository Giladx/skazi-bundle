#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Common/PresetFader.h"
#include <map>
#include "ChopEngine.h"

class ShaksukaProcessor : public juce::AudioProcessor
{
public:
    ShaksukaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
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
    void randomizeAll(); // RANDOM button: a whole new on-grid setup
    int getCurrentPreset() const { return currentPreset; }
    const juce::String& getPresetLabel() const { return presetLabel; }

    // A/B compare: switching saves the current settings into the slot being left
    void switchAB (bool toB);
    bool isOnB() const { return onB; }

    void randomizeSeed();

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    skazi::PresetFader fader; // fades around preset changes
    chop::Engine engine;

    static juce::String weightId (int fx) { return "w" + juce::String (fx); }
    static juce::String enableId (int fx) { return "on" + juce::String (fx); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void applyValues (const std::map<juce::String, float>& values);

    int currentPreset = 0;
    juce::String presetLabel { juce::String::fromUTF8 ("FACTORY \xc2\xb7 Init") };
    juce::ValueTree slotA, slotB;
    bool onB = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShaksukaProcessor)
};
