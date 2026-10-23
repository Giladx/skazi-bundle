#pragma once

#include "../../Common/SkaziCore.h"

namespace zhug
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng);
} // namespace zhug
