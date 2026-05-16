#include "PluginParameters.h"

namespace opal
{

namespace
{
    using Param = juce::AudioProcessorValueTreeState::Parameter;

    std::unique_ptr<juce::AudioParameterFloat> knob (const juce::String& id,
                                                     const juce::String& name)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },          // version-hint = 1
            name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f),
            0.5f);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    return juce::AudioProcessorValueTreeState::ParameterLayout {
        knob (ParamID::drive, "Drive"),
        knob (ParamID::bass,  "Bass"),
        // Spec §5 originally called this "Hue". After Stage 5.1 the same
        // knob drives the radial-fold count (kaleidoscope) instead — much
        // more impactful than a subtle tint in a monochrome shader. The
        // ParameterID stays "hue" for state-recall back-compat; only the
        // host-visible name changes.
        knob (ParamID::hue,   "Fold"),
        knob (ParamID::grain, "Grain"),
        knob (ParamID::warp,  "Warp"),
        knob (ParamID::trail, "Trail"),
        knob (ParamID::pop,   "Pop"),
        knob (ParamID::temp,  "Temp")
    };
}

} // namespace opal
