#pragma once

#include "../../Common/SkaziCore.h"

namespace tahini
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng);
} // namespace tahini
