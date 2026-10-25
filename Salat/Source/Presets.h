#pragma once

#include "../../Common/SkaziCore.h"

namespace salat
{
const std::vector<skazi::Preset>& presets();
skazi::Preset randomPreset (juce::Random& rng); // MAKE A SALAT
} // namespace salat
