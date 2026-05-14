#pragma once

#include <atomic>
#include <cstdint>

namespace opal
{

// Real-time-safe audio-analysis state shared between the audio thread (writer)
// and the GUI / render threads (readers). Independent atomics rather than a
// single struct snapshot, so every store is guaranteed lock-free on arm64 /
// x86_64.
//
// Per band we publish THREE different envelopes:
//   - Level  : medium-speed envelope (~10 ms) — current "this band right now"
//   - Peak   : fast attack, slow release (~1 ms / 150 ms) — captures transients
//   - Avg    : slow symmetric average (~800 ms) — long-term loudness reference
//
// The butterchurn-style separation lets the visualizer react to peaks
// (transient flashes) without the slow loudness baseline drowning them out,
// and gives a quiet reference signal for normalisation / mood. Single
// sustained envelope alone made the shader read as "one shape flashes on
// every kick" — three signals per band gives the shader much more to chew on.
//
// `onsetCounter` is monotonic; consumers compute the frame-to-frame delta to
// detect new onsets.
struct AnalysisBus
{
    std::atomic<float>    bassLevel    { 0.0f };
    std::atomic<float>    midLevel     { 0.0f };
    std::atomic<float>    highLevel    { 0.0f };

    std::atomic<float>    bassPeak     { 0.0f };
    std::atomic<float>    midPeak      { 0.0f };
    std::atomic<float>    highPeak     { 0.0f };

    std::atomic<float>    bassAvg      { 0.0f };
    std::atomic<float>    midAvg       { 0.0f };
    std::atomic<float>    highAvg      { 0.0f };

    std::atomic<float>    rms          { 0.0f };
    std::atomic<uint32_t> onsetCounter { 0 };
};

} // namespace opal
