#pragma once

#include "PluginProcessor.h"
#include "GUI/DebugOverlayComponent.h"
#include "GUI/VisualizerComponent.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processorRef;

    VisualizerComponent   visualizer;
    DebugOverlayComponent overlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
