#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      visualizer (p),
      overlay (p)
{
    addAndMakeVisible (visualizer);  // GL fills the whole bounds (back)
    addAndMakeVisible (overlay);     // Transparent CPU paint on top (front)

    setSize (640, 360);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    // Background falls through to the OpenGL visualizer behind us.
    juce::ignoreUnused (g);
}

void PluginEditor::resized()
{
    const auto bounds = getLocalBounds();
    visualizer.setBounds (bounds);
    overlay   .setBounds (bounds);
}
