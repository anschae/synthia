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
 → [reserved: future NAM/IR stage]
 → Delay → Reverb (Cosmos/Shimmer/Abyss/Bloom/Cloud) → width/out/mix
Modulation: env, pick env, 2 LFOs, random, step seq (polymeter, triplets/quintuplets), 4 macros → 16-slot matrix
```
_(Update with actual source file/class layout.)_

## Rules
- Real-time safety: no allocation, locks, I/O or logging on the audio thread; preallocate in `prepareToPlay`; `ScopedNoDenormals`; smooth all parameters; any block size / sample rate 44.1–192 kHz.
- Parameter IDs are permanent once released. Bump state version + add migration when changing state layout.
- Layers that are off / at level 0 skip processing (fade in on re-enable).
- Nonlinear stages use oversampling per the Quality setting.
- Feedback paths (delay, reverb, granular feedback) must be unconditionally stable (internal limiting).
- Tune DSP with `synthia-render` on test DIs; keep tuning constants centralised and documented. The user judges the sound by ear.

## Status
- [ ] M0 Scaffold + CI + passthrough
- [ ] M1 Core infra (params, input, dry, mixer, filter, amp shaper, delay, mod engine, presets, render CLI, tests)
- [ ] M2 Voicer
- [ ] M3 Reverb
- [ ] M4 Granular
- [ ] M5 Spectral
- [ ] M6 Resonator / Vocoder / Formant
- [ ] M7 Step seq, random, chopper, stutter, mod UI
- [ ] M8 GUI pass, factory presets, randomize/A-B, performance, user guide

## Decisions log
- 2026-11-20: No note detection (user decision) — processing-based synthesis only.
- 2026-11-20: All layers in v1 (built in milestones). Granular + ambient/shimmer reverb + standard delay (ping-pong, host sync) added by user request.
- 2026-11-20: NAM/IR stage deferred to a later version.

## Known issues / gotchas
_(none yet)_
