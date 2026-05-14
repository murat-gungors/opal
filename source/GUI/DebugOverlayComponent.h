#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

class PluginProcessor;

// Transparent overlay component drawn on top of the OpenGL visualizer.
// Hosts the debug readout (BPM/PPQ/BEAT/PLAY transport text, four
// audio-band bars, beat-phase strip, onset flash). Owns its own 30 Hz
// repaint timer so the GL render thread stays decoupled from GUI paint.
class DebugOverlayComponent : public juce::Component,
                              private juce::Timer
{
public:
    explicit DebugOverlayComponent (PluginProcessor&);
    ~DebugOverlayComponent() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    PluginProcessor& processorRef;

    std::uint32_t lastOnsetCounter { 0 };
    float         onsetFlashAlpha  { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugOverlayComponent)
};
