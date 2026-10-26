#pragma once

#include "../../Common/SkaziCore.h"

namespace kube
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng); // RANDOM: a sound from a random category
} // namespace kube
