#pragma once

#include "Pattern.h"

namespace midigod
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng);
} // namespace midigod
