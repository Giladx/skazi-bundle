#pragma once

// Shared base for SKAZI plugins: parameter state, undo, A/B compare and presets.
#include "SkaziCore.h"
#include "PresetFader.h"

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
        currentPreset = index;
        presetLabel = list[(size_t) index].category + juce::String::fromUTF8 (" \xc2\xb7 ") + list[(size_t) index].name;
        const auto values = list[(size_t) index].values;
        fader.change ([this, values] { applyValues (values); });
    }

    void randomizeAll()
    {
        auto& r = juce::Random::getSystemRandom();
        const auto values = randomPreset (r).values;
        presetLabel = juce::String::fromUTF8 ("RANDOM \xc2\xb7 ") + juce::String (r.nextInt ({ 100, 1000 }));
        fader.change ([this, values] { applyValues (values); });
    }

    int getCurrentPreset() const { return currentPreset; }
    const juce::String& getPresetLabel() const { return presetLabel; }

    void switchAB (bool toB)
    {
        if (toB == onB) return;
        (onB ? slotB : slotA) = apvts.copyState();
        onB = toB;
        fader.change ([this, toB]
        {
            const auto& target = toB ? slotB : slotA;
            if (target.isValid())
                apvts.replaceState (target.createCopy());
        });
    }
    bool isOnB() const { return onB; }

    // Call as the last line of processBlock: fades around preset changes so they never click or jump
    void finishBlock (juce::AudioBuffer<float>& buffer) { fader.process (buffer, getSampleRate()); }

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
    // Every parameter gets a value: the preset's, or its default when the preset leaves it out,
    // so nothing from the previous sound leaks into the new one.
    void applyValues (const std::map<juce::String, float>& values)
    {
        undo.beginNewTransaction();
        for (auto* p : getParameters())
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto it = values.find (prm->getParameterID());
                const float norm = it != values.end() ? prm->convertTo0to1 (it->second) : prm->getDefaultValue();
                if (std::abs (prm->getValue() - norm) > 1.0e-7f)
                    prm->setValueNotifyingHost (norm);
            }
    }

private:
    PresetFader fader;
    int currentPreset = 0;
    juce::String presetLabel { juce::String::fromUTF8 ("FACTORY \xc2\xb7 Init") };
    juce::ValueTree slotA, slotB;
    bool onB = false;
};
} // namespace skazi
