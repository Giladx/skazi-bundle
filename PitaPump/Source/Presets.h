#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace presets
{
// Values in parameter units keyed by parameter ID; every preset lists every parameter
struct Preset
{
    juce::String name, category;
    std::map<juce::String, float> values;
};

const std::vector<Preset>& all();
juce::StringArray categories();
Preset makeRandom (juce::Random& rng); // RANDOM button: a new on-grid pump in SYNC mode
} // namespace presets
