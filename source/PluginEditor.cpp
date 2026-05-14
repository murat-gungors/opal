#include "PluginEditor.h"

namespace
{
    // Stage 0 placeholder background — neutral dark used until the OpenGL
    // visualizer ships in Stage 3.
    constexpr auto kBackgroundColour = 0xff1a1f24;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (640, 360);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBackgroundColour));
}

void PluginEditor::resized()
{
}
