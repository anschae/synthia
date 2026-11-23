#pragma once
// The complete Synthia signal flow (testable without the plugin wrapper):
//
//  in -> InputStage (gain, HPF, gate) ----------------------------+-> B Spectral
//                                    \-> [align delay] -> analysis (env, pick)
//                                                   -> A Voicer, C Resonator, D Granular, E Dry
//  layers -> mixer (level/pan/mute/solo/"to filter")
//         -> filter bus: MainFilter -> AmpShaper ; + bypass bus
//         -> [reserved insert point: future NAM/IR amp stage]
//         -> Delay -> Reverb -> width -> output gain -> global dry/wet -> bypass xfade
//
// Everything runs in chunks of <= 32 samples: parameters are smoothed and
// modulated once per chunk (ParamState + ModEngine), gains/cutoff are
// interpolated per sample.

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>

#include "dsp/AmpShaper.h"
#include "dsp/Granular.h"
#include "dsp/InputStage.h"
#include "dsp/MainFilter.h"
#include "dsp/ModEngine.h"
#include "dsp/ParamState.h"
#include "dsp/Resonator.h"
#include "dsp/Reverb.h"
#include "dsp/Spectral.h"
#include "dsp/StereoDelay.h"
#include "dsp/Voicer.h"
#include "dsp/VizData.h"

namespace synthia::dsp
{
/** Host transport info for one block. */
struct HostTiming
{
    double bpm = 120.0;
    double ppqAtBlockStart = 0.0;
    double quartersPerBar = 4.0;
    bool playing = false;
    bool valid = false;
};

class DryLayer
{
public:
    void prepare (double sr) noexcept { lp[0].setup (800.0f, sr); lp[1].setup (800.0f, sr); reset(); }
    void reset() noexcept { lp[0].reset(); lp[1].reset(); }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, float tilt) noexcept
    {
        const float gl = dbToGain (-6.0f * tilt), gh = dbToGain (6.0f * tilt);
        for (int s = 0; s < n; ++s)
        {
            const float l0 = lp[0].lp (inL[s]), r0 = lp[1].lp (inR[s]);
            outL[s] = l0 * gl + (inL[s] - l0) * gh;
            outR[s] = r0 * gl + (inR[s] - r0) * gh;
        }
    }

private:
    OnePole lp[2];
};

class Engine
{
public:
    enum Layer { layerVoicer, layerSpectral, layerReso, layerGranular, layerDry, numLayers };

    Engine();

    /** Must be called before process (allocates). */
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void attachParameter (int index, std::atomic<float>* value) noexcept { params.attach (index, value); }

    /** buffer: 2 channels (stereo out). numInputChannels 1 or 2. */
    void process (juce::AudioBuffer<float>& buffer, int numInputChannels, const HostTiming& host) noexcept;

    /** Latency the engine wants to report (Align Spectral). Safe to call from any thread. */
    int getDesiredLatency() const noexcept { return desiredLatency.load(); }
    /** Latency for given settings (message thread, e.g. prepareToPlay). */
    static int latencyFor (bool align, int spSizeChoice, double sampleRate) noexcept
    {
        return align ? Spectral::sizeForChoice (spSizeChoice, sampleRate) : 0;
    }

    double getTailSeconds() const noexcept { return 60.0; }

    // ---- UI read-outs (relaxed atomics, written by the audio thread)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<int> pickCount { 0 };
    std::atomic<float> envOut { 0.0f };
    ParamState params;
    ModEngine mod;
    VizData viz; // live data for the editor displays

    /** Test hooks. */
    Voicer& getVoicer() noexcept { return voicer; }
    Spectral& getSpectral() noexcept { return spectral; }
    Granular& getGranular() noexcept { return granular; }
    bool isLayerProcessing (int layer) const noexcept { return layerRunning[layer]; }

private:
    void processChunk (float* L, float* R, int n, int numInputChannels, const HostTiming& host, int offsetInBlock) noexcept;

    /** Reserved insert point (after mixer/amp shaper, before the delay) for a
        future NAM / IR amp stage. Intentionally a no-op for now. */
    void processInsert (float*, float*, int) noexcept {}

    double sr = 48000.0;
    InputStage input;
    Voicer voicer;
    Spectral spectral;
    ResonatorLayer reso;
    Granular granular;
    DryLayer dry;
    MainFilter filter;
    AmpShaper amp;
    StereoDelay delay;
    Reverb reverb;

    Fader layerFade[numLayers], delayFade, reverbFade, bypassFade;
    bool layerNeedsReset[numLayers] {}, layerRunning[numLayers] {};
    bool delayNeedsReset = true, reverbNeedsReset = true;

    // Align Spectral: delay lines for the non-spectral paths
    DelayLine alignL, alignR, alignMono, alignRawL, alignRawR;
    int alignSamples = 0;
    std::atomic<int> desiredLatency { 0 };

    double freePpq = 0.0;
    bool prepared = false;
    double tlColLen = 750.0, tlCount = 0.0;
    float tlPeak = 0.0f;
    bool tlPick = false;
    float pickEnvForMod = 0.0f;
    int pickCounter = 0;
};
} // namespace synthia::dsp
