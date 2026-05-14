#include "Analyzer.h"

#include <algorithm>
#include <cmath>

namespace opal
{

namespace
{
    // Empirical scale that maps "typical mixed-music per-band mean magnitude"
    // to a 0..1 visual range with Hann-windowed 512-point FFT at -12 to -6 dBFS.
    // Tune by ear in Stage 4; clipped to [0, 1] downstream.
    constexpr float kBandMagnitudeScale = 0.05f;

    // Onset gate: flux must exceed median × this multiplier AND beat a small
    // absolute floor so quiet noise floors don't pulse.
    constexpr float kOnsetMedianMultiplier = 1.8f;
    constexpr float kOnsetMinFlux          = 0.5f;

    // Time constants (seconds) for the envelope followers.
    constexpr float kBandsTau = 0.010f; // 10 ms
    constexpr float kRmsTau   = 0.030f; // 30 ms
}

Analyzer::Analyzer()
    : fft (fftOrder)
{
}

void Analyzer::prepareToPlay (double newSampleRate)
{
    sampleRate = newSampleRate;

    // Hann window — symmetric definition.
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f
            * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                    * (float) i / (float) (fftSize - 1)));

    const auto freqToBin = [this] (float hz)
    {
        const auto bin = (int) std::round (hz * (float) fftSize / (float) sampleRate);
        return juce::jlimit (0, numBins - 1, bin);
    };

    bassBinLo = freqToBin (20.0f);
    bassBinHi = freqToBin (250.0f);
    midBinLo  = juce::jmin (numBins - 1, bassBinHi + 1);
    midBinHi  = freqToBin (4000.0f);
    highBinLo = juce::jmin (numBins - 1, midBinHi + 1);
    highBinHi = freqToBin (20000.0f);

    // Per-FFT-frame alpha for band envelopes.
    const auto framePeriod = (float) hopSize / (float) sampleRate;
    alphaBands = 1.0f - std::exp (-framePeriod / kBandsTau);

    reset();
}

void Analyzer::reset()
{
    sampleRing        .fill (0.0f);
    fftWorkspace      .fill (0.0f);
    previousMagnitudes.fill (0.0f);
    fluxRing          .fill (0.0f);
    recentFlux        .fill (0.0f);

    sampleRingWriteIndex = 0;
    samplesSinceLastFft  = 0;
    fluxRingIndex        = 0;
    fluxRingFilled       = 0;

    envBass = envMid = envHigh = envRms = 0.0f;
    onsetCounterLocal = 0;
}

void Analyzer::processSamples (const float* mono, int numSamples, AnalysisBus& bus)
{
    if (numSamples <= 0)
        return;

    // 1) Block-level RMS, smoothed.
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSq += (double) mono[i] * (double) mono[i];

    const auto blockRms = std::sqrt ((float) (sumSq / (double) numSamples));
    const auto blockDuration = (float) numSamples / (float) sampleRate;
    const auto alphaRms = 1.0f - std::exp (-blockDuration / kRmsTau);
    envRms = alphaRms * blockRms + (1.0f - alphaRms) * envRms;
    bus.rms.store (envRms, std::memory_order_relaxed);

    // 2) Push samples through the ring.
    for (int i = 0; i < numSamples; ++i)
    {
        sampleRing[(size_t) sampleRingWriteIndex] = mono[i];
        sampleRingWriteIndex = (sampleRingWriteIndex + 1) % ringSize;
    }
    samplesSinceLastFft += numSamples;

    // 3) Run an FFT for every hop's worth of new samples (usually 0 or 1).
    while (samplesSinceLastFft >= hopSize)
    {
        runFftFrame (bus);
        samplesSinceLastFft -= hopSize;
    }
}

void Analyzer::runFftFrame (AnalysisBus& bus)
{
    // Pull the last `fftSize` samples in chronological order from the ring.
    const auto readStart = (sampleRingWriteIndex - fftSize + ringSize) % ringSize;
    for (int i = 0; i < fftSize; ++i)
        fftWorkspace[(size_t) i] = sampleRing[(size_t) ((readStart + i) % ringSize)];

    // Hann window in place.
    juce::FloatVectorOperations::multiply (fftWorkspace.data(), window.data(), fftSize);

    // Zero the upper half — required size for performFrequencyOnlyForwardTransform.
    juce::FloatVectorOperations::clear (fftWorkspace.data() + fftSize, fftSize);

    fft.performFrequencyOnlyForwardTransform (fftWorkspace.data(), /*ignoreNegativeFreqs*/ true);

    // 3-band mean magnitude — keeps bands comparable despite differing bin counts.
    const auto bandMean = [this] (int lo, int hi)
    {
        float sum = 0.0f;
        for (int k = lo; k <= hi; ++k)
            sum += fftWorkspace[(size_t) k];
        const auto count = (hi - lo + 1);
        return count > 0 ? sum / (float) count : 0.0f;
    };

    const auto bass = juce::jlimit (0.0f, 1.0f, bandMean (bassBinLo, bassBinHi) * kBandMagnitudeScale);
    const auto mid  = juce::jlimit (0.0f, 1.0f, bandMean (midBinLo,  midBinHi)  * kBandMagnitudeScale);
    const auto high = juce::jlimit (0.0f, 1.0f, bandMean (highBinLo, highBinHi) * kBandMagnitudeScale);

    envBass = alphaBands * bass + (1.0f - alphaBands) * envBass;
    envMid  = alphaBands * mid  + (1.0f - alphaBands) * envMid;
    envHigh = alphaBands * high + (1.0f - alphaBands) * envHigh;

    bus.bassLevel.store (envBass, std::memory_order_relaxed);
    bus.midLevel .store (envMid,  std::memory_order_relaxed);
    bus.highLevel.store (envHigh, std::memory_order_relaxed);

    // Spectral flux: sum of positive magnitude deltas over previous frame.
    float flux = 0.0f;
    for (int k = 0; k < numBins; ++k)
    {
        const auto cur = fftWorkspace[(size_t) k];
        const auto diff = cur - previousMagnitudes[(size_t) k];
        if (diff > 0.0f)
            flux += diff;
        previousMagnitudes[(size_t) k] = cur;
    }

    // Slide the last-three buffer and append.
    recentFlux[2] = recentFlux[1];
    recentFlux[1] = recentFlux[0];
    recentFlux[0] = flux;

    fluxRing[(size_t) fluxRingIndex] = flux;
    fluxRingIndex = (fluxRingIndex + 1) % fluxHistory;
    if (fluxRingFilled < fluxHistory)
        ++fluxRingFilled;

    // Median of the filled portion of the flux ring — stack copy + nth_element.
    std::array<float, fluxHistory> sorted {};
    std::copy_n (fluxRing.begin(), fluxRingFilled, sorted.begin());
    const auto mid_iter = sorted.begin() + fluxRingFilled / 2;
    std::nth_element (sorted.begin(), mid_iter, sorted.begin() + fluxRingFilled);
    const auto fluxMedian = *mid_iter;

    // Onset: the *middle* of the three recent flux values must be a strict
    // local maximum AND beat the gated threshold. Looking at the middle gives
    // us symmetric neighbours and only one frame of detection latency.
    const auto candidate = recentFlux[1];
    const bool localMax  = candidate > recentFlux[0] && candidate > recentFlux[2];
    const bool aboveGate = candidate > fluxMedian * kOnsetMedianMultiplier
                        && candidate > kOnsetMinFlux;

    if (localMax && aboveGate)
    {
        ++onsetCounterLocal;
        bus.onsetCounter.store (onsetCounterLocal, std::memory_order_relaxed);
    }
}

} // namespace opal
