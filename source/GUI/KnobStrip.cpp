#include "KnobStrip.h"

#include "../Plugin/PluginParameters.h"

namespace
{
    struct KnobDef
    {
        const char* paramId;
        const char* displayName;
    };

    constexpr std::array<KnobDef, 8> kKnobs {{
        { opal::ParamID::drive, "DRIVE" },
        { opal::ParamID::bass,  "BASS"  },
        { opal::ParamID::hue,   "FOLD"  },     // see PluginParameters.cpp
        { opal::ParamID::grain, "GRAIN" },
        { opal::ParamID::warp,  "WARP"  },
        { opal::ParamID::trail, "TRAIL" },
        { opal::ParamID::pop,   "POP"   },
        { opal::ParamID::temp,  "TEMP"  }
    }};

    constexpr auto kStripBgColour    = 0xff14181c;
    constexpr auto kLabelColour      = 0xffd6e0f0;
    constexpr auto kSliderFillColour = 0xffd6e0f0;
    constexpr auto kSliderTrackColour = 0xff404a55;
    constexpr auto kSliderThumbColour = 0xffeaf1ff;
}

KnobStrip::KnobStrip (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        auto& k = knobs[(size_t) i];

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, /*readOnly*/ true, 56, 12);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (kSliderFillColour));
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (kSliderTrackColour));
        k.slider.setColour (juce::Slider::thumbColourId,               juce::Colour (kSliderThumbColour));
        k.slider.setColour (juce::Slider::textBoxTextColourId,         juce::Colour (kLabelColour));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        k.slider.setDoubleClickReturnValue (true, 0.5);                // double-click → default
        addAndMakeVisible (k.slider);

        k.label.setText (kKnobs[(size_t) i].displayName, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold));
        k.label.setColour (juce::Label::textColourId, juce::Colour (kLabelColour));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, kKnobs[(size_t) i].paramId, k.slider);
    }
}

KnobStrip::~KnobStrip() = default;

void KnobStrip::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kStripBgColour));
}

void KnobStrip::resized()
{
    auto bounds = getLocalBounds().reduced (8, 4);
    const auto count   = (int) knobs.size();
    const auto cellW   = bounds.getWidth() / count;

    for (int i = 0; i < count; ++i)
    {
        auto cell      = bounds.removeFromLeft (cellW);
        auto labelArea = cell.removeFromTop (14);
        knobs[(size_t) i].label .setBounds (labelArea);
        knobs[(size_t) i].slider.setBounds (cell.reduced (2));
    }
}
