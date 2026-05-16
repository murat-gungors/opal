#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace opal
{

// Parameter IDs are stable for the lifetime of the plugin — host automation
// references these strings, so renames here break user projects.
namespace ParamID
{
    inline constexpr auto drive = "drive";
    inline constexpr auto bass  = "bass";
    inline constexpr auto hue   = "hue";
    inline constexpr auto grain = "grain";
    inline constexpr auto warp  = "warp";
    inline constexpr auto trail = "trail";
    inline constexpr auto pop   = "pop";
    inline constexpr auto temp  = "temp";
}

// All eight knobs are normalised 0..1 with default 0.5. The shader and the
// AVTS attachments treat them as continuous multipliers — 0 disables an
// effect, 0.5 is the calibrated "musical" default, 1 is maximum response.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace opal
