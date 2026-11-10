<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS atmospheric design audit and low-risk expansion
Owner-requested lab, 2026-11-10 · issue #530 · **proposal, not merged capability**

## Scope and compatibility baseline

Reviewed live `essentials.chaos_lite` v1 source/README/module.json, the 28-module
Studio catalogue, `apps/studio/patches` scenes, the merged eight-preset module
selector (#522), and the owner's original CHAOS v0.1 source archive (October 2026).
The scary-music study guide informs roughness, detuning, spectra, and sound mass;
**no throat/full-throat/singing/vocal formant scope**.

CHAOS v1 has 26 parameter IDs (slots 0–25), two saw→square→sine morph
oscillators, parallel / ring-AM / delay-bend routes, stereo three-comb body,
seeded drift and jitter, roughness (1–150 Hz), one sine-only self-modulated
LFO targeting A pitch/body tuning/cutoff, lowpass and output limiting. It has
no internal ADSR, random-selectable LFO shape, drive/fold, sub oscillator, or
looper. It handles up to 8 prepared poly voices, despite the host's 16-channel
maximum; audio above channel 8 is silent. Non-bandlimited upper saw/square
harmonics can alias.

**A real bug in the authored instrument patches:** CHAOS `gate` is multiplied
into output audio immediately. Patching keyboard gate to CHAOS *and* downstream
ADSR means note-off hard-cuts the ADSR release. In #530, the CHAOS gate input
is intentionally unpatched (default 1) while keyboard gate drives ADSR → twin
left/right VCAs. Leave CHAOS `fall=0` in these patches: without a gate edge,
its pitch fall cannot retrigger. If pitch-fall-on-note is needed later, add
an independent trigger port; do not change current gate semantics.

## What makes a good CHAOS patch here

A useful instrument patch is repeatable, plays notes clearly, and still has
depth when modulations are reduced. Start with **one audible focus** (warm pad,
lead attack, metallic event); use a **stable base sound**, 1–3 slow independent
movements, and only enough roughness to complement it. Modulation has to be
calibrated to *destination range*: a cable amount of .05 on a 20–20,000 Hz
cutoff adds nearly a 1 kHz sweep at full source, not 5% of the base frequency.
Add envelope release, usable stereo, conservative resonance/feedback, and an
explicit output path. Keep the same musical action useful at low modulation
depth and upper extreme without saturating into the same timbre.

The five tools in the attached guide are aesthetic **options**, not a preset
checklist: (a) roughness and deterministic instability are available, (b) slow
detune and slides are available, (c) ametric drift is effective for atmospheric
pads, (d) sub energy must be produced in the audible/monitorable range and
checked on speakers, (e) human-voice resemblance is deliberately out of scope.
The most compelling CHAOS identity is a *shifting resonant instrument*, not an
always-on maximum-destruction patch.

The merged eight-module preset menu changes 26 values at once. That is useful
for **local sound starting points**, whereas a **library patch** demonstrates
keyboard articulation, independent modulators, effects and stereo routing.
Do not duplicate the preset data into an unrelated patch format. For patch
authoring, first choose a compatible preset tone, then explicitly set all
important values in patch JSON; naming a preset does not persist it in the
patch.

Reference reading: Ableton Learning Synths
([envelopes](https://learningsynths.ableton.com/en/envelopes/synthesizer-envelopes),
[recipes](https://learningsynths.ableton.com/recipes/get-to-know-this-synth),
[random LFO](https://learningsynths.ableton.com/en/recipes/old-fashioned-computer))
and [Ableton Live 12 Instrument Reference](https://www.ableton.com/en/manual/live-instrument-reference/).
Their distinction between ADSR shaping, destination-specific amounts and
S&H/smooth LFO forms supports the choices, but supplies no Wirescape DSP proof.

## Curated patch results / expose controls next to every patch

| Patch | Purpose and existing controls to expose | Future controls (not implemented) |
|---|---|---|
| Basics 6 · CHAOS Keys (revised) | Core: `shape_a`, `shape_b`, `detune`; Body: `body_mix`, `feedback`; `roughness`; external ADSR attack/release | grouped front-panel ADSR shortcut, not a new internal envelope |
| Scene 3 · Black Ice Choir (revised) | Pad: `body_mix`, `body_tune`, `cutoff`, `route`, `depth`, `drift`; external ADSR attack/release | smooth-random internal LFO waveform |
| CHAOS Lab 1 · Ember Veil | Two warm oscillator shapes, `detune`, body mix/feedback, cutoff, drift; external slow random range/smooth + ADSR | single `lfo_shape` for smooth-random variation |
| CHAOS Lab 2 · Wireglass | `route`, `depth`, `shape_a/b`, `rough_rate`, `roughness`, body tune; internal LFO vibrato, ADSR, delay mix | per-note LFO reset or triggered pitch envelope only if genuinely required |
| CHAOS Lab 3 · Orbiting Iron | Clock BPM/duty, external function `rise`, `fall`, `curve`; `depth`, `body_mix`, `feedback`, `rough_rate`, delay/reverb | S&H or smooth-random LFO selector |

The new Lab 1/2/3 files have explicit output buses 0/1. Lab 1/2 are keyboard
instruments; Lab 3 autoplays and intentionally uses the existing function
envelope for non-ADSR percussive contours. Oscillator-gate behavior is not
changed. Reuse existing `example.random_source` (clocked step and smooth),
`essentials.lfo`, `essentials.adsr`, `essentials.function_envelope`,
`fx.delay` and `fx.reverb` before adding any native DSP feature.

### Original five CHAOS v0.1 demonstrations — honest rework map

| Original preset | Wirescape-capable equivalent | Not equivalent / don't fake |
|---|---|---|
| Clean to scream | morph from sine to saw/square, increase `roughness` / `rough_rate`, `jitter`, ring route | no nonlinear vocal chaos, subharmonic injection or frequency-jump algorithm |
| Shower strings | close high semitone notes and roughness around 70 Hz, body + narrow stereo | no string bow model; needs owner listening for alias/harshness |
| Cluster mass | play 4–8 independent notes/CHAOS instances with detuned poly keyboard and slow drift | 12×3 internal cluster voices and arbitrary microtonal note fanout not present |
| Sour detune | existing `detune` (-60…60 cents); vary register and beat rate | no separate internal unison voice bank; host chords and two oscillators only |
| Rotting loop | slow modulation, delay tone, reverb/freeze, spectral texture externally | no reverse/half-speed varispeed looper or tape-age DSP; `fx.delay` is not a looper |

## Suggested module panel design, after #522 preset selector

Do not add all parameters to a top-level front panel; existing grouped panel
data already has 01 Oscillators, 02 Balance, 03 Body, 04 Character,
05 Internal LFO + Out, 06 Routing. Preserve those IDs/controls.

- **Performance strip (UI-only, very low cost):** playable Tone
  (`shape_a/b`, `detune`), Motion (`drift`, `lfo_depth`, `lfo_rate`),
  Edge (`roughness`, `depth`), Body (`body_mix`, `feedback`),
  Brightness (`cutoff`). Display exact values. Do **not** implement a
  fan-out macro until the team defines save/undo/param precedence.
- **A/B + Coupling:** route labelled Parallel / Ring-AM / Delay bend;
  A/B oscillator morph/octal tuning, detune, pan and gain; coupling depth.
- **Body:** tune, feedback, damping, mix, with a small caution when feedback
  is high; no undocumented self-oscillation guarantee.
- **Instability/Mod:** drift, jitter, fall, roughness/rate, internal
  LFO shape (future), rate, depth, self, target; leave host modulation cables
  discoverable on continuous parameters.
- **Output:** cutoff and level. Separate external FX/envelopes from CHAOS's
  controls in the graph; patch-wide macro panel needs independent design.

## Incremental engineering for Codex Lead / Coda

**P0 (existing, no native work):** ship well-articulated patches, grouped
panel labels, named presets (#522), ADSR/VCA wiring, modulated parameter
cables, external S&H/smooth random. This is sufficient for the three lab
sounds and most of the requested atmospheres.

**P1 (small additive native + catalogue + UI; separate T1 PR):** append
`lfo_shape` as slot 26 in `native/modules2/chaos_lite/dsp.h`,
`module.json` and regenerated catalogue (range 0–3, integer:
0 Sine **existing default**, 1 Triangle, 2 Sample & Hold,
3 Smooth Random). Keep **all slots 0–25, parameter ranges, IDs,
and existing sine output bit-identical**. No reset to defaults for saved
patches. Use existing per-voice seeded RNG (prefer an *independent* LFO
stream to avoid affecting drift/jitter sequence) and a bounded per-cycle
sample-and-hold draw; smooth random interpolates adjacent targets
with e.g. cosine ease. Advance on absolute per-sample phase, not
block boundaries. Shape changes need a 5–20 ms output crossfade for
live clicks; default shape zero should retain old output exactly
when untouched. Preserve state v1 loading; write state v2 with
new RNG/held/target values, finite validation; do not change ABI.
If v1 loading cannot be kept deterministic/bit-identical, stop and
escalate to Keel before implementation. Add sample-rate
44.1/48/96 kHz, frame 1/17/64/128, seeded, state round-trip,
poly-voice isolation, extreme-rate and no-allocation tests.
No new UI dependencies. The 64-param host budget leaves room.

**P2 (optional, after P1 listening):** an *independent* function-envelope
routing convenience, or a second internal mod destination only if repeated
real patches cannot be expressed with existing parameter modulation
cables. Retain the existing hard-gate default; do not silently change
release behavior. A triggered one-shot fall on note-off needs an additive
trigger or a full internal ADSR and is not a free panel toggle.

**P3 (only after owner listening + Pi budget):** original-sounding
waveshaper/fold as its own module and/or follow the existing #491
primitive roadmap. Reuse standalone Resonator rather than duplicating
different structure modes in CHAOS. Do not add throat/singing,
Surge-sized oscillators, time-stretch/varispeed looper, or
full modulation matrix to the lite engine now.

### Review checklist (not a claim of completed tests)

For every new DSP parameter: catalogue and panel sync; no renaming,
shifting or repurposing IDs; old-patch sine audio identical; host modulation
range unchanged; state v1 compatibility; correct seed/frame determinism;
8-voice and 44.1/48/96 kHz checks; bounded `process`; finite output;
zero callback allocation; native+WASM CI; 2-instance Pi offline probe with
worst block < half of 128 frames at 48 kHz; device-off build.
T1 changes require a different-session reviewer. If tests require changing
existing thresholds, stop rather than loosening them.

**Prototype boundary:** a standalone Python/NumPy offline LFO waveform
proof can establish phase continuity, seeded random sequence, and
envelope/roughness modulation direction, but is **not** proof of the native
CHAOS audio sound, physical Pi performance or a compiled Wirescape module.
