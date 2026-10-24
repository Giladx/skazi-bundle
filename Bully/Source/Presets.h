#pragma once

#include "../../Common/SkaziCore.h"

namespace bully
{
const std::vector<skazi::Preset>& presets();     // factory presets in style folders
skazi::Preset randomPreset (juce::Random& rng);  // FULL RANDOM: every knob, warp, filter, matrix slot and FX
} // namespace bully
