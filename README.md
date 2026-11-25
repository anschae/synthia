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

## Quick start

1. Put Synthia on a track with your **clean DI** (mono or stereo). Synthia is an audio effect, not an instrument.
2. Pick a factory preset (top bar → preset name). Good first stops: *Poly Saw Lead*, *Djent Synth Layer*,
   *Glass Pad*, *Chord Vocoder Strums*, *Shimmer Cathedral*.
3. Set **Input → Gain** so the input meter peaks around −12 … −6 dB and watch the **Pick** LED: it should flash
   once per pick. If it misses soft notes, raise **Pick Sens**; if it double-triggers, raise **Retrigger**.

Synthia never detects pitch: every sound is made from the audio itself, so chords, bends, vibrato and
8-string low notes are processed just like single notes.

## User guide

### Signal flow
```
Input (gain, HPF, gate, envelope follower, pick detector)
 ├─ A Voicer      polyphonic synth from a filter bank
 ├─ B Spectral    STFT pads: freeze, blur, partials, shift, stretch, robot/whisper
 ├─ C Resonator   tuned resonators / chord vocoder / vowel filter (4 chord slots)
 ├─ D Granular    grain clouds from an 8 s buffer, synced stutter
 └─ E Dry         your clean guitar
 → Mixer (level, pan, mute, solo, "→ Filter") → Main filter → Amp shaper → Delay → Reverb → Output
```
Layers that are switched off cost no CPU.

### Pages
- **Layers** – input stage on the left, one tab per layer (A–E), mixer strips at the bottom.
  "→ Filter" decides whether a layer goes through the main filter + amp shaper (the dry guitar usually
  should not).
- **Filter & FX** – main filter, amp shaper (pick ADSR, swell), 16-step chopper, delay, reverb.
- **Modulation** – LFOs, random, macros, step sequencer, and the 16-slot matrix.

### Live displays
Every section shows what it is doing: the **Picks** strip under Input (last 3 s, a white mark per
detected pick, yellow envelope, dashed gate threshold — use it to set Pick Sens/Retrigger), the
Voicer's band levels (grey = suppressed by Focus), the spectrum with frozen spectrum and kept
partials, chord keyboards and resonator voice meters, the granular buffer with every grain, filter
curve, pick ADSR, chopper playhead, delay echoes, reverb decay, LFO shapes and live matrix activity.
Displays only run while the window is open and cost no audio CPU otherwise.

### Input & sustain
The guitar's natural dynamics (loud pick, fast decay) are passed on to every layer. Two tools even it out:
- **Sustain compressor** (Input → Sustain): switch **Comp** on and raise **Sustain** — lower threshold,
  higher ratio and automatic make-up, like a sustainer pedal. **Attack** lets some pick through,
  **Dry too** decides whether the clean Dry layer is compressed as well. Pick detection and the envelope
  follower still listen to the uncompressed guitar. The orange line in the **Picks** display shows the
  gain reduction.
- **Voicer Hold** (Voicer → Sustain): every band is held at the level it had after the pick while the
  string fades (**Hold Time** = how long the held level takes to fade). Gives organ/pad-like sustain
  without the pumping of a compressor; muting the strings still stops the sound.

### A — Voicer (the synth)
The guitar is split into 24–72 narrow bands; every band drives its own oscillator at the band's
instantaneous frequency. **Wave** morphs sine → saw → square → pulse; **Sub**, **Oct Up** and
**Interval** add voices at ½×, 2× and the chosen interval. **Unison** (Voices 1–7, Detune, Width)
stacks detuned copies of the main voice per band for supersaw-style width; **Drift** adds a slow random
pitch movement per band. The **Wave** display shows the waveform and the mix of all voices. **Sharpness** trades attack speed (low) for
clean, stable tones (high). **Focus** suppresses side bands around each partial (less warble on chords).
**Tilt** negative = rounder (fundamentals), positive = brighter/buzzier. **Low** ~45 Hz covers 7/8-string
low notes. **Drive / Fold** are oversampled according to **Quality** (top bar).

### B — Spectral
**Freeze** captures the current sound and holds it (with a little phase **Drift** so it breathes);
**Blend** mixes frozen and live. **Blur** smears notes into pads. **Partials** keeps only the N
strongest peaks (glassy/organ). **Shift** (Hz) makes metallic, inharmonic tones; **Stretch** scales all
frequencies. *Latency:* the spectral layer lags by the FFT size. That's fine for pads; for perfectly
aligned printing switch **Align** (top bar) on – Synthia then reports the latency to the DAW and delays
everything else to match.

### C — Resonator / Vocoder / Formant
Set up to 4 chords (root, type, octave, spread) and choose the active one with **Chord Slot** (automate
it, or route the step sequencer to it – see the *Chord Sequencer* preset).
- **Resonator** – your playing excites tuned strings (String), bars (Metal), bowls (Glass) or a bowed
  string (Bowed: the bow noise follows your dynamics).
- **Vocoder** – your guitar "plays" an internal chord (saw/square/supersaw). **Noise** adds consonants
  on pick attacks.
- **Formant** – a talking A-E-I-O-U filter (modulate **Vowel** with an LFO or the envelope).

### D — Granular
Grains are read from the last 8 s. **Position** = how far back, **Spray** = random offset, **Pitch** with
optional **Quantise** (octaves = shimmer). **Freeze** stops recording. **Stutter** repeats a host-synced
slice (1/4 … 1/32, dotted/triplets) and captures a new one every 1/4, 1/2 or bar.

### Filter & amp shaper
Ladder/SVF/comb/formant filter with **Env Amt** (envelope follower) and **Pick Amt** (pick envelope) –
there is no keytracking. **Pick ADSR** reshapes the volume on every pick (synth-style attacks/decays;
sustain is held while the string rings). **Swell** = automatic volume swell. **Chopper** = 16-step gate.

### Delay & reverb
Delay: ms or synced, ping-pong, filtered feedback, tape wobble. Reverb modes: **Cosmos** (huge hall),
**Shimmer** (octave/fifth shifted feedback), **Abyss** (downward shimmer), **Bloom** (slow build-up),
**Cloud** (granular smearing). **Infinite** freezes the reverb tail (new input is muted).

### Modulation
Sources: Env Follower, Pick Env, LFO 1/2 (free or synced incl. triplets, optional retrigger on pick),
Random, Step Sequencer and Macros 1–4. Each matrix slot: **Source → Destination × Amount**, optionally
scaled by **Via** (e.g. Macro 1 fades a modulation in). Modulated knobs show a yellow ring with the live
value. The **step sequencer** has its own length (1–32 steps) independent of the bar – 7 steps of 1/16
against 4/4 gives a polymeter; rates include triplets and quintuplets. Draw values with the mouse,
double-click (or Alt-click) resets a step, the small boxes below enable glide into a step.

### Factory presets
Categories: Lead, **Pluck** (saw/sine/square plucks, glass, funk, ping-pong arp, djent layer, granular),
**Instrument** imitations (piano, electric piano, flute, clarinet, strings, cello, church/rock organ,
trumpet, choir, music box, marimba, harp, sitar drone), Bass, Djent, Pad, Ambient, Resonator, Vocoder,
Rhythmic, Mod, FX. The instruments are approximations made from guitar — they follow your playing
(no MIDI), so they work best with clean, single-note or simple chord parts. All presets are levelled
to a similar loudness on a clean DI.

### Presets, A/B, randomize
User presets are saved to `Documents/Synthia/Presets` (`*.synthia`, plain XML). **A/B** keeps two
versions of the sound; **A→B** copies. **Randomize** (top bar) changes the whole sound, the **Rnd**
buttons only one section; **Chaos** sets how far it moves from the current sound.

### Tips for low tunings
- Keep **HPF** below your lowest note (G#1 ≈ 52 Hz, F#1 ≈ 46 Hz).
- Voicer: **Low** 40–45 Hz, **Sharpness** ≈ 50 %, **Tilt** slightly negative; for tight djent
  layers lower **Smooth** and use **Pick ADSR**.
- Use the Dry layer (not routed to the filter) under synth layers for punch.

## Building (developers)
See `CLAUDE.md` for build commands, architecture and conventions.
