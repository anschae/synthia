# Synthia

Creative guitar-synth / sound-mangler audio effect (VST3 · AU · Standalone) for macOS and Windows.
Synthia turns a clean electric-guitar DI into polyphonic synth tones, spectral pads, tuned
resonators and chord vocoders, granular clouds and huge shimmer reverbs — **without any pitch or
note detection**. Every sound is made by processing the audio itself, so chords, bends and
8-string low notes "just work".

> Status: under development — see `CLAUDE.md` → Status for what currently works.

## Download & install

Builds come from GitHub Actions (no local build needed):

1. Open the repo on GitHub → **Actions** → the latest green **Build** run on `main`.
2. Download the artifact **Synthia-macOS** or **Synthia-Windows** (bottom of the run page) and unzip it.

### macOS
Copy the files to:

| File | Destination |
|---|---|
| `Synthia.vst3` | `~/Library/Audio/Plug-Ins/VST3/` (Cubase) |
| `Synthia.component` | `~/Library/Audio/Plug-Ins/Components/` (Logic) |
| `Synthia.app` | anywhere (standalone, for quick tests) |
| `synthia-render` | anywhere (offline renderer, optional) |

The builds are only ad-hoc signed, so macOS quarantines them. Remove the quarantine flag once
after copying (Terminal):

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Synthia.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Synthia.component
xattr -dr com.apple.quarantine /path/to/Synthia.app
xattr -dr com.apple.quarantine /path/to/synthia-render
```

Logic caches AU scans: if Synthia does not show up, restart Logic (or run
`killall -9 AudioComponentRegistrar` in Terminal) and check *Settings → Plug-in Manager*.

### Windows
Copy `Synthia.vst3` (the whole folder) to `C:\Program Files\Common Files\VST3\`, then rescan
plug-ins in Cubase. `Synthia.exe` is the standalone app, `synthia-render.exe` the offline renderer.

## Offline rendering (`synthia-render`)

```sh
synthia-render input.wav preset.xml output.wav [--bpm 120] [--tail 4]
```

Runs the complete plugin offline over a WAV file using a preset file (any preset saved from the
plugin, or an exported state XML). `--bpm` sets the host tempo for synced features, `--tail`
adds seconds of silence at the end so delays and reverbs can ring out.
