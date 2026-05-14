#pragma once

#include <atomic>

namespace opal
{

// Real-time-safe transport state shared between the audio thread (writer) and
// the GUI thread (reader). Each field is an independent atomic so every store
// is guaranteed lock-free on the platforms we target (arm64, x86_64). A frame
// of per-field skew between reads is visually imperceptible and acceptable.
struct HostTransportBus
{
    std::atomic<float>  bpm         { 0.0f };
    std::atomic<double> ppqPosition { 0.0  };
    std::atomic<bool>   isPlaying   { false };
};

} // namespace opal
