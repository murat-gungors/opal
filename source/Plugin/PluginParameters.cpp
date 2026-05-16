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
        knob (ParamID::hue,   "Hue"),
        knob (ParamID::grain, "Grain"),
        knob (ParamID::warp,  "Warp"),
        knob (ParamID::trail, "Trail"),
        knob (ParamID::pop,   "Pop"),
        knob (ParamID::temp,  "Temp")
    };
}

} // namespace opal
