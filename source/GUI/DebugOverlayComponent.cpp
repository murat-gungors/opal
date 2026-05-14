#include "DebugOverlayComponent.h"

#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int   kOverlayTimerHz           = 30;
    constexpr float kOnsetFlashDecayPerFrame  = 1.0f / (0.10f * (float) kOverlayTimerHz); // ~100 ms
}

DebugOverlayComponent::DebugOverlayComponent (PluginProcessor& p)
    : processorRef (p)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    startTimerHz (kOverlayTimerHz);
}

DebugOverlayComponent::~DebugOverlayComponent() = default;

void DebugOverlayComponent::timerCallback()
{
    repaint();
}

void DebugOverlayComponent::paint (juce::Graphics& g)
{
    const auto& transport = processorRef.getTransportBus();
    const auto& analysis  = processorRef.getAnalysisBus();

    const auto bpm     = transport.bpm        .load (std::memory_order_relaxed);
    const auto ppq     = transport.ppqPosition.load (std::memory_order_relaxed);
    const auto playing = transport.isPlaying  .load (std::memory_order_relaxed);

    const auto bass    = analysis.bassLevel   .load (std::memory_order_relaxed);
    const auto mid     = analysis.midLevel    .load (std::memory_order_relaxed);
    const auto high    = analysis.highLevel   .load (std::memory_order_relaxed);
    const auto rmsVal  = analysis.rms         .load (std::memory_order_relaxed);
    const auto onsets  = analysis.onsetCounter.load (std::memory_order_relaxed);

    if (onsets != lastOnsetCounter)
        onsetFlashAlpha = 1.0f;
    else
        onsetFlashAlpha = std::max (0.0f, onsetFlashAlpha - kOnsetFlashDecayPerFrame);
    lastOnsetCounter = onsets;

    const auto beat      = static_cast<int>   (std::floor (ppq));
    const auto beatPhase = static_cast<float> (ppq - std::floor (ppq));

    const auto bounds = getLocalBounds().reduced (16);

    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    constexpr int rowHeight = 18;
    constexpr int textBaselineOffset = 14;
    const     int transportTop = bounds.getY() + 4;

    auto drawTransportLine = [&] (int row, const juce::String& label, const juce::String& value)
    {
        const auto y = transportTop + textBaselineOffset + row * rowHeight;
        const auto text = label.paddedRight (' ', 6) + value;
        g.setColour (juce::Colours::white);
        g.drawSingleLineText (text, bounds.getX(), y);
    };

    drawTransportLine (0, "BPM",  juce::String (bpm, 2));
    drawTransportLine (1, "PPQ",  juce::String (ppq, 3));
    drawTransportLine (2, "BEAT", juce::String (beat));
    drawTransportLine (3, "PLAY", playing ? "YES" : "NO");

    const auto analysisTop = transportTop + 4 * rowHeight + 12;

    const auto barTrackColour = juce::Colour (0x80333a44); // semi-transparent so GL shows through faintly
    const auto barFillColour  = juce::Colour (0xffd6e0f0);

    auto drawAnalysisBar = [&] (int row, const juce::String& label, float value)
    {
        const auto y = analysisTop + row * rowHeight;
        const auto labelX = bounds.getX();
        constexpr int labelWidth = 56;
        constexpr int valueWidth = 56;

        g.setColour (juce::Colours::white);
        g.drawSingleLineText (label.paddedRight (' ', 5), labelX, y + textBaselineOffset);

        const auto barRect = juce::Rectangle<int> (labelX + labelWidth, y + 4,
                                                   bounds.getWidth() - labelWidth - valueWidth,
                                                   rowHeight - 8);
        g.setColour (barTrackColour);
        g.fillRect (barRect);
        g.setColour (barFillColour);
        g.fillRect (barRect.withWidth (static_cast<int> ((float) barRect.getWidth()
                                                        * juce::jlimit (0.0f, 1.0f, value))));

        g.setColour (juce::Colours::white);
        g.drawSingleLineText (juce::String (juce::jlimit (0.0f, 9.99f, value), 2),
                              bounds.getRight() - valueWidth, y + textBaselineOffset);
    };

    drawAnalysisBar (0, "BASS", bass);
    drawAnalysisBar (1, "MID",  mid);
    drawAnalysisBar (2, "HIGH", high);
    drawAnalysisBar (3, "RMS",  rmsVal);

    if (onsetFlashAlpha > 0.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (onsetFlashAlpha));
        g.fillEllipse ((float) (bounds.getRight() - 12), (float) (bounds.getY() + 4),
                       8.0f, 8.0f);
    }

    auto barTrack = juce::Rectangle<int> (bounds.getX(),
                                          bounds.getBottom() - 4,
                                          bounds.getWidth(),
                                          3);
    g.setColour (barTrackColour);
    g.fillRect (barTrack);
    g.setColour (barFillColour);
    g.fillRect (barTrack.withWidth (static_cast<int> ((float) barTrack.getWidth() * beatPhase)));
}
