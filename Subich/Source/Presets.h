#pragma once

#include "../../Common/SkaziCore.h"

namespace subich
{
const std::vector<skazi::Preset>& presets();     // 1000 bass presets in style folders
skazi::Preset randomPreset (juce::Random& rng);  // RANDOM button
} // namespace subich
