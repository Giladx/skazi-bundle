#pragma once

#include "../../Common/SkaziCore.h"

namespace quazi
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng);
} // namespace quazi
