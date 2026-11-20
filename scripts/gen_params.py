#!/usr/bin/env python3
"""Generates src/params/ParamList.h (the X-macro parameter table).

RULES (see CLAUDE.md): parameter IDs are permanent once released. New
parameters are APPENDED to the end of PARAMS (the modulation-destination list
is derived from this order and stored by index). Never reorder or remove.
Run:  python3 scripts/gen_params.py  (then commit the regenerated header)
"""
import os

# kind: F float, I stepped integer (float param with interval 1), B bool, C choice
# curve: Lin, Log, Sq (quadratic, for ranges starting at 0)
P = []
def F(id, name, mn, mx, d, curve, unit, sec, tip, mod=1): P.append((id, name, 'F', mn, mx, d, curve, unit, sec, mod, 'none', tip))
def I(id, name, mn, mx, d, unit, sec, tip, mod=1):        P.append((id, name, 'I', mn, mx, d, 'Lin', unit, sec, mod, 'none', tip))
def B(id, name, d, sec, tip):                              P.append((id, name, 'B', 0, 1, d, 'Lin', '', sec, 0, 'none', tip))
def C(id, name, choices, d, sec, tip):                     P.append((id, name, 'C', 0, 0, d, 'Lin', '', sec, 0, choices, tip))

# ---------------------------------------------------------------- global
F('inGain',  'Input Gain', -24, 24, 0, 'Lin', 'dB', 'global', 'Input gain before everything else. Aim for healthy peaks on the input meter.', 0)
F('outGain', 'Output Gain', -24, 24, 0, 'Lin', 'dB', 'global', 'Final output level.', 0)
F('mix',     'Global Mix', 0, 1, 1, 'Lin', '%', 'global', 'Global dry/wet: 0 % = untouched input, 100 % = only Synthia.')
F('outWidth','Output Width', 0, 2, 1, 'Lin', '%', 'global', 'Stereo width of the processed signal (0 = mono, 100 % = as is, 200 % = extra wide).')
C('quality', 'Quality', 'quality', 1, 'global', 'Oversampling for the nonlinear Voicer stages: Eco = off, Normal = 2x, High = 4x.')
B('bypass',  'Bypass', 0, 'global', 'Bypasses Synthia (click-free crossfade).')
B('alignSpec','Align Spectral', 0, 'global', 'Off: zero latency, the Spectral layer lags behind (fine for pads). On: reports the STFT latency and delays all other paths so everything lines up when printing.')

# ---------------------------------------------------------------- input stage
F('hpf',     'HPF', 20, 200, 30, 'Log', 'Hz', 'input', 'High-pass filter on the input. Keep it below your lowest string (G#1 = 52 Hz).')
B('gateOn',  'Gate', 1, 'input', 'Noise gate on the input (removes hum/hiss between notes).')
F('gateThr', 'Gate Threshold', -90, -20, -62, 'Lin', 'dB', 'input', 'Gate opens above this level.')
F('gateRel', 'Gate Release', 5, 1000, 120, 'Log', 'ms', 'input', 'How quickly the gate closes after the signal drops.')
F('envAtk',  'Env Attack', 0.1, 100, 5, 'Log', 'ms', 'input', 'Attack of the envelope follower (modulation source "Env Follower").')
F('envRel',  'Env Release', 5, 2000, 150, 'Log', 'ms', 'input', 'Release of the envelope follower.')
F('pickSens','Pick Sensitivity', 0, 1, 0.6, 'Lin', '%', 'input', 'Sensitivity of the pick/onset detector (no pitch involved). Higher = detects softer picks.')
F('pickHold','Pick Retrigger', 10, 500, 60, 'Log', 'ms', 'input', 'Minimum time between two detected picks.')
F('pickDecay','Pick Env Decay', 20, 2000, 300, 'Log', 'ms', 'input', 'Decay time of the pick envelope (modulation source "Pick Env").')

# ---------------------------------------------------------------- mixer (per source)
for s, nm, on, lvl in [('vc','Voicer',1,0.8),('sp','Spectral',0,0.7),('rs','Resonator',0,0.7),('gr','Granular',0,0.7),('dry','Dry',1,0.0)]:
    B(s+'On', nm+' On', on, 'mix', f'Switches the {nm} layer on/off. Off layers cost no CPU.')
    F(s+'Level', nm+' Level', 0, 1, lvl, 'Sq', '%', 'mix', f'Level of the {nm} layer in the mixer.')
    F(s+'Pan', nm+' Pan', -1, 1, 0, 'Lin', '', 'mix', f'Pan/balance of the {nm} layer.')
    B(s+'Mute', nm+' Mute', 0, 'mix', f'Mutes the {nm} layer.')
    B(s+'Solo', nm+' Solo', 0, 'mix', f'Solos the {nm} layer (several solos can be on).')
    B(s+'ToFlt', nm+' to Filter', 1 if s != 'dry' else 0, 'mix', f'Routes the {nm} layer through the main filter and amp shaper. Off = it bypasses them.')

# ---------------------------------------------------------------- A voicer
C('vcBands', 'Bands', 'vcBands', 2, 'voicer', 'Number of analysis/synthesis bands. More = finer, smoother chords, more CPU.')
F('vcLow',   'Low', 40, 200, 45, 'Log', 'Hz', 'voicer', 'Lowest band frequency. ~45 Hz covers low 7/8-string notes.')
F('vcHigh',  'High', 2000, 8000, 5000, 'Log', 'Hz', 'voicer', 'Highest band frequency.')
F('vcSharp', 'Sharpness', 0, 1, 0.5, 'Lin', '%', 'voicer', 'Band Q. Higher = each band locks onto a single partial (cleaner, slower attack in the lows); lower = faster, buzzier.')
F('vcWave',  'Wave', 0, 3, 1, 'Lin', '', 'voicer', 'Waveform morph: sine -> saw -> square -> pulse.')
F('vcPw',    'Pulse Width', 0.05, 0.5, 0.25, 'Lin', '%', 'voicer', 'Pulse width used at the pulse end of the Wave morph.')
F('vcMain',  'Main', 0, 1, 1, 'Lin', '%', 'voicer', 'Level of the main voice (same pitch as the guitar).')
F('vcSub',   'Sub', 0, 1, 0, 'Lin', '%', 'voicer', 'Sub voice one octave down (half phase rate per band).')
F('vcOct',   'Octave Up', 0, 1, 0, 'Lin', '%', 'voicer', 'Voice one octave up (double phase rate).')
F('vcInt',   'Interval', 0, 1, 0, 'Lin', '%', 'voicer', 'Level of the interval voice.')
C('vcIntRatio', 'Interval Ratio', 'vcInterval', 4, 'voicer', 'Interval of the interval voice in semitones.')
F('vcDetune','Detune', 0, 50, 6, 'Lin', 'ct', 'voicer', 'Per-band slow pitch drift in cents: unison-like movement.')
F('vcFocus', 'Focus', 0, 1, 0.5, 'Lin', '%', 'voicer', 'Lateral inhibition across neighbouring bands: emphasises spectral peaks, reduces chord warble.')
F('vcTilt',  'Tilt', -1, 1, -0.2, 'Lin', '', 'voicer', 'Negative = favour low bands (fundamentals, rounder), positive = high bands (brighter, buzzier).')
F('vcSmooth','Smooth', 0.5, 100, 6, 'Log', 'ms', 'voicer', 'Smoothing of the per-band amplitude envelopes.')
F('vcDrive', 'Drive', 0, 36, 0, 'Lin', 'dB', 'voicer', 'Saturation (oversampled per Quality).')
F('vcFold',  'Fold', 0, 1, 0, 'Lin', '%', 'voicer', 'Wavefolder amount (oversampled per Quality).')
F('vcRing',  'Ring', 0, 1, 0, 'Lin', '%', 'voicer', 'Ring-modulates the output with the sub-octave signal.')
F('vcBits',  'Crush Bits', 4, 16, 16, 'Lin', 'bit', 'voicer', 'Bit depth reduction (16 = off).')
F('vcRate',  'Crush Rate', 0, 1, 0, 'Lin', '%', 'voicer', 'Sample-rate reduction (0 = off).')
F('vcSpread','Stereo Spread', 0, 1, 0.4, 'Lin', '%', 'voicer', 'Scatters the bands across the stereo field.')

# ---------------------------------------------------------------- B spectral
C('spSize',  'FFT Size', 'spSize', 1, 'spectral', 'STFT size: smaller = faster/more transient, larger = smoother pads. Scaled up automatically at 88.2 kHz and above.')
B('spFreeze','Freeze', 0, 'spectral', 'Captures the current spectrum and holds it (crossfaded).')
F('spFrzBlend','Freeze Blend', 0, 1, 1, 'Lin', '%', 'spectral', 'Frozen vs live spectrum while Freeze is on.')
F('spDrift', 'Freeze Drift', 0, 1, 0.3, 'Lin', '%', 'spectral', 'Random phase drift of the frozen spectrum (keeps it alive, not buzzy).')
F('spBlur',  'Blur', 0, 1, 0.5, 'Lin', '%', 'spectral', 'Temporal smoothing of every bin: notes melt into each other.')
I('spPartials','Partials', 0, 64, 0, '', 'spectral', 'Keeps only the N strongest spectral peaks (0 = off): glassy, organ-like.')
F('spShift', 'Shift', -1000, 1000, 0, 'Lin', 'Hz', 'spectral', 'Linear frequency shift: inharmonic, metallic.')
F('spStretch','Stretch', 0.5, 2, 1, 'Log', 'x', 'spectral', 'Scales all frequencies (harmonic stretch/compress).')
C('spMode',  'Mode', 'spMode', 0, 'spectral', 'Normal / Robot (zero phase) / Whisper (random phase).')

# ---------------------------------------------------------------- C resonator / vocoder / formant
C('rsMode',  'Mode', 'rsMode', 0, 'reso', 'Resonator: tuned resonators excited by the guitar. Vocoder: guitar modulates an internal chord. Formant: vowel filter.')
I('rsSlot',  'Chord Slot', 0, 3, 0, '', 'reso', 'Active chord slot (1-4). Automatable and modulatable, e.g. by the step sequencer.')
for n in range(1, 5):
    C(f'c{n}Root', f'Chord {n} Root', 'root', [9, 4, 7, 2][n-1], 'reso', f'Root note of chord slot {n}.')
    C(f'c{n}Type', f'Chord {n} Type', 'chordType', [0, 2, 5, 10][n-1], 'reso', f'Chord type of slot {n}.')
    I(f'c{n}Oct',  f'Chord {n} Octave', 0, 6, 2, '', 'reso', f'Octave of slot {n} (2 = C2...B2).')
    F(f'c{n}Spread', f'Chord {n} Spread', 0, 1, 0.3, 'Lin', '%', 'reso', f'Voicing spread of slot {n}: 0 = close, 100 % = wide open.')
C('rsMat',   'Material', 'rsMaterial', 0, 'reso', 'Resonator material: String (comb), Metal / Glass (modal), Bowed (string + bow noise shaped by your playing).')
F('rsDecay', 'Decay', 0.05, 20, 2, 'Log', 's', 'reso', 'Resonator decay time.')
F('rsBright','Brightness', 0, 1, 0.6, 'Lin', '%', 'reso', 'Brightness / damping of the resonators.')
F('rsInharm','Inharmonicity', 0, 1, 0.1, 'Lin', '%', 'reso', 'Detunes the overtones (stiff strings, bells).')
F('rsExcite','Excite', -24, 12, 0, 'Lin', 'dB', 'reso', 'How hard the guitar excites the resonators.')
C('vcoBands','Vocoder Bands', 'vcoBands', 2, 'reso', 'Number of vocoder bands.')
C('vcoWave', 'Carrier Wave', 'vcoWave', 2, 'reso', 'Carrier oscillator: saw, square or supersaw, playing the active chord slot.')
F('vcoDetune','Carrier Detune', 0, 1, 0.3, 'Lin', '%', 'reso', 'Detune of the carrier (supersaw spread).')
F('vcoNoise','Carrier Noise', 0, 1, 0.2, 'Lin', '%', 'reso', 'Noise in the carrier on pick attacks (makes transients audible).')
F('vcoFormant','Formant Shift', -12, 12, 0, 'Lin', 'st', 'reso', 'Shifts the vocoder carrier bands against the guitar bands.')
F('vcoRelease','Vocoder Release', 5, 500, 60, 'Log', 'ms', 'reso', 'Release of the vocoder band envelopes.')
F('fmVowel', 'Vowel', 0, 4, 0, 'Lin', '', 'reso', 'Continuous vowel morph A - E - I - O - U.')
F('fmReso',  'Formant Resonance', 0, 1, 0.5, 'Lin', '%', 'reso', 'Resonance (Q) of the formant filters.')
F('fmShift', 'Gender', -12, 12, 0, 'Lin', 'st', 'reso', 'Shifts all formants (negative = larger/"male", positive = smaller/"female").')

# ---------------------------------------------------------------- D granular
F('grSize',  'Grain Size', 10, 500, 120, 'Log', 'ms', 'gran', 'Length of each grain.')
F('grDensity','Density', 1, 100, 12, 'Log', '/s', 'gran', 'Grains per second.')
F('grPos',   'Position', 0, 4000, 200, 'Sq', 'ms', 'gran', 'How far back in the 8 s buffer the grains start.')
F('grSpray', 'Spray', 0, 1000, 80, 'Sq', 'ms', 'gran', 'Random jitter of the start position.')
F('grPitch', 'Pitch', -24, 24, 0, 'Lin', 'st', 'gran', 'Grain pitch in semitones.')
C('grQuant', 'Pitch Quantise', 'grQuant', 0, 'gran', 'Quantises pitch + jitter to octaves / fifths / semitones (octaves = shimmer).')
F('grPJit',  'Pitch Jitter', 0, 12, 0, 'Lin', 'st', 'gran', 'Random pitch deviation per grain.')
F('grPanJit','Pan Jitter', 0, 1, 0.5, 'Lin', '%', 'gran', 'Random pan per grain.')
F('grRev',   'Reverse', 0, 1, 0, 'Lin', '%', 'gran', 'Probability that a grain plays backwards.')
C('grWindow','Window', 'grWindow', 0, 'gran', 'Grain envelope shape.')
B('grFreeze','Buffer Freeze', 0, 'gran', 'Stops recording: grains keep playing the frozen buffer.')
B('grStutter','Stutter', 0, 'gran', 'Host-synced stutter: repeats a short slice (replaces the grain cloud while on).')
C('grStutDiv','Stutter Slice', 'stutDiv', 6, 'gran', 'Length of the repeated slice.')
C('grStutHold','Stutter Retrigger', 'stutHold', 0, 'gran', 'How often a new slice is captured.')
F('grFb',    'Feedback', 0, 0.95, 0, 'Lin', '%', 'gran', 'Feeds the grains back into the buffer (safely limited).')

# ---------------------------------------------------------------- E dry
F('dryTilt', 'Dry Tilt', -1, 1, 0, 'Lin', '', 'dry', 'Simple tone tilt for the dry layer (dark <-> bright).')

# ---------------------------------------------------------------- main filter
B('fltOn',   'Filter On', 1, 'filter', 'Switches the main filter on/off.')
C('fltType', 'Filter Type', 'fltType', 0, 'filter', 'Ladder LP 24 dB, SVF 12 dB types, comb or formant.')
F('fltCut',  'Cutoff', 20, 20000, 12000, 'Log', 'Hz', 'filter', 'Cutoff frequency (Formant type: vowel position).')
F('fltRes',  'Resonance', 0, 1, 0.15, 'Lin', '%', 'filter', 'Resonance. Self-oscillation is possible but tamed.')
F('fltDrive','Filter Drive', 0, 24, 0, 'Lin', 'dB', 'filter', 'Drive into the filter (anti-aliased).')
F('fltEnv',  'Env Amount', -1, 1, 0, 'Lin', '', 'filter', 'Envelope follower -> cutoff (+-4 octaves).')
F('fltPick', 'Pick Amount', -1, 1, 0, 'Lin', '', 'filter', 'Pick envelope -> cutoff (+-4 octaves). Classic synth "pluck" filter.')

# ---------------------------------------------------------------- amp shaper
F('adsrAmt', 'Pick ADSR', 0, 1, 0, 'Lin', '%', 'amp', 'How much the pick-triggered ADSR shapes the volume of the routed layers.')
F('adsrA',   'Attack', 0.5, 2000, 5, 'Log', 'ms', 'amp', 'ADSR attack.')
F('adsrD',   'Decay', 5, 5000, 300, 'Log', 'ms', 'amp', 'ADSR decay.')
F('adsrS',   'Sustain', 0, 1, 0.6, 'Lin', '%', 'amp', 'ADSR sustain level (held while you let the note ring).')
F('adsrR',   'Release', 5, 5000, 250, 'Log', 'ms', 'amp', 'ADSR release (starts when the string fades).')
B('swellOn', 'Swell', 0, 'amp', 'Automatic volume swell on every pick.')
F('swellTime','Swell Time', 10, 3000, 400, 'Log', 'ms', 'amp', 'Swell attack time.')
B('chopOn',  'Chopper', 0, 'amp', 'Tempo-synced 16-step gate.')
C('chopDiv', 'Chopper Rate', 'syncDiv', 16, 'amp', 'Length of one chopper step.')
F('chopDepth','Chopper Depth', 0, 1, 1, 'Lin', '%', 'amp', 'Depth of the chopper gate.')
F('chopSmooth','Chopper Smooth', 0.5, 50, 3, 'Log', 'ms', 'amp', 'Edge smoothing of the chopper.')
for n in range(1, 17):
    B(f'chop{n}', f'Chop Step {n}', 1 if n % 2 == 1 else 0, 'amp', f'Chopper step {n} open/closed.')

# ---------------------------------------------------------------- delay
B('dlyOn',   'Delay On', 0, 'delay', 'Switches the delay on/off.')
B('dlySync', 'Delay Sync', 1, 'delay', 'Delay time synced to the host tempo.')
F('dlyTime', 'Delay Time', 1, 2000, 375, 'Log', 'ms', 'delay', 'Delay time in ms (Sync off).')
C('dlyDiv',  'Delay Note', 'syncDiv', 13, 'delay', 'Delay time as a note value (Sync on).')
F('dlyFb',   'Delay Feedback', 0, 1, 0.35, 'Lin', '%', 'delay', 'Feedback (limited, stays stable even at 100 %).')
F('dlyMix',  'Delay Mix', 0, 1, 0.25, 'Lin', '%', 'delay', 'Delay dry/wet.')
B('dlyPing', 'Ping-Pong', 0, 'delay', 'Repeats bounce between left and right.')
F('dlyLow',  'Delay Low Cut', 20, 2000, 120, 'Log', 'Hz', 'delay', 'Low cut in the feedback path.')
F('dlyHigh', 'Delay High Cut', 1000, 20000, 7000, 'Log', 'Hz', 'delay', 'High cut in the feedback path.')
F('dlyModRate','Delay Mod Rate', 0.05, 8, 0.6, 'Log', 'Hz', 'delay', 'Tape wobble rate.')
F('dlyModDepth','Delay Mod Depth', 0, 1, 0.15, 'Lin', '%', 'delay', 'Tape wobble depth.')
F('dlyWidth','Delay Width', 0, 1, 1, 'Lin', '%', 'delay', 'Stereo width of the repeats.')

# ---------------------------------------------------------------- reverb
B('rvOn',    'Reverb On', 1, 'reverb', 'Switches the reverb on/off.')
C('rvMode',  'Reverb Mode', 'rvMode', 0, 'reverb', 'Cosmos: huge modulated hall. Shimmer: pitch shifters in the loop. Abyss: downward shimmer. Bloom: slowly swelling build-up. Cloud: granular diffusion in the tank.')
F('rvSize',  'Size', 0, 1, 0.7, 'Lin', '%', 'reverb', 'Room size.')
F('rvDecay', 'Decay', 0.5, 60, 6, 'Log', 's', 'reverb', 'Decay time (RT60).')
B('rvInf',   'Infinite', 0, 'reverb', 'Infinite decay / freeze: the tank holds what is in it, new input is muted.')
B('rvPreSync','Pre-delay Sync', 0, 'reverb', 'Pre-delay synced to the host tempo.')
F('rvPre',   'Pre-delay', 0, 500, 20, 'Sq', 'ms', 'reverb', 'Pre-delay in ms (Sync off).')
C('rvPreDiv','Pre-delay Note', 'syncDiv', 16, 'reverb', 'Pre-delay as a note value (Sync on).')
F('rvDiff',  'Diffusion', 0, 1, 0.75, 'Lin', '%', 'reverb', 'Input diffusion (smears the attack).')
F('rvDampLo','Low Damping', 20, 1000, 80, 'Log', 'Hz', 'reverb', 'Low frequencies below this decay faster.')
F('rvDampHi','High Damping', 1000, 20000, 7000, 'Log', 'Hz', 'reverb', 'High frequencies above this decay faster.')
F('rvModDepth','Reverb Mod Depth', 0, 1, 0.4, 'Lin', '%', 'reverb', 'Modulation depth of the tank (chorus-like movement).')
F('rvModRate','Reverb Mod Rate', 0.05, 5, 0.4, 'Log', 'Hz', 'reverb', 'Modulation rate of the tank.')
F('rvShim',  'Shimmer', 0, 1, 0.5, 'Lin', '%', 'reverb', 'Amount of pitch-shifted feedback (Shimmer/Abyss modes; also usable in the others).')
C('rvShimInt','Shimmer Interval', 'shimInterval', 0, 'reverb', 'Interval of the shimmer pitch shifter (Abyss always shifts down).')
F('rvBloom', 'Bloom Time', 0.1, 4, 1, 'Log', 's', 'reverb', 'Build-up time in Bloom mode.')
F('rvDuck',  'Ducking', 0, 1, 0, 'Lin', '%', 'reverb', 'Ducks the reverb while you play.')
F('rvWidth', 'Reverb Width', 0, 1, 1, 'Lin', '%', 'reverb', 'Stereo width of the reverb.')
F('rvMix',   'Reverb Mix', 0, 1, 0.3, 'Lin', '%', 'reverb', 'Reverb dry/wet.')

# ---------------------------------------------------------------- modulation sources
for n in (1, 2):
    C(f'lfo{n}Shape', f'LFO {n} Shape', 'lfoShape', 0, 'mod', f'LFO {n} waveform.')
    B(f'lfo{n}Sync', f'LFO {n} Sync', 0, 'mod', f'LFO {n} synced to the host tempo.')
    F(f'lfo{n}Rate', f'LFO {n} Rate', 0.01, 20, [0.5, 2.0][n-1], 'Log', 'Hz', 'mod', f'LFO {n} rate (Sync off).')
    C(f'lfo{n}Div', f'LFO {n} Note', 'syncDiv', [2, 8][n-1], 'mod', f'LFO {n} cycle length (Sync on).')
    F(f'lfo{n}Phase', f'LFO {n} Phase', 0, 1, 0, 'Lin', '', 'mod', f'LFO {n} start phase.')
    B(f'lfo{n}Retrig', f'LFO {n} Retrigger', 0, 'mod', f'Restarts LFO {n} on every pick.')
C('rndShape','Random Shape', 'rndShape', 1, 'mod', 'Random source: stepped (S&H) or smooth.')
B('rndSync', 'Random Sync', 0, 'mod', 'Random source synced to the host tempo.')
F('rndRate', 'Random Rate', 0.05, 30, 2, 'Log', 'Hz', 'mod', 'Random rate (Sync off).')
C('rndDiv',  'Random Note', 'syncDiv', 12, 'mod', 'Random step length (Sync on).')
I('seqSteps','Seq Steps', 1, 32, 16, '', 'mod', 'Number of steps. Independent of the bar -> polymeter (e.g. 7 or 13 steps against 4/4).', 0)
C('seqDiv',  'Seq Rate', 'syncDiv', 16, 'mod', 'Length of one step (incl. triplets and quintuplets).')
B('seqSync', 'Seq Sync', 1, 'mod', 'Step sequencer follows the host position (off = free running at the host tempo).')
for n in range(1, 33):
    F(f'seqV{n}', f'Seq Step {n}', -1, 1, 0, 'Lin', '', 'mod', f'Value of step {n}.', 0)
for n in range(1, 33):
    B(f'seqG{n}', f'Seq Glide {n}', 0, 'mod', f'Glide into step {n}.')
for n in range(1, 5):
    F(f'macro{n}', f'Macro {n}', 0, 1, 0, 'Lin', '%', 'mod', f'Macro {n}: a knob you can route anywhere in the matrix.', 0)
for n in range(1, 17):
    C(f'm{n}Src', f'Mod {n} Source', 'modSource', 0, 'matrix', f'Matrix slot {n}: modulation source.')
    C(f'm{n}Dst', f'Mod {n} Destination', 'modDest', 0, 'matrix', f'Matrix slot {n}: destination parameter.')
    F(f'm{n}Amt', f'Mod {n} Amount', -1, 1, 0, 'Lin', '', 'matrix', f'Matrix slot {n}: bipolar amount.', 0)
    C(f'm{n}Via', f'Mod {n} Via', 'modSource', 0, 'matrix', f'Matrix slot {n}: optional scaler (e.g. a macro controls the amount).')

# ---------------------------------------------------------------- APPEND NEW PARAMETERS BELOW THIS LINE ONLY

def esc(s): return s.replace('\\', '\\\\').replace('"', '\\"')
ids = [p[0] for p in P]
assert len(ids) == len(set(ids)), 'duplicate id'
out = ['// GENERATED by scripts/gen_params.py -- do not edit by hand.',
       '// X (id, name, kind, min, max, def, curve, unit, section, modulatable, choices, tooltip)',
       '#pragma once', '', '#define SYNTHIA_PARAMS(X) \\']
for (id, name, kind, mn, mx, d, curve, unit, sec, mod, ch, tip) in P:
    out.append(f'    X ({id}, "{esc(name)}", {kind}, {float(mn)}f, {float(mx)}f, {float(d)}f, {curve}, "{unit}", {sec}, {mod}, {ch}, "{esc(tip)}") \\')
out.append('')
here = os.path.dirname(os.path.abspath(__file__))
open(os.path.join(here, '..', 'src', 'params', 'ParamList.h'), 'w').write('\n'.join(out) + '\n')
print(len(P), 'parameters')
