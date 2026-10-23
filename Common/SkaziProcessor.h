#pragma once

// Shared base for SKAZI plugins: parameter state, undo, A/B compare and presets.
#include "SkaziCore.h"

namespace skazi
{
class SkaziProcessor : public juce::AudioProcessor
{
public:
    SkaziProcessor (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, const juce::Identifier& stateType)
        : AudioProcessor (buses), apvts (*this, &undo, stateType, std::move (layout)) {}

    virtual const std::vector<Preset>& presetList() const = 0;
    virtual Preset randomPreset (juce::Random& rng) const = 0;

    float v (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    void loadPreset (int index)
    {
        const auto& list = presetList();
        if (list.empty()) return;
        index = ((index % (int) list.size()) + (int) list.size()) % (int) list.size();
        applyValues (list[(size_t) index].values);
        currentPreset = index;
        presetLabel = list[(size_t) index].category + juce::String::fromUTF8 (" \xc2\xb7 ") + list[(size_t) index].name;
    }

    void randomizeAll()
    {
        auto& r = juce::Random::getSystemRandom();
        applyValues (randomPreset (r).values);
        presetLabel = juce::String::fromUTF8 ("RANDOM \xc2\xb7 ") + juce::String (r.nextInt ({ 100, 1000 }));
    }

    int getCurrentPreset() const { return currentPreset; }
    const juce::String& getPresetLabel() const { return presetLabel; }

    void switchAB (bool toB)
    {
        if (toB == onB) return;
        (onB ? slotB : slotA) = apvts.copyState();
        const auto& target = toB ? slotB : slotA;
        if (target.isValid())
            apvts.replaceState (target.createCopy());
        onB = toB;
    }
    bool isOnB() const { return onB; }

    // AudioProcessor boilerplate shared by every SKAZI plugin
    const juce::String getName() const override { return JucePlugin_Name; }
    bool hasEditor() const override { return true; }
    bool isMidiEffect() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    juce::UndoManager undo; // declared before apvts, which points at it
    juce::AudioProcessorValueTreeState apvts;

protected:
    void applyValues (const std::map<juce::String, float>& values)
    {
        undo.beginNewTransaction();
        for (auto& [id, value] : values)
            if (auto* prm = apvts.getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

private:
    int currentPreset = 0;
    juce::String presetLabel { juce::String::fromUTF8 ("FACTORY \xc2\xb7 Init") };
    juce::ValueTree slotA, slotB;
    bool onB = false;
};
} // namespace skazi
