#include "PluginEditor.h"

namespace
{
    // Height reserved for the knob strip at the bottom of the editor.
    constexpr int kKnobStripHeight = 90;
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      visualizer (p),
      overlay (p),
      knobStrip (p.getParameters())
{
    addAndMakeVisible (visualizer);   // GL behind
    addAndMakeVisible (overlay);      // CPU paint in front of GL
    addAndMakeVisible (knobStrip);    // Bottom controls

    setSize (640, 360);
    setResizable (true, true);
    setResizeLimits (480, 270, 7680, 4320);

    setWantsKeyboardFocus (true);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    // Knob strip at bottom (only visible outside fullscreen — see
    // applyFullscreenChromeVisibility), but always reserve its space when
    // visible so the visualizer doesn't reflow.
    if (knobStrip.isVisible())
    {
        const auto strip = bounds.removeFromBottom (kKnobStripHeight);
        knobStrip.setBounds (strip);
    }

    visualizer.setBounds (bounds);
    overlay   .setBounds (bounds);
}

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();

    if (code == 'F' || code == 'f' || code == juce::KeyPress::F11Key)
    {
        toggleFullscreen();
        return true;
    }

    if (code == juce::KeyPress::escapeKey)
    {
        exitFullscreen();
        return true;
    }

    return false;
}

void PluginEditor::applyFullscreenChromeVisibility (bool fullscreen)
{
    // In fullscreen we want a clean canvas: hide the debug overlay AND the
    // knob strip. Leaving fullscreen restores both.
    overlay  .setVisible (! fullscreen);
    knobStrip.setVisible (! fullscreen);
    resized();
}

void PluginEditor::toggleFullscreen()
{
    if (auto* tlw = juce::TopLevelWindow::getActiveTopLevelWindow())
    {
        if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tlw))
        {
            const auto goingFullscreen = ! rw->isFullScreen();
            rw->setFullScreen (goingFullscreen);
            applyFullscreenChromeVisibility (goingFullscreen);
        }
    }
}

void PluginEditor::exitFullscreen()
{
    if (auto* tlw = juce::TopLevelWindow::getActiveTopLevelWindow())
    {
        if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tlw))
        {
            if (rw->isFullScreen())
            {
                rw->setFullScreen (false);
                applyFullscreenChromeVisibility (false);
            }
        }
    }
}
