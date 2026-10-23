#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace presets
{
// Values are in parameter units (percent, choice index, dB...) keyed by parameter ID.
// Every preset lists every parameter, so loading one fully defines the sound.
struct Preset
{
    juce::String name, category;
    std::map<juce::String, float> values;
};

const std::vector<Preset>& all();      // FACTORY presets first, then 500 generated ones
juce::StringArray categories();        // in menu order
Preset makeRandom (juce::Random& rng); // what the RANDOM button loads; always on the grid (SYNC)
} // namespace presets
