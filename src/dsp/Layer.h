#pragma once
// Common interface data for the sound layers (A-E).

#include "dsp/ModEngine.h"
#include "dsp/ParamState.h"

namespace synthia::dsp
{
/** Everything a layer may need for one chunk (<= kChunk samples). */
struct ChunkContext
{
    const ParamState& p;
    const Transport& t;
    const bool* picks;   // per sample: pick detected
    float env = 0;       // envelope follower 0..1
    float envLinear = 0; // envelope follower, linear amplitude
    float pickEnv = 0;   // pick envelope 0..1
    int n = 0;
    double sr = 48000.0;
};
} // namespace synthia::dsp
