#include <catch2/catch_test_macros.hpp>

#include <DSP/Analyzer.h>
#include <Util/AnalysisBus.h>

#include <cmath>
#include <vector>

using opal::Analyzer;
using opal::AnalysisBus;

namespace
{
    constexpr double kTestSampleRate = 48000.0;
    constexpr int    kBlockSize      = 256;

    template <typename Generator>
    void runFor (Analyzer& a, AnalysisBus& bus, Generator gen, double seconds)
    {
        const auto totalSamples = static_cast<int> (seconds * kTestSampleRate);
        std::vector<float> block ((size_t) kBlockSize, 0.0f);
        int idx = 0;
        while (idx < totalSamples)
        {
            const auto n = std::min (kBlockSize, totalSamples - idx);
            for (int i = 0; i < n; ++i)
                block[(size_t) i] = gen (idx + i);
            a.processSamples (block.data(), n, bus);
            idx += n;
        }
    }

    constexpr float kTwoPi = 6.2831853071795864769f;

    float sineAt (float frequencyHz, float amplitude, int sampleIndex)
    {
        return amplitude * std::sin (kTwoPi * frequencyHz
                                     * (float) sampleIndex / (float) kTestSampleRate);
    }
}

TEST_CASE ("Analyzer: silence keeps all metrics at zero", "[analyzer][silence]")
{
    Analyzer a;
    AnalysisBus bus;
    a.prepareToPlay (kTestSampleRate);

    runFor (a, bus, [] (int) { return 0.0f; }, 1.0);

    CHECK (bus.rms        .load() < 0.001f);
    CHECK (bus.bassLevel  .load() < 0.001f);
    CHECK (bus.midLevel   .load() < 0.001f);
    CHECK (bus.highLevel  .load() < 0.001f);
    CHECK (bus.onsetCounter.load() == 0u);
}

TEST_CASE ("Analyzer: 100 Hz sine concentrates in bass band", "[analyzer][bands]")
{
    Analyzer a;
    AnalysisBus bus;
    a.prepareToPlay (kTestSampleRate);

    runFor (a, bus, [] (int i) { return sineAt (100.0f, 0.5f, i); }, 0.5);

    const auto bass = bus.bassLevel.load();
    const auto mid  = bus.midLevel .load();
    const auto high = bus.highLevel.load();

    CHECK (bass > 0.01f);
    CHECK (bass > mid  * 5.0f);
    CHECK (bass > high * 5.0f);
}

TEST_CASE ("Analyzer: 8 kHz sine concentrates in high band", "[analyzer][bands]")
{
    Analyzer a;
    AnalysisBus bus;
    a.prepareToPlay (kTestSampleRate);

    runFor (a, bus, [] (int i) { return sineAt (8000.0f, 0.5f, i); }, 0.5);

    const auto bass = bus.bassLevel.load();
    const auto mid  = bus.midLevel .load();
    const auto high = bus.highLevel.load();

    CHECK (high > 0.01f);
    CHECK (high > bass * 5.0f);
    CHECK (high > mid);
}

TEST_CASE ("Analyzer: silence -> tone triggers an onset", "[analyzer][onset]")
{
    Analyzer a;
    AnalysisBus bus;
    a.prepareToPlay (kTestSampleRate);

    // First let the flux median settle on silence, then abruptly hand it a
    // mid-band tone — the burst should clear the median × 1.8 gate at least once.
    runFor (a, bus, [] (int) { return 0.0f; }, 0.3);
    const auto onsetsBefore = bus.onsetCounter.load();

    runFor (a, bus, [] (int i) { return sineAt (1000.0f, 0.5f, i); }, 0.3);

    CHECK (bus.onsetCounter.load() > onsetsBefore);
}
