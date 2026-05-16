#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

// Horizontal row of 8 rotary knobs bound to the plugin's AudioProcessor-
// ValueTreeState. Each slider is attached so host automation and state
// recall are wired automatically. Stage 6 will refine the visual treatment
// with a custom LookAndFeel; for now the stock JUCE rotary works.
class KnobStrip : public juce::Component
{
public:
    explicit KnobStrip (juce::AudioProcessorValueTreeState&);
    ~KnobStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Slot
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::array<Slot, 8> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobStrip)
};
