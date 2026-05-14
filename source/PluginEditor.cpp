#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      visualizer (p),
      overlay (p)
{
    addAndMakeVisible (visualizer);   // GL behind
    addAndMakeVisible (overlay);      // CPU paint in front

    setSize (640, 360);
    setResizable (true, true);
    setResizeLimits (480, 270, 7680, 4320);     // 270p to 8K

    // Required so F / Esc reach keyPressed in Standalone (plugin hosts route
    // keyboard separately, so this is effectively Standalone-only).
    setWantsKeyboardFocus (true);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    // The OpenGL visualizer fills the entire client area; the editor itself
    // has nothing to paint.
    juce::ignoreUnused (g);
}

void PluginEditor::resized()
{
    const auto bounds = getLocalBounds();
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

void PluginEditor::toggleFullscreen()
{
    // setFullScreen lives on ResizableWindow, not TopLevelWindow. In Standalone
    // the active top-level is a DocumentWindow (subclass of ResizableWindow).
    // In plugin hosts there's no ResizableWindow to find — F becomes a no-op,
    // which is the right behaviour: the DAW owns the editor frame.
    if (auto* tlw = juce::TopLevelWindow::getActiveTopLevelWindow())
    {
        if (auto* rw = dynamic_cast<juce::ResizableWindow*> (tlw))
        {
            const auto goingFullscreen = ! rw->isFullScreen();
            rw->setFullScreen (goingFullscreen);
            overlay.setVisible (! goingFullscreen);
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
                overlay.setVisible (true);
            }
        }
    }
}
