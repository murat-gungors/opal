#pragma once

#include <atomic>
#include <cstdint>

namespace opal
{

// Real-time-safe audio-analysis state shared between the audio thread (writer)
// and the GUI / future render threads (readers). Same decision as
// HostTransportBus: independent atomics rather than a single struct snapshot,
// so every store is guaranteed lock-free on arm64 / x86_64.
//
// `onsetCounter` is monotonic; consumers compute the frame-to-frame delta to
// detect new onsets.
struct AnalysisBus
{
    std::atomic<float>    bassLevel    { 0.0f };
    std::atomic<float>    midLevel     { 0.0f };
    std::atomic<float>    highLevel    { 0.0f };
    std::atomic<float>    rms          { 0.0f };
    std::atomic<uint32_t> onsetCounter { 0 };
};

} // namespace opal
