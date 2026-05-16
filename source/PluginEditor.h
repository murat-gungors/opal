#pragma once

#include "PluginProcessor.h"
#include "GUI/DebugOverlayComponent.h"
#include "GUI/KnobStrip.h"
#include "GUI/VisualizerComponent.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void toggleFullscreen();
    void exitFullscreen();
    void applyFullscreenChromeVisibility (bool fullscreen);

    PluginProcessor& processorRef;

    VisualizerComponent   visualizer;
    DebugOverlayComponent overlay;
    KnobStrip             knobStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
