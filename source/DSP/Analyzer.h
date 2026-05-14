#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstdint>

#include "../Util/AnalysisBus.h"

namespace opal
{

// Real-time audio analyzer. Owns a 512-sample FFT, ring buffer, Hann window,
// 3-band logarithmic envelope follower, RMS follower, and a spectral-flux
// onset detector with a 43-frame (~230 ms @ 48 kHz / hop 256) median gate.
//
// All buffers are fixed-size and pre-allocated. `prepareToPlay` is the only
// state-changing entry point that touches sample-rate-dependent values.
// `processSamples` is the only entry called from the audio thread and is
// RT-safe: no allocation, no locks, no juce::String, no logging.
//
// Inputs are mono — callers (PluginProcessor) downmix stereo to mono before
// invoking. Outputs are published via AnalysisBus atomic stores.
class Analyzer
{
public:
    static constexpr int  fftOrder    = 9;
    static constexpr int  fftSize     = 1 << fftOrder; // 512
    static constexpr int  hopSize     = fftSize / 2;   // 256, 50% overlap
    static constexpr int  numBins     = fftSize / 2;   // 256 unique magnitude bins
    static constexpr int  ringSize    = fftSize * 2;   // 1024 sample circular buffer
    static constexpr int  fluxHistory = 43;            // ~230 ms gate for onset median

    Analyzer();

    void prepareToPlay (double newSampleRate);
    void reset();

    void processSamples (const float* monoSamples, int numSamples, AnalysisBus& bus);

private:
    void runFftFrame (AnalysisBus& bus);

    juce::dsp::FFT fft;

    double sampleRate { 48000.0 };

    std::array<float, fftSize>       window           {};
    std::array<float, ringSize>      sampleRing       {};
    std::array<float, fftSize * 2>   fftWorkspace     {};
    std::array<float, numBins>       previousMagnitudes {};
    std::array<float, fluxHistory>   fluxRing         {};
    std::array<float, 3>             recentFlux       {};

    int sampleRingWriteIndex { 0 };
    int samplesSinceLastFft  { 0 };
    int fluxRingIndex        { 0 };
    int fluxRingFilled       { 0 };

    int bassBinLo { 0 }, bassBinHi { 0 };
    int midBinLo  { 0 }, midBinHi  { 0 };
    int highBinLo { 0 }, highBinHi { 0 };

    float envBass { 0.0f }, envMid { 0.0f }, envHigh { 0.0f }, envRms { 0.0f };

    // Peak followers (fast attack, slow release) and long-term averages,
    // one set per band — butterchurn-style three-band three-smoothing.
    float envBassPeak { 0.0f }, envMidPeak { 0.0f }, envHighPeak { 0.0f };
    float envBassAvg  { 0.0f }, envMidAvg  { 0.0f }, envHighAvg  { 0.0f };

    float alphaBands       { 0.0f };
    float alphaPeakAttack  { 0.0f };
    float alphaPeakRelease { 0.0f };
    float alphaAvg         { 0.0f };

    uint32_t onsetCounterLocal { 0 };
};

} // namespace opal
