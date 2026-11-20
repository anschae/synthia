#include "dsp/Engine.h"

namespace synthia::dsp
{
// Layout assumptions used by the DSP (parameters generated in consecutive groups).
static_assert (pid::m1Dst == pid::m1Src + 1 && pid::m1Amt == pid::m1Src + 2 && pid::m1Via == pid::m1Src + 3 && pid::m2Src == pid::m1Src + 4);
static_assert (pid::seqV32 == pid::seqV1 + 31 && pid::seqG32 == pid::seqG1 + 31 && pid::macro4 == pid::macro1 + 3);
static_assert (pid::chop16 == pid::chop1 + 15);
static_assert (pid::c2Root == pid::c1Root + 4 && pid::c1Type == pid::c1Root + 1 && pid::c1Oct == pid::c1Root + 2 && pid::c1Spread == pid::c1Root + 3);
static_assert (pid::spOn == pid::vcOn + 6 && pid::dryOn == pid::vcOn + 24);

namespace
{
// Mixer parameter indices per layer: On, Level, Pan, Mute, Solo, ToFlt (consecutive).
constexpr int kLayerBase[Engine::numLayers] = { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn, pid::dryOn };
} // namespace

Engine::Engine()
{
    for (int i = 0; i < pid::count; ++i)
        params.attach (i, nullptr);
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    params.prepare (sr);
    mod.prepare (sr);
    input.prepare (sr);
    voicer.prepare (sr);
    spectral.prepare (sr);
    reso.prepare (sr);
    granular.prepare (sr);
    dry.prepare (sr);
    filter.prepare (sr);
    amp.prepare (sr);
    delay.prepare (sr);
    reverb.prepare (sr);
    const int maxAlign = Spectral::sizeForChoice (2, sr) + 8;
    for (auto* d : { &alignL, &alignR, &alignMono, &alignRawL, &alignRawR })
        d->prepare (maxAlign);
    prepared = true;
    for (auto& f : layerFade) f.setup (15.0f, sr);
    delayFade.setup (20.0f, sr);
    reverbFade.setup (20.0f, sr);
    bypassFade.setup (20.0f, sr);
    reset();
}

void Engine::reset()
{
    params.snapToTargets();
    mod.reset();
    input.reset();
    voicer.reset();
    spectral.reset();
    reso.reset();
    granular.reset();
    dry.reset();
    filter.reset();
    amp.reset();
    delay.reset();
    reverb.reset();
    for (auto* d : { &alignL, &alignR, &alignMono, &alignRawL, &alignRawR })
        d->clear();
    for (int i = 0; i < numLayers; ++i)
    {
        layerFade[i].value = layerFade[i].target = 0.0f;
        layerNeedsReset[i] = false;
        layerRunning[i] = false;
    }
    delayFade.value = delayFade.target = params.on (pid::dlyOn) ? 1.0f : 0.0f;
    reverbFade.value = reverbFade.target = params.on (pid::rvOn) ? 1.0f : 0.0f;
    bypassFade.value = bypassFade.target = params.on (pid::bypass) ? 1.0f : 0.0f;
    delayNeedsReset = reverbNeedsReset = false;
    freePpq = 0.0;
    alignSamples = latencyFor (params.on (pid::alignSpec), params.choice (pid::spSize), sr);
    desiredLatency.store (alignSamples);
}

void Engine::process (juce::AudioBuffer<float>& buffer, int numInputChannels, const HostTiming& host) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || total <= 0)
        return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    if (numInputChannels < 2)
        std::copy (L, L + total, R);
    if (! prepared) // some hosts call process before prepareToPlay: pass through
        return;
    for (int i = 0; i < total; ++i) // never let NaN/Inf from the host into the feedback loops
    {
        L[i] = sanitize (L[i]);
        R[i] = sanitize (R[i]);
    }
    for (int off = 0; off < total; off += kChunk)
        processChunk (L + off, R + off, std::min (kChunk, total - off), numInputChannels, host, off);
}

void Engine::processChunk (float* L, float* R, int n, int numInputChannels, const HostTiming& host, int offset) noexcept
{
    juce::ignoreUnused (numInputChannels);
    // ---- parameters (+ modulation computed at the end of the previous chunk)
    params.beginChunk (n, mod.modOffsets());
    const auto& p = params;

    // ---- transport
    Transport t;
    t.bpm = host.valid && host.bpm > 1.0 ? host.bpm : 120.0;
    t.quartersPerBar = host.valid ? host.quartersPerBar : 4.0;
    const double spq = t.samplesPerQuarter (sr);
    if (host.valid && host.playing)
    {
        t.playing = true;
        t.ppq = host.ppqAtBlockStart + (double) offset / spq;
        freePpq = t.ppq + (double) n / spq;
    }
    else
    {
        t.playing = false;
        t.ppq = freePpq;
        freePpq += (double) n / spq;
    }

    // ---- latency / align
    const int wantAlign = latencyFor (p.on (pid::alignSpec), p.choice (pid::spSize), sr);
    if (wantAlign != alignSamples)
    {
        alignSamples = wantAlign;
        desiredLatency.store (wantAlign);
    }

    // ---- input stage
    InputStage::Settings is;
    is.gainDb = p.cur (pid::inGain);
    is.hpfHz = p.cur (pid::hpf);
    is.gateOn = p.on (pid::gateOn);
    is.gateThrDb = p.cur (pid::gateThr);
    is.gateRelMs = p.cur (pid::gateRel);
    input.setChunk (is, n);
    InputStage::AnalysisSettings as;
    as.envAtkMs = p.cur (pid::envAtk);
    as.envRelMs = p.cur (pid::envRel);
    as.pickSens = p.cur (pid::pickSens);
    as.pickHoldMs = p.cur (pid::pickHold);
    as.pickDecayMs = p.cur (pid::pickDecay);
    input.setAnalysis (as);

    float rawL[kChunk], rawR[kChunk];   // untouched input (global dry / bypass)
    float stL[kChunk], stR[kChunk];     // after input stage (aligned)
    float monoNow[kChunk];              // after input stage, undelayed (spectral)
    float mono[kChunk];                 // after input stage, aligned
    bool picks[kChunk];
    bool anyPick = false;
    for (int s = 0; s < n; ++s)
    {
        rawL[s] = L[s];
        rawR[s] = R[s];
        float l = L[s], r = R[s];
        input.processSample (l, r, s);
        monoNow[s] = 0.5f * (l + r);
        if (alignSamples > 0)
        {
            alignL.push (l); alignR.push (r); alignMono.push (monoNow[s]);
            alignRawL.push (rawL[s]); alignRawR.push (rawR[s]);
            stL[s] = alignL.at (alignSamples);
            stR[s] = alignR.at (alignSamples);
            mono[s] = alignMono.at (alignSamples);
            rawL[s] = alignRawL.at (alignSamples);
            rawR[s] = alignRawR.at (alignSamples);
        }
        else
        {
            stL[s] = l; stR[s] = r; mono[s] = monoNow[s];
        }
        picks[s] = input.analyse (mono[s]);
        anyPick = anyPick || picks[s];
    }
    if (anyPick)
        pickCount.fetch_add (1, std::memory_order_relaxed);
    {
        const float pk = input.takePeak();
        if (pk > inPeak.load (std::memory_order_relaxed))
            inPeak.store (pk, std::memory_order_relaxed);
    }
    envOut.store (input.envNorm(), std::memory_order_relaxed);

    // Fully bypassed: output the (aligned) input and skip all processing (saves CPU).
    bypassFade.target = p.on (pid::bypass) ? 1.0f : 0.0f;
    if (bypassFade.value >= 1.0f && bypassFade.target >= 1.0f)
    {
        std::copy (rawL, rawL + n, L);
        std::copy (rawR, rawR + n, R);
        for (int i = 0; i < numLayers; ++i)
        {
            layerFade[i].value = 0.0f; // layers fade in again after bypass
            layerRunning[i] = false;
            layerNeedsReset[i] = true;
        }
        mod.advance (n, p, t, anyPick, input.envNorm(), input.pickEnvelope());
        return;
    }

    ChunkContext ctx { p, t, picks, input.envNorm(), input.envLinear(), input.pickEnvelope(), n, sr };

    // ---- layers + mixer
    float busFL[kChunk] {}, busFR[kChunk] {}, busBL[kChunk] {}, busBR[kChunk] {};
    bool anySolo = false;
    for (int i = 0; i < numLayers; ++i)
        anySolo = anySolo || p.on (kLayerBase[i] + 4);
    for (int i = 0; i < numLayers; ++i)
    {
        const int b = kLayerBase[i];
        const bool enabled = p.on (b) && ! p.on (b + 3) && (! anySolo || p.on (b + 4)) && p.cur (b + 1) > 0.0001f;
        auto& fade = layerFade[i];
        fade.target = enabled ? 1.0f : 0.0f;
        if (fade.silent())
        {
            layerRunning[i] = false;
            layerNeedsReset[i] = true;
            continue;
        }
        if (layerNeedsReset[i])
        {
            switch (i)
            {
                case layerVoicer: voicer.reset(); break;
                case layerSpectral: spectral.reset(); break;
                case layerReso: reso.reset(); break;
                case layerGranular: granular.reset(); break;
                default: dry.reset(); break;
            }
            layerNeedsReset[i] = false;
        }
        layerRunning[i] = true;
        float oL[kChunk], oR[kChunk];
        switch (i)
        {
            case layerVoicer: voicer.process (mono, oL, oR, ctx); break;
            case layerSpectral: spectral.process (monoNow, oL, oR, ctx); break;
            case layerReso: reso.process (mono, oL, oR, ctx); break;
            case layerGranular: granular.process (mono, oL, oR, ctx); break;
            default: dry.process (stL, stR, oL, oR, n, p.cur (pid::dryTilt)); break;
        }
        const bool toFilter = p.on (b + 5);
        float* dL = toFilter ? busFL : busBL;
        float* dR = toFilter ? busFR : busBR;
        for (int s = 0; s < n; ++s)
        {
            const float lvl = p.interp (b + 1, s, n) * fade.next();
            float gl, gr;
            panGains (p.interp (b + 2, s, n), gl, gr);
            dL[s] += oL[s] * lvl * gl;
            dR[s] += oR[s] * lvl * gr;
        }
    }

    // ---- main filter + amp shaper on the filter bus
    const bool fltOn = p.on (pid::fltOn);
    const int fltType = p.choice (pid::fltType);
    const float envAmt = p.cur (pid::fltEnv), pickAmt = p.cur (pid::fltPick);
    const float res = p.cur (pid::fltRes), fdrive = p.cur (pid::fltDrive);
    AmpShaper::Settings ams;
    ams.adsrAmt = p.cur (pid::adsrAmt);
    ams.a = p.cur (pid::adsrA);
    ams.d = p.cur (pid::adsrD);
    ams.s = p.cur (pid::adsrS);
    ams.r = p.cur (pid::adsrR);
    ams.swellOn = p.on (pid::swellOn);
    ams.swellMs = p.cur (pid::swellTime);
    ams.chopOn = p.on (pid::chopOn);
    ams.chopDepth = p.cur (pid::chopDepth);
    ams.chopSmoothMs = p.cur (pid::chopSmooth);
    ams.stepQuarters = syncDivQuarters (p.choice (pid::chopDiv));
    ams.steps = 0;
    for (int k = 0; k < 16; ++k)
        if (p.on (pid::chop1 + k)) ams.steps |= (1u << k);
    amp.setChunk (ams);
    const float pickEnvNow = input.pickEnvelope();
    const float envNow = input.envNorm();
    for (int s = 0; s < n; ++s)
    {
        float l = busFL[s], r = busFR[s];
        if (fltOn)
        {
            const float cutBase = p.interp (pid::fltCut, s, n);
            const float oct = 4.0f * (envAmt * envNow + pickAmt * pickEnvNow);
            filter.processSample (l, r, fltType, cutBase * std::exp2 (oct), res, fdrive);
        }
        const float g = amp.next (picks[s], input.envLinear(), t.ppq + (double) s / spq);
        L[s] = l * g + busBL[s];
        R[s] = r * g + busBR[s];
    }

    // ---- reserved insert point (future NAM / IR amp stage)
    processInsert (L, R, n);

    // ---- delay
    delayFade.target = p.on (pid::dlyOn) ? 1.0f : 0.0f;
    if (! delayFade.silent())
    {
        if (delayNeedsReset) { delay.reset(); delayNeedsReset = false; }
        StereoDelay::Settings ds;
        ds.timeSamples = p.on (pid::dlySync) ? (float) (syncDivQuarters (p.choice (pid::dlyDiv)) * spq)
                                             : p.cur (pid::dlyTime) * 0.001f * (float) sr;
        ds.feedback = p.cur (pid::dlyFb);
        ds.mix = p.cur (pid::dlyMix);
        ds.pingPong = p.on (pid::dlyPing);
        ds.lowCut = p.cur (pid::dlyLow);
        ds.highCut = p.cur (pid::dlyHigh);
        ds.modRate = p.cur (pid::dlyModRate);
        ds.modDepth = p.cur (pid::dlyModDepth);
        ds.width = p.cur (pid::dlyWidth);
        delay.setChunk (ds, n);
        for (int s = 0; s < n; ++s)
        {
            float l = L[s], r = R[s];
            delay.processSample (l, r);
            const float f = delayFade.next();
            L[s] += (l - L[s]) * f;
            R[s] += (r - R[s]) * f;
        }
    }
    else
        delayNeedsReset = true;

    // ---- reverb
    reverbFade.target = p.on (pid::rvOn) ? 1.0f : 0.0f;
    if (! reverbFade.silent())
    {
        if (reverbNeedsReset) { reverb.reset(); reverbNeedsReset = false; }
        Reverb::Settings rs;
        rs.mode = p.choice (pid::rvMode);
        rs.size = p.cur (pid::rvSize);
        rs.decay = p.cur (pid::rvDecay);
        rs.infinite = p.on (pid::rvInf);
        rs.preSamples = p.on (pid::rvPreSync) ? (float) (syncDivQuarters (p.choice (pid::rvPreDiv)) * spq)
                                               : p.cur (pid::rvPre) * 0.001f * (float) sr;
        rs.preSamples = std::min (rs.preSamples, (float) (2.4 * sr));
        rs.diffusion = p.cur (pid::rvDiff);
        rs.dampLo = p.cur (pid::rvDampLo);
        rs.dampHi = p.cur (pid::rvDampHi);
        rs.modDepth = p.cur (pid::rvModDepth);
        rs.modRate = p.cur (pid::rvModRate);
        rs.shimmer = p.cur (pid::rvShim);
        static constexpr float shimSemis[] = { 12, 7, 19, 24, -12 };
        rs.shimRatio = std::exp2 (shimSemis[juce::jlimit (0, 4, p.choice (pid::rvShimInt))] / 12.0f);
        rs.bloomSec = p.cur (pid::rvBloom);
        rs.duck = p.cur (pid::rvDuck);
        rs.width = p.cur (pid::rvWidth);
        rs.mix = p.cur (pid::rvMix);
        rs.env = input.envLinear() * 4.0f;
        reverb.setChunk (rs, n);
        for (int s = 0; s < n; ++s)
        {
            float l = L[s], r = R[s];
            reverb.processSample (l, r);
            const float f = reverbFade.next();
            L[s] += (l - L[s]) * f;
            R[s] += (r - R[s]) * f;
        }
    }
    else
        reverbNeedsReset = true;

    // ---- width, output gain, global mix, bypass
    bypassFade.target = p.on (pid::bypass) ? 1.0f : 0.0f;
    float outPk = 0.0f;
    for (int s = 0; s < n; ++s)
    {
        const float w = p.interp (pid::outWidth, s, n);
        const float m = 0.5f * (L[s] + R[s]), sd = 0.5f * (L[s] - R[s]) * w;
        const float g = dbToGain (p.interp (pid::outGain, s, n));
        float l = (m + sd) * g, r = (m - sd) * g;
        const float mix = p.interp (pid::mix, s, n);
        l = rawL[s] + (l - rawL[s]) * mix;
        r = rawR[s] + (r - rawR[s]) * mix;
        const float b = bypassFade.next();
        l += (rawL[s] - l) * b;
        r += (rawR[s] - r) * b;
        // final safety: never send NaN/Inf or absurd levels to the host
        l = clamp (sanitize (l), -8.0f, 8.0f);
        r = clamp (sanitize (r), -8.0f, 8.0f);
        L[s] = l;
        R[s] = r;
        outPk = std::max (outPk, std::max (std::abs (l), std::abs (r)));
    }
    if (outPk > outPeak.load (std::memory_order_relaxed))
        outPeak.store (outPk, std::memory_order_relaxed);

    // ---- modulation for the next chunk
    mod.advance (n, p, t, anyPick, input.envNorm(), input.pickEnvelope());
}
} // namespace synthia::dsp
