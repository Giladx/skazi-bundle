#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "KickEngine.h"
#include "PresetBank.h"

class KickassProcessor : public juce::AudioProcessor
{
public:
    KickassProcessor();
    ~KickassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // The factory bank doubles as the host's program list.
    int getNumPrograms() override { return kick::numFactoryPresets(); }
    int getCurrentProgram() override { return juce::jmax (0, (int) apvts.state.getProperty ("presetIndex", 0)); }
    void setCurrentProgram (int index) override { loadFactoryPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Current knob settings, safe to call from any thread. */
    kick::Params currentParams() const;

    /** Audition button: fires a hit on the next audio block. */
    void triggerFromUi() { uiTrigger.store (true); }

    /** Counts hits, so the editor can flash its hit LED. */
    int getHitCount() const { return hitCount.load(); }

    // Click layer sample (message thread). The file path is saved with the state.
    bool loadClickSample (const juce::File&);
    void clearClickSample();
    kick::ClickSamplePtr getClickSample() const { return std::atomic_load (&clickSample); }
    juce::String getClickSampleName() const;
    /** One-click setups for a loaded kick: the sample alone, or its top over the synth sub. */
    void useSampleAsFullKick();
    void useSampleAsTopLayer();
    juce::File getClickSampleFile() const { return juce::File (apvts.state.getProperty ("clickFile").toString()); }

    // Presets (message thread). The browser runs through the factory bank, then the user folder.
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    void stepPreset (int delta);
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresets();
    juce::String getPresetDisplayName() const;
    int getPresetIndex() const { return apvts.state.getProperty ("presetIndex", -1); }
    juce::File getUserPresetFile() const { return juce::File (apvts.state.getProperty ("presetFile").toString()); }

    /** True once any knob or the click sample differs from the loaded preset. */
    bool isPresetModified() const;

    /** Output peak since the editor last asked (the meter resets it). */
    float takePeak() { return outputPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState; // on-screen keyboard

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void setClickSample (kick::ClickSamplePtr);
    void setParam (const juce::String& id, float plainValue);
    void applyParams (const kick::Params&);
    void setPresetLabel (int index, const juce::String& name, const juce::File& file = {});
    void markPresetClean();

    kick::Params loadedParams;          // what the current preset set, for the "modified" mark
    kick::ClickSamplePtr loadedSample;

    kick::Engine engine;
    kick::ClickSamplePtr clickSample;          // swapped atomically; the audio thread copies it per block
    std::vector<kick::ClickSamplePtr> retired; // keeps replaced samples alive so the audio thread never frees one
    double sampleRate = 44100.0;
    std::atomic<bool> uiTrigger { false };
    std::atomic<int> hitCount { 0 };
    std::atomic<float> outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickassProcessor)
};
