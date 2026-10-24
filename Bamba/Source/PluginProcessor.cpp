#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

BambaProcessor::BambaProcessor()
    : SkaziProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                      skazi::buildLayout (bamba::paramDefs()), "BAMBA")
{
}

bool BambaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BambaProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void BambaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.render (buffer, midi, settings(), getPlayHead());
    finishBlock (buffer); // smooth preset changes
    midi.clear();
}

juce::AudioProcessorEditor* BambaProcessor::createEditor() { return new BambaEditor (*this); }

const std::vector<skazi::Preset>& BambaProcessor::presetList() const { return bamba::presets(); }
skazi::Preset BambaProcessor::randomPreset (juce::Random& rng) const { return bamba::randomPreset (rng); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BambaProcessor(); }
