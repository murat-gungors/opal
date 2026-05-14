#include "PluginEditor.h"

#include <cmath>

namespace
{
    // Stage 0 placeholder background — neutral dark used until the OpenGL
    // visualizer ships in Stage 3.
    constexpr auto kBackgroundColour = 0xff1a1f24;
    constexpr int  kOverlayTimerHz   = 30;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (640, 360);
    startTimerHz (kOverlayTimerHz);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::timerCallback()
{
    repaint();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBackgroundColour));

    const auto& bus = processorRef.getTransportBus();
    const auto bpm       = bus.bpm        .load (std::memory_order_relaxed);
    const auto ppq       = bus.ppqPosition.load (std::memory_order_relaxed);
    const auto playing   = bus.isPlaying  .load (std::memory_order_relaxed);
    const auto beat      = static_cast<int> (std::floor (ppq));
    const auto beatPhase = static_cast<float> (ppq - std::floor (ppq));

    const auto bounds = getLocalBounds().reduced (16);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    auto drawLine = [&] (int row, const juce::String& label, const juce::String& value)
    {
        constexpr int rowHeight = 18;
        const auto y = bounds.getY() + 16 + row * rowHeight;
        const auto text = label.paddedRight (' ', 6) + value;
        g.drawSingleLineText (text, bounds.getX(), y);
    };

    drawLine (0, "BPM",  juce::String (bpm, 2));
    drawLine (1, "PPQ",  juce::String (ppq, 3));
    drawLine (2, "BEAT", juce::String (beat));
    drawLine (3, "PLAY", playing ? "YES" : "NO");

    // Beat-phase bar — fills 0→1 across the width across each quarter note.
    auto barTrack = juce::Rectangle<int> (bounds.getX(),
                                          bounds.getBottom() - 4,
                                          bounds.getWidth(),
                                          3);
    g.setColour (juce::Colour (0xff333a44));
    g.fillRect (barTrack);
    g.setColour (juce::Colour (0xffd6e0f0));
    g.fillRect (barTrack.withWidth (static_cast<int> (barTrack.getWidth() * beatPhase)));
}

void PluginEditor::resized()
{
}
