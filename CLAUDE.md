# CLAUDE.md — Synthia

Guidance for Claude sessions working in this repo. **Keep this file up to date** after every milestone, bug fix or design decision — the user continues development across many sessions and relies on it.

## What Synthia is
A VST3/AU/Standalone **audio effect** that creatively synthesizes and mangles a clean electric-guitar DI signal: polyphonic synth tones, spectral pads, tuned resonators/vocoder, granular clouds, plus a clean delay and an ambient/shimmer reverb, all tied together by a modulation matrix.

**Hard rule: no pitch/note detection, no MIDI note output.** Everything is derived by processing the audio. A pick/onset detector (no pitch) is allowed.

## User context
- Andreas, guitarist (mostly 7-string, low tunings down to ~52 Hz; also 8-string), prog/djent. Records clean DI, reamps later.
- DAWs: Cubase (main), Logic Pro (needs AU).
- Gets builds from GitHub Actions artifacts — does not build locally. CI must always be green on `main`.
- Sister projects (same toolchain/conventions): `anschae/irene` (IR loader), `anschae/prog-delay`.
- Planned later, **not now**: a NAM (A1/A2) + 1–2 IR amp/cab stage. Keep the reserved insert point (after mixer/amp shaper, before delay) and the option for a parallel amp source in mind; don't implement.

## Identity
- Product: `Synthia` · Manufacturer: `anschae` · Manufacturer code: `Ansc` (same as IRene / Prog Delay) · Plugin code: `Snth` · Bundle id: `com.anschae.synthia`

## Build & CI
- JUCE 8.0.15 (pinned in `CMakeLists.txt`, same as Prog Delay; IRene uses JUCE 9), Catch2 v3.16.1, CMake ≥ 3.24, C++20.
- Targets: VST3, AU (macOS), Standalone; `synthia-render` CLI (offline WAV renderer); DSP test target.
- GitHub Actions: macOS universal (arm64+x86_64, min 11) + Windows x64; artifacts `Synthia-macOS`, `Synthia-Windows`; pluginval (strictness ≥ 5), auval; tests.
- macOS: ad-hoc signed — README explains `xattr -dr com.apple.quarantine`.
- Build (Linux/macOS):
  ```sh
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=$PWD/.deps
  cmake --build build --parallel
  ctest --test-dir build --output-on-failure
  ```
  Release uses LTO (slow links). For iteration: `-B build-dev -DCMAKE_BUILD_TYPE=RelWithDebInfo`.
  Local JUCE/Catch2 checkouts can be reused with `-DFETCHCONTENT_SOURCE_DIR_JUCE=… -DFETCHCONTENT_SOURCE_DIR_CATCH2=…`.
- Options: `SY_BUILD_TESTS` (ON), `SY_BUILD_PLUGIN` (ON), `SY_BUILD_RENDER` (ON), `SY_BUILD_TOOLS` (OFF, UI snapshot).
- All sources under `src/` are globbed into the INTERFACE library `SYShared` (except `PluginEntry.cpp`); tests, the renderer and the plugin link it.
- CI: `.github/workflows/build.yml` — jobs linux (tests + render CLI + pluginval under xvfb), macos (universal, tests, ad-hoc codesign, pluginval, `auval -strict -v aufx Snth Ansc`), windows (x64, static-runtime check, tests, pluginval), package (checks zips; on `v*` tags attaches them to a GitHub Release). pluginval strictness = `PLUGINVAL_STRICTNESS` (10).
- The `gh` CLI in Claude sessions may have no valid token; CI status can then be read via the public API (`curl https://api.github.com/repos/anschae/synthia/actions/runs`) if the repo is public.

## Architecture
```
Input (gain, HPF, gate, env follower, pick trigger)
 ├─ A Voicer   (filter-bank → per-band phase → new waveforms; ~0 latency)
 ├─ B Spectral (STFT freeze/blur/partials/shift/stretch/robot; "Align Spectral" latency option, default off)
 ├─ C Resonator / Vocoder / Formant (4 chord slots)
 ├─ D Granular (8 s buffer, 64-grain pool, stutter)
 └─ E Dry
 → Mixer → Main filter → Amp shaper (pick ADSR, swell, chopper)
 → [reserved: future NAM/IR stage]  (Engine::processInsert, a no-op)
 → Delay → Reverb (Cosmos/Shimmer/Abyss/Bloom/Cloud) → width/out/mix
Modulation: env, pick env, 2 LFOs, random, step seq (polymeter, triplets/quintuplets), 4 macros → 16-slot matrix
```

### File layout
```
src/params/ParamList.h   GENERATED parameter table (X-macro) -- edit scripts/gen_params.py, then run it
src/params/Params.*      ParamSpec (ranges/curves/units/sections/tooltips), pid:: indices, choice lists,
                         note divisions, APVTS layout, value formatting, modDestinations()
src/dsp/ParamState.h     audio-thread parameter view: smoothing (normalised domain, 20 ms) + modulation per chunk
src/dsp/ModEngine.h      Lfo, StepSequencer, ModEngine (sources + 16-slot matrix -> normalised offsets)
src/dsp/Engine.*         complete signal flow (chunks of <= 32 samples), mixer, align delays, bypass
src/dsp/InputStage.h     gain, 24 dB HPF, gate, envelope follower, onset (pick) detector
src/dsp/Voicer.h         layer A (+ voicer_tuning constants)
src/dsp/Spectral.h       layer B (complex FFT, preallocated)
src/dsp/Resonator.h      layer C (resonator / vocoder / formant, chordNotes(), reso_tuning)
src/dsp/Granular.h       layer D (grains + stutter)
src/dsp/MainFilter.h     ladder / SVF / comb / formant + ADAA drive
src/dsp/Formant.h        3-formant vowel filter (shared)
src/dsp/AmpShaper.h      pick ADSR, swell, chopper
src/dsp/StereoDelay.h    delay
src/dsp/Reverb.h         16-line FDN reverb + pitch shifter (reverb_tuning constants)
src/dsp/Common.h         helpers: DelayLine, Svf, OnePole, fastTanh, safeLimit, sinCycle, fastAtan2, Rng ...
src/model/PresetManager.* factory + user presets (.synthia XML in Documents/Synthia/Presets), A/B
src/model/FactoryPresets.* factory presets as parameter differences to the defaults
src/model/Randomizer.h   per-section / global randomize with chaos
src/dsp/VizData.h        lock-free data for the editor's live displays (relaxed atomics, written only while an editor is open)
src/ui/                  LookAndFeel, Widgets (ParamKnob/Toggle/Combo/DestButton/Meter/SectionPanel with footer/side slots),
                         Viz (live displays), Pages (Layers, Filter & FX, Modulation incl. StepSeqEditor + MatrixPanel), TopBar
src/PluginProcessor.*    APVTS + Engine + state (SynthiaState root, stateVersion, migrate()) + latency timer
src/PluginEditor.*       MainView at design size 1200x790, scaled 75-200 % with an AffineTransform
tools/Render.cpp         synthia-render CLI     tools/Snapshot.cpp  editor -> PNG (SY_BUILD_TOOLS)
scripts/gen_params.py    parameter table generator   scripts/mkpreset.py  state file from id=value pairs
tests/                   Catch2: CoreTests (params, state, fuzz, decay, presets, mod), LayerTests (per layer)
```

### Key design decisions
- **Parameters:** ~340, all in the APVTS (incl. step-seq values, chopper steps, chord slots, matrix) so
  presets/A-B/randomize/state are uniform. Matrix destination = choice index into `modDestinations()`
  (all modulatable F/I params in table order) -> **append new params at the end of gen_params.py only**.
- **Modulation** is added in the normalised domain after smoothing; `ParamState::cur()` is the
  effective value. Per-sample interpolation (`interp`) for gains, pan, cutoff, width, out gain.
  Mod sources are computed at the end of a chunk and applied in the next one (<= 0.7 ms lag).
- **Chunking:** everything runs in chunks of <= 32 samples (kChunk). Any host block size works.
- **Layers off** (On off / mute / level 0 / not soloed) are skipped entirely after a 15 ms fade and
  reset when re-enabled. Delay/reverb likewise (20 ms fade).
- **Voicer:** complex two-pole resonator per band (cascade of two identical one-poles), amplitude
  = 2|y|, phase rate from the smoothed product y[n]·conj(y[n-1]) (atan2 every 4 samples), free-running
  phase accumulators for main/sub/oct/interval, polyBLEP waveforms, oscillators muted above 0.45 fs.
  Focus = (a_k / max neighbour)^(6·focus) for non-peaks. Drive + fold oversampled (JUCE IIR polyphase,
  2x/4x, latency of ~1-3 samples is *not* reported -- the Voicer is a resynthesis, not phase-coherent
  with the dry). Crush at base rate. Tuning constants: `voicer_tuning` in Voicer.h.
- **Main filter drive** uses ADAA tanh instead of oversampling (keeps the dry path latency-free).
- **Spectral:** Hann, hop N/4, latency exactly N. N = 1024/2048/4096 x (1, 2, 4 at >70k / >150k).
  Phase vocoder only when shift/stretch are active (otherwise the analysis phase is reused ->
  transparent). Freeze = second spectrum with its own phase accumulators + random drift.
  Align Spectral: input-stage outputs, analysis taps and the global dry are delayed by N; the
  processor reports latency from a 10 Hz message-thread timer (`Engine::getDesiredLatency`).
- **Reverb:** 16 lines, fast Walsh-Hadamard matrix, per-line LP/HP damping, gains from RT60.
  Shimmer is crossfaded into the loop (energy neutral) + slow tank limiter; with Infinite the
  shimmer is not fed back (pure freeze) and input is muted. Abyss forces downward shifting.
- **Granular:** grains never cross the write head; stutter records the slice live at every
  retrigger point (1/4, 1/2, bar) and repeats it (3 ms seam crossfade).
- **Resonator:** chords are user-set (chord slots); nothing is detected. String = comb with LP
  damping + dispersion allpass; Metal/Glass = 4 modes per voice; Bowed = string + input-envelope noise.
- **Pick detector:** HF (900 Hz HP) fast env vs slow env ratio + floor, both from Pick Sensitivity;
  retrigger hold. Envelope follower output 0..1 = -60..0 dBFS.
- **UI scale / chaos** are stored in the session state but not applied from presets or A/B.
- **Live displays** (v0.2, mockup: claude.ai artifact "Synthia – Visualisierungen Mockup"): the editor sets
  `engine.viz.active`; the engine and layers then publish into `dsp::VizData` with relaxed atomic stores
  (per chunk, or per FFT frame for Spectral; timeline/granular columns per sample-accumulated peaks).
  UI components in `ui/Viz.*` derive from `VizBox` and repaint from the editor's 30 Hz timer, only for
  the current page and only while showing. Displays: pick timeline (input), Voicer bands, spectrum
  (+ frozen + partials), chord keyboards + resonator voices + vowel map, granular buffer/grains/stutter,
  filter curve (analytic, incl. live modulated cutoff), pick ADSR (live stage), chopper playhead,
  delay echoes, reverb decay + shimmer tiers, LFO previews (live phase), matrix activity per slot.
  Things computed from parameters (curves, chord notes) use `VizBox::effective()` (modulated value).
- **Voicer unison** (v0.3): params `vcUniVoices` (1-7), `vcUniDetune` (+-cents of the outer voices),
  `vcUniWidth`; main voice only (sub/oct/interval stay single), own phase per band and voice,
  1/sqrt(N) gain, stereo per voice on top of the band spread. Worst case 72 bands x 7 voices + all
  voices ~27 % of one cloud core. `vcDetune` is shown as "Drift" (ID unchanged). WaveView (side of the
  Voices row) draws the morph + the mix of all voices.
- **SectionPanel** layout slots: `setFooter()` (below the last row) and `addSide (comp, row)`
  (right of the last control in a row). The chord slots are `ChordCard`s in the C panel footer.

## Rules
- Real-time safety: no allocation, locks, I/O or logging on the audio thread; preallocate in `prepareToPlay`; `ScopedNoDenormals`; smooth all parameters; any block size / sample rate 44.1–192 kHz.
- Parameter IDs are permanent once released. Bump state version + add migration when changing state layout.
- Layers that are off / at level 0 skip processing (fade in on re-enable).
- Nonlinear stages use oversampling per the Quality setting.
- Feedback paths (delay, reverb, granular feedback) must be unconditionally stable (internal limiting).
- Tune DSP with `synthia-render` on test DIs; keep tuning constants centralised and documented. The user judges the sound by ear.

## Status
- [x] M0 Scaffold + CI + passthrough
- [x] M1 Core infra (params, input, dry, mixer, filter, amp shaper, delay, mod engine, presets, render CLI, tests)
- [x] M2 Voicer (incl. Quality = Eco/2x/4x oversampling of drive + fold)
- [x] M3 Reverb (Cosmos/Shimmer/Abyss/Bloom/Cloud)
- [x] M4 Granular (incl. stutter)
- [x] M5 Spectral (incl. Align Spectral latency handling)
- [x] M6 Resonator / Vocoder / Formant (4 chord slots)
- [x] M7 Step seq (polymeter, triplets/quintuplets, glide), random, chopper, stutter sync, mod UI (rings, seq editor, matrix)
- [x] M8 GUI pass, 32 factory presets, randomize (global + per section, chaos) / A-B, performance, README user guide

Milestones 1-6 were built in one go (one commit) because the parameter table and engine are
shared; CI was green after each push (2026-11-20: all jobs incl. auval + pluginval 10).

- [x] v0.2 Live displays for every section (2026-11-24)
- [x] v0.3 Voicer unison (1-7 voices) + waveform display; "Supersaw Unison Lead" preset (2026-11-24)

**Not yet done / next steps**
- Nobody has played it yet: tune by ear with real DIs (`synthia-render`). Factory preset levels were
  set on a synthetic Karplus-Strong DI (`scripts/` has no DI; render with your own files).
- Pick detector thresholds tuned on synthetic plucks only -> check `pickSens` defaults on real DIs.
- Possible extras: MIDI learn, preset tags, input/output spectrum overlay on the filter curve.

**Performance** (synthia-render, 48 kHz, block 64, RelWithDebInfo, cloud Xeon 2.1 GHz):
Init (Voicer 48 bands + reverb) ~8 % of one core; all five layers + delay + reverb at defaults ~11 %;
worst case (72 bands, 4x OS, all voices, 100 grains/s x 500 ms, metal resonators, all FX) ~25 %.
Apple Silicon / a recent laptop is 2-3x faster per core.

## Decisions log
- 2026-11-20: No note detection (user decision) — processing-based synthesis only.
- 2026-11-20: All layers in v1 (built in milestones). Granular + ambient/shimmer reverb + standard delay (ping-pong, host sync) added by user request.
- 2026-11-20: NAM/IR stage deferred to a later version.
- 2026-11-20: JUCE 8.0.15 + Catch2 v3.16.1 like Prog Delay; manufacturer code `Ansc` (shared with IRene/Prog Delay).
- 2026-11-20: All parameters (incl. step values, chopper steps, chord slots, matrix) live in the APVTS.
- 2026-11-20: Quality: Eco = no oversampling, Normal = 2x, High = 4x (Voicer drive/fold only);
  main-filter drive uses ADAA to keep the dry path latency-free.
- 2026-11-20: Spectral FFT size scales with sample rate; latency = N; Align Spectral default off.
- 2026-11-20: Reverb = 16-line FDN for every mode (spec allowed 8-16).
- 2026-11-20: Stutter replaces the grain cloud while on; retrigger every 1/4, 1/2 or bar (extra param `grStutHold`).
- 2026-11-24: Unison only on the Voicer main voice (CPU; sub/oct/interval stay single). Params appended (IDs vcUni*).
- 2026-11-20: User presets: `Documents/Synthia/Presets/*.synthia` (XML state, also accepted by synthia-render).

## Known issues / gotchas
- **Never run a permanent `juce::Timer` in the processor.** On Linux, pluginval (in-process) crashed
  ~50 % of runs in the host run loop while the processor's 10 Hz latency timer was running. Latency is
  now reported via an APVTS parameter listener (alignSpec, spSize) + AsyncUpdater.
- Voicer low bands need time to build up (narrow filters): at default Sharpness the lowest band
  (~50 Hz) rises in ~50 ms. Lower Sharpness = faster/buzzier. Physics of filter banks, not a bug.
- Voicer oversampling (IIR polyphase) adds ~1-3 samples group delay that is not reported as latency.
- Quality changes switch the oversampler instantly (possible tiny click while playing).
- Reverb Infinite = pure freeze: the shimmer is not fed back while Infinite is on (otherwise it piles up).
- Synced features use the host position while playing; when stopped they free-run at the host tempo.
- `restoreState` resets all parameters to defaults before applying a stored tree (missing params ->
  defaults). UI scale and chaos are only restored from the host session, not from presets/A-B.
- Factory presets reference parameters by ID; a typo asserts in debug builds and is caught by the
  "factory presets load and render" test only indirectly -- keep `applyFactoryPreset`'s jassert.
- `formatValue()` is used by the host for parameter text; keep it allocation-light (message thread only).
- Non-ASCII UI strings must be written as `juce::CharPointer_UTF8 ("\x..")` (MSVC + JUCE assert).
- The GUI snapshot tool needs a display: `xvfb-run -a build-dev/SynthiaSnapshot_artefacts/RelWithDebInfo/SynthiaSnapshot out/ "Glass Pad"`.
  It switches all layers on and feeds synthetic plucks *after* opening the editor, so the live displays show data.
- Never call `setAlpha`/`setBounds` etc. inside `paint()` (repaint loop) -- do it in `tick()` (see VowelMap).
