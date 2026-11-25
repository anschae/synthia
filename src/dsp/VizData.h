#pragma once
// Data for the editor's live displays. Written by the audio thread with relaxed
// atomic stores (no locks, no allocation), read by the UI timer. Tearing between
// elements is harmless for drawing. Writers check `active` (set while an editor
// is open) and skip all work otherwise.

#include <array>
#include <atomic>
#include <cstdint>

namespace synthia::dsp
{
struct VizData
{
    std::atomic<bool> active { false };

    // ---- input timeline: one column per kTimelineColSeconds, ring of kTimelineCols
    static constexpr int kTimelineCols = 256;
    static constexpr double kTimelineSeconds = 3.0;
    static constexpr int kTimelineVisible = 192;
    std::array<std::atomic<float>, kTimelineCols> tlPeak {}, tlEnv {}, tlGr {};
    std::array<std::atomic<uint8_t>, kTimelineCols> tlPick {};
    std::atomic<int> tlWrite { 0 }; // index of the next column to be written
    std::atomic<float> gateThreshold { 0.0f };

    // ---- voicer bands
    static constexpr int kMaxBands = 72;
    std::array<std::atomic<float>, kMaxBands> vcAmp {}, vcWeight {};
    std::atomic<int> vcBands { 0 };
    std::atomic<float> vcLo { 45.0f }, vcHi { 5000.0f };

    // ---- granular buffer + grains
    static constexpr int kGrCols = 512;
    std::array<std::atomic<float>, kGrCols> grCol {}; // peak per column (absolute buffer position)
    std::atomic<int> grWriteCol { 0 };
    std::atomic<float> grBufferSeconds { 8.0f };
    struct Grain
    {
        std::atomic<float> delaySec { 0 }; // read position, seconds behind the write head
        std::atomic<float> semis { 0 };
        std::atomic<float> age { 0 };     // 0..1
        std::atomic<int> flags { 0 };     // bit0 active, bit1 reverse
    };
    std::array<Grain, 64> grains {};
    std::atomic<int> grStutActive { 0 };
    std::atomic<float> grStutStartSec { 0 }, grStutLenSec { 0 }; // slice start behind the write head
    std::atomic<int> grFrozen { 0 };

    // ---- filter / amp shaper
    std::atomic<float> fltCutoff { 1000.0f };
    std::atomic<float> adsrLevel { 0.0f };
    std::atomic<int> adsrStage { 0 };
    std::atomic<int> chopStep { -1 };

    // ---- spectral: log-spaced display bins (dB)
    static constexpr int kSpecBins = 256;
    static constexpr float kSpecLo = 30.0f, kSpecHi = 16000.0f;
    std::array<std::atomic<float>, kSpecBins> spLive {}, spFrozen {};
    std::array<std::atomic<uint8_t>, kSpecBins> spPeak {};
    std::atomic<int> spFrozenOn { 0 };
    std::atomic<int> spFrameCounter { 0 };

    // ---- resonator voices
    std::array<std::atomic<float>, 8> rsVoice {};
    std::atomic<int> rsVoices { 0 };

    // ---- modulation
    std::atomic<float> lfo1Phase { 0 }, lfo2Phase { 0 };
};
} // namespace synthia::dsp
