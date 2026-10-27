<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M2 ABI 1 module port inventory

Static source research only. `UNKNOWN` means not established by reviewed files. No build, audio, or runtime test performed.

Migration target: retain Essentials identities and lossless v1→v2 patch contents (`docs/specs/ABI2_PLAN.md`, sections 0, 5 B4; `AGENTS.md`, Engineering rules).

## adsr

**1. Identity.** `essentials.adsr`; display name from `native/modules/adsr/module.json:1`; source `native/modules/adsr/module.cpp:94-115`, `native/modules/adsr/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `envelope` | CV | n/a | `native/modules/adsr/module.cpp:94-115`; `native/modules/adsr/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `attack` | attack | .0001 | 30 | .01 | s / plain values; no module-side taper | No integer rounding; continuous values | `native/modules/adsr/module.cpp:94-115`; `native/modules/adsr/module.json:1` |
| 1 | `decay` | decay | .0001 | 30 | .1 | s / plain values; no module-side taper | No integer rounding; continuous values | `native/modules/adsr/module.cpp:94-115`; `native/modules/adsr/module.json:1` |
| 2 | `sustain` | sustain | 0 | 1 | .7 | none / plain values; no module-side taper | No integer rounding; continuous values | `native/modules/adsr/module.cpp:94-115`; `native/modules/adsr/module.json:1` |
| 3 | `release` | release | .0001 | 30 | .2 | s / plain values; no module-side taper | No integer rounding; continuous values | `native/modules/adsr/module.cpp:94-115`; `native/modules/adsr/module.json:1` |

**4. State.** State has level/start/stage/timing, note ownership and gate state; reset clears envelope and sustain ramp (`native/modules/adsr/module.cpp:7-29`). No module-local save/load callback in descriptor (`native/modules/adsr/module.cpp:99-114`). Native-v1 inspection preserves the original JSON bytes and parsed object, but does not serialize DSP runtime fields (`native/document/document_reader.cpp:34-36,50-51,73-85`; `native/document/document_reader.h:22-35`).

**5. ABI 2 status.** Ported. `native/modules2/adsr/dsp.h:6-10`: existing parameter and envelope IDs/ranges/defaults retained; added gate and notes input ports (**added**, not an established ID break: ABI 1 has zero inputs). Notes ignored while gate patched and per-voice state changes (`native/modules2/adsr/dsp.h:11-38`; `native/modules/adsr/module.cpp:38-60`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** `process` has only fixed arithmetic and output writes over `frames`, with no allocation, lock, I/O or mutable global; `new` appears only in `create` (`native/modules/adsr/module.cpp:16-17,62-93`). No `native/modules/adsr/DESIGN.md` is present at this ref; third-party provenance/licence/revision are **UNKNOWN (looked in that path, `native/modules/adsr/module.cpp:1-5` and `native/modules/adsr/module.json:1`)**.

**Parameter interpretation / stepped:** Attack/Decay/Release are raw float seconds; integer sample duration is `llround(seconds * rate)` (not a stepped knob). Sustain is a smoothed float target via `LinearRamp` (`native/modules/adsr/module.cpp:32-48,65-85`).

**ABI 1 vs ABI 2 full interface (per-direction indices):**

| Element | ABI 1 (ID/index/range/default) | ABI 2 (ID/index/range/default) | Classification |
|---|---|---|---|
| input 0 gate | — | `gate` 0, gate signal (`native/modules2/adsr/dsp.h:7`) | added |
| input 1 notes | — | `notes` 1, note events (`native/modules2/adsr/dsp.h:7`) | added |
| output 0 envelope | `envelope` 0, control (`native/modules/adsr/module.cpp:94`) | `envelope` 0, CV unipolar (`native/modules2/adsr/dsp.h:8`) | identical ID/order; cosmetic hint |
| param 0 attack | `attack` 0, .0001..30, default .01 (`native/modules/adsr/module.json:1`; `native/modules/adsr/module.cpp:95`) | `attack` 0, .0001..30, default .01 (`native/modules2/adsr/dsp.h:10`) | identical |
| param 1 decay | `decay` 1, .0001..30, default .1 (`native/modules/adsr/module.json:1`; `native/modules/adsr/module.cpp:96`) | `decay` 1, .0001..30, default .1 (`native/modules2/adsr/dsp.h:10`) | identical |
| param 2 sustain | `sustain` 2, 0..1, default .7 (`native/modules/adsr/module.json:1`; `native/modules/adsr/module.cpp:97`) | `sustain` 2, 0..1, default .7 (`native/modules2/adsr/dsp.h:10`) | identical |
| param 3 release | `release` 3, .0001..30, default .2 (`native/modules/adsr/module.json:1`; `native/modules/adsr/module.cpp:98`) | `release` 3, .0001..30, default .2 (`native/modules2/adsr/dsp.h:10`) | identical |

**Behavior:** ABI 1 note/GATE handling uses source+note ownership (`native/modules/adsr/module.cpp:50-60`); ABI 2 adds Schmitt gate cable precedence over note events and 16 voices (`native/modules2/adsr/dsp.h:11-13,20-23,33-38`). Behavior changed; new port additions are **added**, not proven numeric ID breakage.

**Second-pass migration details.** ABI 1 has **no input buffers** (`input_count=0`); `process` ignores its input argument and writes the one envelope output, with triggering exclusively from `WS_NOTE_ON`, `WS_NOTE_OFF` and `WS_GATE` events (`native/modules/adsr/module.cpp:38-63,90-105`). Attack, decay and release parameter events assign continuous seconds without integer coercion; `llround(seconds * sample_rate)` quantizes *stage sample counts* only. Sustain is a continuous smoothed ramp target (`native/modules/adsr/module.cpp:32-48,65-85`). Note owner is last source/note, and release requires ownership; reset clears `gated`/stage but does **not explicitly clear stored source/note** (`native/modules/adsr/module.cpp:13-25,50-60`). `process` uses only the instance state and fixed per-frame arithmetic: **allocation/locks/I/O/process-wide mutable globals: no** (`native/modules/adsr/module.cpp:7-17,62-93`). ABI 2's extra inputs must be mapped as additions, with gate connection taking precedence over notes; port IDs are strings in native v1 cables (`native/modules2/adsr/dsp.h:7,20-24,33-38`; `native/document/document_reader.cpp:86-92`).

## clock

**1. Identity.** `essentials.clock`; display name from `native/modules/clock/module.json:1`; source `native/modules/clock/module.cpp:67-73`, `native/modules/clock/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `reset` | CV | null/unpatched reset = low; no reset edge (`native/modules/clock/module.cpp:54-58`) | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |
| output 0 | `clock` | CV | n/a | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `bpm` | bpm | 20 | 300 | 120 | BPM / plain units; no DSP-side taper | No integer cast (`native/modules/clock/module.cpp:43-47`) | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |
| 1 | `ticks_per_beat` | ticks per beat | 1 | 24 | 1 | none / plain units; no DSP-side taper | Yes, nearest integer (`native/modules/clock/module.cpp:45`) | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |
| 2 | `duty` | duty | .01 | .99 | .5 | none / plain units; no DSP-side taper | No integer cast (`native/modules/clock/module.cpp:43-47`) | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |
| 3 | `run` | run | 0 | 1 | 1 | none / plain units; no DSP-side taper | Thresholded at 0.5 (`native/modules/clock/module.cpp:60`) | `native/modules/clock/module.cpp:67-73`; `native/modules/clock/module.json:1` |

**4. State.** Per-instance phase, reset-edge flag, tempo ramp and ticks/duty/run (`native/modules/clock/module.cpp:21-39`); no module save callback (`native/modules/clock/module.cpp:70-72`). Patch inspect retains JSON bytes/object (`native/document/document_reader.cpp:34-36,50-51`).

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/clock/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** BPM ramps, ticks rounded, duty direct, run thresholded; output high window and sample-rate phase from (`native/modules/clock/module.cpp:40-65`). Process has bounded loop, no allocation, locking, I/O or mutable globals (`native/modules/clock/module.cpp:51-66`); only create allocates (`native/modules/clock/module.cpp:27-28`). `DESIGN.md:9,13` describes fixed instance state, original DSP, no external third-party DSP code or pinned revision; sample rate 1..384000 Hz (`native/modules/clock/module.cpp:11-15`).

**Second-pass migration details.** The reset input is read as `in ? in[0] : nullptr`; null, nonfinite, or below-0.5 samples count as low, and only a **rising** reset sets `phase=0` (`native/modules/clock/module.cpp:51-58`). BPM is a continuous ramp target, Ticks per beat is rounded with `floor(value + 0.5)` after range validation, Duty remains continuous, and Run is retained as a float but interpreted as a switch at `>=0.5` (`native/modules/clock/module.cpp:40-49,59-64`). When stopped the output is zero and phase does not advance (`native/modules/clock/module.cpp:59-65`). `process` uses private state, finite checks and a bounded sample loop; **no allocation, locks, I/O or process-wide mutable globals** (`native/modules/clock/module.cpp:16-25,51-66`). `DESIGN.md` explicitly says original DSP, **no external third-party DSP, no external pinned revision**, with inherited GPLv3 `LinearRamp` contract (`native/modules/clock/DESIGN.md:9-13`).

## function_envelope

**1. Identity.** `essentials.function_envelope`; display name from `native/modules/function_envelope/module.json:1`; source `native/modules/function_envelope/module.cpp:74-80`, `native/modules/function_envelope/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `gate` | gate | null/unpatched gate = low; falling segment begins if previously high (`native/modules/function_envelope/module.cpp:57-71`) | `native/modules/function_envelope/module.cpp:74-80`; `native/modules/function_envelope/module.json:1` |
| output 0 | `envelope` | CV | n/a | `native/modules/function_envelope/module.cpp:74-80`; `native/modules/function_envelope/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `rise` | rise | .0001 | 30 | .01 | s / plain units, curve shapes envelope (not a UI taper) | No parameter rounding (`native/modules/function_envelope/module.cpp:40-52`) | `native/modules/function_envelope/module.cpp:74-80`; `native/modules/function_envelope/module.json:1` |
| 1 | `fall` | fall | .0001 | 30 | .1 | s / plain units, curve shapes envelope (not a UI taper) | No parameter rounding (`native/modules/function_envelope/module.cpp:40-52`) | `native/modules/function_envelope/module.cpp:74-80`; `native/modules/function_envelope/module.json:1` |
| 2 | `curve` | curve | -1 | 1 | 0 | none / plain units, curve shapes envelope (not a UI taper) | No parameter rounding (`native/modules/function_envelope/module.cpp:40-52`) | `native/modules/function_envelope/module.cpp:74-80`; `native/modules/function_envelope/module.json:1` |

**4. State.** Per-instance level/start/endpoint, high flag, elapsed/length and latched curve; reset zeroes segment (`native/modules/function_envelope/module.cpp:21-36,47-52`). ABI1 has no module-local state save callback (`native/modules/function_envelope/module.cpp:77-79`); native document inspection retains raw JSON only (`native/document/document_reader.cpp:34-36,82-85`).

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/function_envelope/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Rise/fall are seconds converted with `llround(time * sample_rate)` into a discrete *duration*, but parameter values remain continuous; Curve is a continuous `expm1(4*Curve*u)/expm1(4*Curve)` shape, not log parameter taper (`native/modules/function_envelope/module.cpp:40-52,61-71`). Bounded per-frame work; no allocation, locks, I/O or globals inside `process` (`native/modules/function_envelope/module.cpp:54-73`), allocation only in create (`native/modules/function_envelope/module.cpp:28-29`). Sample rate accepted 1..384000 Hz (`native/modules/function_envelope/module.cpp:11-15`). `native/modules/function_envelope/DESIGN.md:3-13`: original gated AR DSP with no external third-party code/pinned revision, fixed instance state.

**Second-pass migration details.** `gate = in ? in[0] : nullptr`; null/nonfinite gate samples count as low, with a falling segment from the current level on a high-to-low transition, not an immediate reset (`native/modules/function_envelope/module.cpp:54-71`). Rise/Fall are continuous seconds; `llround(seconds*sample_rate)` rounds the *segment duration*, not the parameter. Curve remains a continuous signed value latched on a gate edge, used in `expm1(4*curve*u)/expm1(4*curve)` to shape the *output*; no parameter taper or stepped coercion (`native/modules/function_envelope/module.cpp:37-52,61-69`). **No allocation, locks, I/O or process-wide mutable globals** within `process` (`native/modules/function_envelope/module.cpp:21-28,54-73`). `DESIGN.md` identifies original DSP, no third-party implementation/pin, and inherited GPLv3 ramp contract (`native/modules/function_envelope/DESIGN.md:5,9-13`).

## ladder

**1. Identity.** `essentials.ladder`; display name from `native/modules/ladder/module.json:1`; source `native/modules/ladder/module.cpp:85-91`, `native/modules/ladder/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | null `in` or `in[0]` is treated as 0.0; nonfinite samples also become 0.0 (`native/modules/ladder/module.cpp:66-71`) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `cutoff` | cutoff | 20 | 20000 | 1000 | Hz / plain Hz ramp, nonlinear DSP tan(pi*f/fs) (`native/modules/ladder/module.cpp:62,72`; `native/modules/ladder/module.cpp:33-36`) | No, continuous float accepted and ramped (`native/modules/ladder/module.cpp:59-65,70-79`) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| 1 | `resonance` | resonance | 0 | 1 | 0 | none / plain linear ramp; nonlinear feedback coefficient (`native/modules/ladder/module.cpp:63,73`) | No, continuous float accepted and ramped (`native/modules/ladder/module.cpp:59-65,70-79`) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| 2 | `drive` | drive | 0 | 2 | 0 | none / plain linear ramp; nonlinear tanh when nonzero (`native/modules/ladder/module.cpp:64,74-79`) | No, continuous float accepted and ramped (`native/modules/ladder/module.cpp:59-65,70-79`) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |

**4. State.** Per-instance 4 double pole delays, previous output, sample rate, max frames and 3 smoothing ramps; prepare and reset clear poles and ramps (`native/modules/ladder/module.cpp:38-58`). No module-specific save/load callback in ABI 1 descriptor (`native/modules/ladder/module.cpp:88-90`). `inspect()` keeps original bytes/object but does not interpret state (`native/document/document_reader.cpp:34-36,50-51,73-85`); no evidence that DSP delays are serialized. 

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/ladder/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Original four-stage TPT cascade with `tan(pi*cutoff/sample_rate)`, delayed resonance feedback `0.85*resonance`, optional `tanh(3*drive*x)/(3*drive)` and denormal-tail clearing (`native/modules/ladder/module.cpp:21-36,70-82`; `native/modules/ladder/DESIGN.md:3-5`). Sample-rate validation 1–384000 Hz (`native/modules/ladder/module.cpp:11-15`); cutoff bounded to 0.45 sample rate before `tan` (`native/modules/ladder/module.cpp:33-36`). `process` executes bounded fixed-state math only, **no allocation, locks, I/O or mutable globals** (`native/modules/ladder/module.cpp:66-84`); allocation only in `create` (`native/modules/ladder/module.cpp:45-46`). Per-instance state, no external DSP code or pinned revision; GPLv3 LinearRamp contract as inherited dependency (`native/modules/ladder/DESIGN.md:7-13`; `native/modules/ladder/module.cpp:2-3`).
**Second-pass migration details.** `audio = in ? in[0] : nullptr`; missing and nonfinite input samples feed **zero to the filter, not guaranteed zero at its output**, since pole memory/feedback persist (`native/modules/ladder/module.cpp:38-43,66-82`). All three parameter IDs are continuous, range-checked floats sent to `LinearRamp`; the `tan` cutoff coefficient and optional `tanh` drive are **DSP transforms**, not parameter tapers. No integer/stepped casts (`native/modules/ladder/module.cpp:33-36,59-65,70-80`). Instance-only four-pole memory and previous output; `process` **does not allocate, lock, use I/O or write process-wide mutable globals** (`native/modules/ladder/module.cpp:38-46,66-84`). `DESIGN.md` asserts original DSP with **no external third-party DSP or pinned revision**, inheriting GPLv3 `LinearRamp` (`native/modules/ladder/DESIGN.md:5,9-13`).

## lfo

**1. Identity.** `essentials.lfo`; display name from `native/modules/lfo/module.json:1`; source `native/modules/lfo/module.cpp:53-61`, `native/modules/lfo/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `control` | CV | n/a | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `rate` | rate | .01 | 20 | 1 | Hz / plain value: LinearRamp (no ABI1 taper) | No: float accepted without rounding (`native/modules/lfo/module.cpp:35-40`) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |
| 1 | `depth` | depth | 0 | 1 | 1 | none / plain value: LinearRamp (no ABI1 taper) | No: float accepted without rounding (`native/modules/lfo/module.cpp:35-40`) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |
| 2 | `offset` | offset | -1 | 1 | 0 | none / plain value: LinearRamp (no ABI1 taper) | No: float accepted without rounding (`native/modules/lfo/module.cpp:35-40`) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |

**4. State.** Instance `double` phase, sample rate, and three `LinearRamp` parameter targets (`native/modules/lfo/module.cpp:8-14`); reset zeroes phase and resets ramps (`native/modules/lfo/module.cpp:21-33`). No save/load callback (`native/modules/lfo/module.cpp:57-59`); native document reader preserves original bytes/object, not DSP phase (`native/document/document_reader.cpp:34-36,50-51,82-85`).

**5. ABI 2 status.** Ported. ABI1 rate/depth/offset values and indices retained; ABI2 adds sync and beats (`native/modules/lfo/module.cpp:53-59`; `native/modules2/lfo/dsp.h:6-15`). Transport-synchronized phase is additional behaviour (`native/modules2/lfo/dsp.h:23-29`; `native/modules/lfo/module.cpp:42-50`). **No ID-breaking change found to existing IDs; additions require v1 migration defaults.**

**6. Port notes.** No inputs; `sin(phase)*depth+offset`, phase increment `tau*rate/sample_rate`, wraps by `fmod` (`native/modules/lfo/module.cpp:42-52`). Event values become ramp targets without integer casts (`native/modules/lfo/module.cpp:35-40`); plain-units ramp, not log/exponential taper. `process` has no allocation, locks, I/O or mutable globals; `new` only in create (`native/modules/lfo/module.cpp:15-18,42-52`). No `native/modules/lfo/DESIGN.md` at inspected ref (**UNKNOWN third-party revision/licence**; searched that exact path and `native/modules/lfo/module.cpp:1-4` only shows project SDK/ramp). ABI2 uses 16 double phases and transport (`native/modules2/lfo/dsp.h:16-30`).

**ABI 1 vs ABI 2 interface (within-direction indices):**

| Element | ABI 1 ID/index/range/default | ABI 2 ID/index/range/default | Classification |
|---|---|---|---|
| output | `control` 0, control (`native/modules/lfo/module.cpp:53`) | `control` 0, bipolar CV (`native/modules2/lfo/dsp.h:8`) | identical ID/order; cosmetic signal hint |
| Rate | `rate` 0, .01..20 / 1 Hz (`native/modules/lfo/module.json:1`; `native/modules/lfo/module.cpp:54`) | `rate` 0, .01..20 / 1 Hz (`native/modules2/lfo/dsp.h:11`) | identical |
| Depth | `depth` 1, 0..1 / 1 (`native/modules/lfo/module.json:1`; `native/modules/lfo/module.cpp:55`) | `depth` 1, 0..1 / 1 (`native/modules2/lfo/dsp.h:12`) | identical |
| Offset | `offset` 2, -1..1 / 0 (`native/modules/lfo/module.json:1`; `native/modules/lfo/module.cpp:56`) | `offset` 2, -1..1 / 0 (`native/modules2/lfo/dsp.h:13`) | identical |
| Tempo sync | — (ABI1 3 params, `native/modules/lfo/module.cpp:57-59`) | `sync` 3, 0..1 / 0, stepped (`native/modules2/lfo/dsp.h:14`) | added |
| Beats/cycle | — (`native/modules/lfo/module.cpp:57-59`) | `beats` 4, .125..16 / 1 beats (`native/modules2/lfo/dsp.h:15`) | added |

**Second-pass migration details.** There are **zero ABI 1 inputs**; `process` never reads an input buffer (`native/modules/lfo/module.cpp:42-59`). Rate/Depth/Offset events feed float targets directly, **none is rounded or stepped**, and `sin(phase)*depth+offset` uses linear ramped values; radians and `fmod` are oscillator math, not parameter tapers (`native/modules/lfo/module.cpp:35-56`). `process` uses instance phase/ramps and a bounded loop; **no allocation, lock, I/O or mutable process-wide global** (`native/modules/lfo/module.cpp:8-18,42-52`). The existing ID/index/range/defaults survive in ABI 2; **new** `sync` (index 3, 0..1/default 0, stepped) and `beats` (index 4, .125..16/default 1, LOG metadata) change transport behavior but do not rename the three existing parameters (`native/modules2/lfo/dsp.h:10-15,23-30`). No `native/modules/lfo/DESIGN.md` exists at this ref; external source/licence/pin is **UNKNOWN (looked in that path and `native/modules/lfo/module.cpp:1-5`; implementation imports the project ramp)**.

## mixer

**1. Identity.** `essentials.mixer`; display name from `native/modules/mixer/module.json:1`; source `native/modules/mixer/module.cpp:62-92`, `native/modules/mixer/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `a` | audio | null input pointer becomes 0.0 per frame (`native/modules/mixer/module.cpp:48-59`) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| input 1 | `b` | audio | null input pointer becomes 0.0 per frame (`native/modules/mixer/module.cpp:48-59`) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `level_a` | level a | 0 | 2 | .5 | none / plain LinearRamp gain (no module taper) | No: float targets accepted directly (`native/modules/mixer/module.cpp:37-45`) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| 1 | `level_b` | level b | 0 | 2 | .5 | none / plain LinearRamp gain (no module taper) | No: float targets accepted directly (`native/modules/mixer/module.cpp:37-45`) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |

**4. State.** Per-instance A/B ramps, sample rate and maximum frame count (`native/modules/mixer/module.cpp:7-12`); reset snaps ramps without changing targets (`native/modules/mixer/module.cpp:22-35`). Descriptor has no save/load callback (`native/modules/mixer/module.cpp:73-90`); module-local serialized DSP state not found in source. Native document reader preserves raw JSON (`native/document/document_reader.cpp:34-36,50-51`).

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/mixer/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Two nullable audio inputs are independently zeroed when absent, then sample-by-sample multiplied by smoothed Level A and B and added (`native/modules/mixer/module.cpp:48-60`); `LinearRamp` from project (`native/modules/mixer/module.cpp:1-2,7-9`). No process allocation, locks, I/O or mutable globals (`native/modules/mixer/module.cpp:48-60`); only `create` invokes `new` (`native/modules/mixer/module.cpp:14-20`). `prepare` requires positive sample rate and nonzero max block (`native/modules/mixer/module.cpp:22-31`). `native/modules/mixer/DESIGN.md` not found at inspected ref; third-party pinned revision/licence **UNKNOWN (looked in DESIGN.md and module.cpp:1-3)**.

**Second-pass migration details.** Each `in[0]`/`in[1]` **buffer** may be null and contributes exactly 0, but the `in` **array pointer itself is dereferenced without a guard**; caller must supply a valid port-pointer array (`native/modules/mixer/module.cpp:48-59`). Both levels are continuous floating ramp targets; there is no step/rounding, logarithmic scale or other taper in `event` or `process` (`native/modules/mixer/module.cpp:37-45,55-59`). `process` only reads instance ramps and iterates frames: **no allocation, lock, I/O or mutable process-wide global**, allocation is in `create` (`native/modules/mixer/module.cpp:7-15,48-60`). No `native/modules/mixer/DESIGN.md` exists at this ref; external source/licence/pin is **UNKNOWN (looked there and `native/modules/mixer/module.cpp:1-4`)**.

## noise

**1. Identity.** `essentials.noise`; display name from `native/modules/noise/module.json:1`; source `native/modules/noise/module.cpp:49-55`, `native/modules/noise/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `audio` | audio | n/a | `native/modules/noise/module.cpp:49-55`; `native/modules/noise/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `level` | level | 0 | 1 | .2 | none / plain level through LinearRamp; no log/exponential taper (`native/modules/noise/module.cpp:34-37,42-46`) | No: finite continuous float 0..1 (`native/modules/noise/module.cpp:34-37`) | `native/modules/noise/module.cpp:49-55`; `native/modules/noise/module.json:1` |

**4. State.** Per-instance 32-bit PRNG state seeded `0x12345678`, level ramp and block frame cap; reset and prepare restore seed (`native/modules/noise/module.cpp:21-33`). No module-local save/load in callback descriptor (`native/modules/noise/module.cpp:49-54`); raw patch JSON preserved by inspector (`native/document/document_reader.cpp:34-36,50-51`) but RNG state is not serialized by module source.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/noise/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Original fixed-seed xorshift32 uniform noise generator, level even when zero consumes PRNG state (`native/modules/noise/module.cpp:21-23,39-47`; `native/modules/noise/DESIGN.md:3-5`). No input ports (`native/modules/noise/module.cpp:49-54`). Sample rate valid 1..384000 and max block >0 (`native/modules/noise/module.cpp:11-15`). No allocation, locks, I/O or mutable globals in `process` (`native/modules/noise/module.cpp:39-48`); heap allocation only in `create` (`native/modules/noise/module.cpp:24-25`). `DESIGN.md` reports original implementation, no third-party DSP / pinned revision, inherited GPLv3 LinearRamp contract (`native/modules/noise/DESIGN.md:7-13`).

**Second-pass migration details.** `input_count=0`, and `process` never reads input buffers; the fixed-seed instance PRNG advances once per output sample, **even at zero Level** (`native/modules/noise/module.cpp:21-23,39-54`). Level is a continuous finite float in 0..1, passed to `LinearRamp`, multiplied after generation, with **no stepped cast or nonlinear parameter taper** (`native/modules/noise/module.cpp:34-37,42-47`). Seed is a `constexpr` shared constant, **not mutable global state**; `process` has **no allocation, lock, I/O or mutable global** (`native/modules/noise/module.cpp:21-25,39-48`). `DESIGN.md` explicitly records original DSP, no external implementation/pinned revision and inherited GPLv3 ramp (`native/modules/noise/DESIGN.md:5,9-13`).

## output

**1. Identity.** `essentials.output`; display name from `native/modules/output/module.json:1`; source `native/modules/output/module.cpp:15-33`, `native/modules/output/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | null input port resolves to silence (`native/modules/output/module.cpp:10-14`) | `native/modules/output/module.cpp:15-33`; `native/modules/output/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/output/module.cpp:15-33`; `native/modules/output/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| — | none | — | — | — | — | — | — | `native/modules/output/module.cpp:15-33` |

**4. State.** Empty module State, no-op reset and event, prepare returns 0 (`native/modules/output/module.cpp:4-9`). No numeric parameters or module-local save/load (`native/modules/output/module.cpp:17-32`). The reader retains source patch JSON (`native/document/document_reader.cpp:34-36,50-51`); no extra per-module runtime state to serialize.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/output/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Output is transparent mono pass-through; a null `in[0]` yields literal zero (not an uninitialized sample) (`native/modules/output/module.cpp:10-14`). No sample-rate assumptions or dynamic dependencies in processing. Bounded per-frame writes, **no allocation, locks, I/O or mutable globals** (`native/modules/output/module.cpp:4-14`); heap allocation only in `create` (`native/modules/output/module.cpp:5-6`). No `native/modules/output/DESIGN.md` at inspected ref; third-party source / pinned revision **UNKNOWN** (looked in `native/modules/output/module.cpp:1-2`, DESIGN.md absent).

**Second-pass migration details.** A **null `in[0]` buffer** is replaced with zero sample-by-sample; the `in` **array itself is not checked** and must be valid (`native/modules/output/module.cpp:10-14`). There are no parameter IDs or tapers, and `event` is a no-op (`native/modules/output/module.cpp:7-9,17-26`). `process` is pure bounded copy/zero: **no allocation, lock, I/O or mutable process-wide global**; `create` alone allocates an empty instance (`native/modules/output/module.cpp:4-14`). No `native/modules/output/DESIGN.md` exists at this ref; third-party provenance/licence/pin is **UNKNOWN (looked there and `native/modules/output/module.cpp:1-2`)**.

## scale_offset

**1. Identity.** `essentials.scale_offset`; display name from `native/modules/scale_offset/module.json:1`; source `native/modules/scale_offset/module.cpp:38-46`, `native/modules/scale_offset/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `control` | CV | null input pointer gives zero, output becomes offset (`native/modules/scale_offset/module.cpp:30-35`) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |
| output 0 | `control` | CV | n/a | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `scale` | scale | -2 | 2 | 1 | none / plain LinearRamp interpolation in DSP, not a nonlinear parameter taper (`native/modules/scale_offset/module.cpp:24-35`) | No integer/enum coercion: direct float `set` (`native/modules/scale_offset/module.cpp:24-29`) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |
| 1 | `offset` | offset | -1 | 1 | 0 | none / plain LinearRamp interpolation in DSP, not a nonlinear parameter taper (`native/modules/scale_offset/module.cpp:24-35`) | No integer/enum coercion: direct float `set` (`native/modules/scale_offset/module.cpp:24-29`) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |

**4. State.** Per-instance Scale and Offset ramps, reset snaps both to targets and prepare supplies sample rate (`native/modules/scale_offset/module.cpp:6-22`). No serialized DSP state callbacks in descriptor (`native/modules/scale_offset/module.cpp:42-44`); reader retains JSON bytes but does not interpret parameters (`native/document/document_reader.cpp:34-36,82-85`).

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/scale_offset/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Control output = `(input or 0)*scale.next()+offset.next()` per sample (`native/modules/scale_offset/module.cpp:30-36`); direct continuous values set by numeric ID, no rounding or nonlinear taper (`native/modules/scale_offset/module.cpp:24-29`). Loop has **no allocation, locks, I/O or mutable global** (`native/modules/scale_offset/module.cpp:30-36`); only create allocates (`native/modules/scale_offset/module.cpp:11-12`). Sample rate used to prepare ramps (`native/modules/scale_offset/module.cpp:18-22`). `native/modules/scale_offset/DESIGN.md` absent at inspected ref: third-party pinned source/licence **UNKNOWN** (looked there and `native/modules/scale_offset/module.cpp:1-3`).

**Second-pass migration details.** Missing **array or buffer** resolves input to zero, so an unpatched output equals the **smoothed offset**, not necessarily zero if Offset has been changed (`native/modules/scale_offset/module.cpp:30-36`). Scale and Offset events set continuous float `LinearRamp` targets without coercion; multiplication/addition is direct linear parameter use, **no stepped or nonlinear taper** (`native/modules/scale_offset/module.cpp:24-36`). `process` has bounded per-frame instance-only work: **no allocation, lock, I/O or mutable process-wide global**, `new` is only in `create` (`native/modules/scale_offset/module.cpp:6-12,30-36`). No `native/modules/scale_offset/DESIGN.md` exists at this ref; third-party provenance/licence/pin is **UNKNOWN (looked there and `native/modules/scale_offset/module.cpp:1-3`)**.

## slew

**1. Identity.** `essentials.slew`; display name from `native/modules/slew/module.json:1`; source `native/modules/slew/module.cpp:56-62`, `native/modules/slew/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `control` | CV | null input pointer means 0 target; slew toward zero from current level (no instant reset) (`native/modules/slew/module.cpp:37-53`) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |
| output 0 | `control` | CV | n/a | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `rise` | rise | 0 | 10 | .1 | s/unit / plain seconds/unit, linear output slew; no logarithmic parameter mapping (`native/modules/slew/module.cpp:30-35,44-50`) | No, accepts finite continuous 0..10 directly (`native/modules/slew/module.cpp:30-36`) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |
| 1 | `fall` | fall | 0 | 10 | .1 | s/unit / plain seconds/unit, linear output slew; no logarithmic parameter mapping (`native/modules/slew/module.cpp:30-35,44-50`) | No, accepts finite continuous 0..10 directly (`native/modules/slew/module.cpp:30-36`) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |

**4. State.** Instance double level (held value), sample rate, rise/fall time, max frame count (`native/modules/slew/module.cpp:21-29`). `reset` sets held value to zero but retains parameter times (`native/modules/slew/module.cpp:29-36`). No per-module save callbacks (`native/modules/slew/module.cpp:59-61`); inspector preserves original JSON (`native/document/document_reader.cpp:34-36,50-51`) without capturing current held level.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 interface comparison **not applicable**: no `native/modules2/slew/dsp.h` in the inspected ABI 2 module set.

**6. Port notes.** Linear per-unit slew with step `1/(T*sample_rate)` and no overshoot; `T=0` snaps to input, NaN/Inf holds previous output (`native/modules/slew/module.cpp:37-55`; `native/modules/slew/DESIGN.md:3-5`). `prepare` validates sample rate 1..384000 Hz (`native/modules/slew/module.cpp:11-15,24-28`). No process-time allocation, locks, I/O or mutable globals (`native/modules/slew/module.cpp:37-55`); only create uses new (`native/modules/slew/module.cpp:22-23`). `DESIGN.md` reports original implementation, no third-party DSP / revision, inherited GPLv3 ramp contract (`native/modules/slew/DESIGN.md:7-13`).

**Second-pass migration details.** Missing `in` array or `in[0]` buffer implies target **zero**, but output is the **previous held level slewed toward zero**, not instant silence (`native/modules/slew/module.cpp:37-53`). Rise/Fall are continuous `s/unit` values (no integer coercion, logarithmic parameter taper or separate smoothing ramp): time `T=0` snaps; otherwise step is `1/(T*sample_rate)` in the appropriate direction; nonfinite sample holds the state (`native/modules/slew/module.cpp:30-36,41-53`). **No process allocation, locks, I/O or process-wide mutable globals** (`native/modules/slew/module.cpp:21-23,37-55`). `DESIGN.md` explicitly says original implementation, no third-party DSP/pin and inherited GPLv3 `LinearRamp` contract (`native/modules/slew/DESIGN.md:5,9-13`).

## vca

**1. Identity.** `essentials.vca`; display name from `native/modules/vca/module.json:1`; source `native/modules/vca/module.cpp:31-51`, `native/modules/vca/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | unpatched audio 0 (`native/modules/vca/module.cpp:24-29`) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |
| input 1 | `gain_cv` | CV | unity (`native/modules/vca/module.cpp:24-30`) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `gain` | gain | 0 | 2 | 1 | none / plain linear gain target through LinearRamp; no DSP parameter taper (`native/modules/vca/module.cpp:18-29`) | No: raw float gain, no cast/rounding (`native/modules/vca/module.cpp:18-22`) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |

**4. State.** Instance contains gain LinearRamp only; prepare uses sample rate, reset snaps ramp (`native/modules/vca/module.cpp:6-17`). No module-local save/load callback (`native/modules/vca/module.cpp:35-50`). Document reader retains patch bytes and parsed JSON object (`native/document/document_reader.cpp:34-36,50-51`); it does not export ramp internals.

**5. ABI 2 status.** Ported: stable module/port/parameter IDs, order and ranges/defaults (`native/modules/vca/module.cpp:31-50`; `native/modules2/vca/dsp.h:6-9`). ABI2 clarifies gain_cv default=1 and CV-unipolar hint; uses 16-voice SIMD and constant fast path (`native/modules2/vca/dsp.h:7,11-19`). **No ID-breaking or range/default changes observed; signal-kind descriptor changed (cosmetic)**. Bitwise sound equivalence UNKNOWN without test.

**6. Port notes.** ABI1 `audio` null -> 0, `gain_cv` null -> 1, connected gain CV multiplies sample regardless of param (`native/modules/vca/module.cpp:24-29`); plain continuous gain ramp (`native/modules/vca/module.cpp:18-22`). `process` has **no allocation, locks, I/O or mutable globals** (`native/modules/vca/module.cpp:24-30`); allocation only in `create` (`native/modules/vca/module.cpp:9-10`). Sample rate supplied to ramp at prepare (`native/modules/vca/module.cpp:15-17`). `native/modules/vca/DESIGN.md` absent at inspected ref; third-party pin/licence UNKNOWN (inspected `native/modules/vca/module.cpp:1-4` and DESIGN.md path).

**ABI 1 vs ABI 2 full interface (indices within direction):**

| Element | ABI 1 ID/index/range/default | ABI 2 ID/index/range/default | Classification |
|---|---|---|---|
| Input audio | `audio` 0, WS_AUDIO, null=0 (`native/modules/vca/module.cpp:24-33`) | `audio` 0, WS2_AUDIO, unpatched 0 (`native/modules2/vca/dsp.h:7,11-18`) | identical |
| Input gain CV | `gain_cv` 1, WS_CONTROL, null=1 (`native/modules/vca/module.cpp:24-33`) | `gain_cv` 1, WS2_CV_UNI, default 1 (`native/modules2/vca/dsp.h:7`) | identical ID/order; cosmetic hint |
| Output audio | `audio` 0, WS_AUDIO (`native/modules/vca/module.cpp:31-33`) | `audio` 0, WS2_AUDIO (`native/modules2/vca/dsp.h:8`) | identical |
| Gain | `gain` 0, 0..2 / 1 (`native/modules/vca/module.json:1`; `native/modules/vca/module.cpp:34`) | `gain` 0, 0..2 / 1 (`native/modules2/vca/dsp.h:9`) | identical |

**Second-pass migration details.** The *audio buffer* `in[0] == nullptr` contributes zero; **gain CV buffer** `in[1] == nullptr` contributes **unity**, not zero. The enclosing `in` array must exist because `process` dereferences it (`native/modules/vca/module.cpp:24-30`). ABI 1 Gain is continuously ramped; `event` does no integer coercion, and `process` multiplies audio × CV × gain without nonlinear parameter taper (`native/modules/vca/module.cpp:18-29`). **No allocation, lock, I/O or mutable process-wide global in process** (`native/modules/vca/module.cpp:6-10,24-30`). No `native/modules/vca/DESIGN.md` exists at this ref; third-party provenance/licence/pin is **UNKNOWN (looked there and `native/modules/vca/module.cpp:1-4`)**. The ABI 2 descriptor expressly supplies CV default 1 (`native/modules2/vca/dsp.h:7`); the descriptor identity matches, but ABI 1 ramp timing versus ABI 2 `b.param`/constant path should be regression-tested (`native/modules/vca/module.cpp:18-29`; `native/modules2/vca/dsp.h:11-19`).

## vcf

**1. Identity.** `essentials.vcf`; display name from `native/modules/vcf/module.json:1`; source `native/modules/vcf/module.cpp:85-91`, `native/modules/vcf/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | null/missing input -> 0; nonfinite sample -> 0 (`native/modules/vcf/module.cpp:63-69`) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| output 0 | `low12` | audio | n/a | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| output 1 | `low24` | audio | n/a | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `cutoff` | cutoff | 20 | 20000 | 1000 | Hz / raw ramp feeding tan(pi*f/fs), not UI taper (`native/modules/vcf/module.cpp:33-36,60,69`) | No: continuous float, no cast or rounding (`native/modules/vcf/module.cpp:57-62`) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| 1 | `resonance` | resonance | 0 | .95 | 0 | none / raw linear parameter ramp (`native/modules/vcf/module.cpp:61,70`) | No: continuous float, no cast or rounding (`native/modules/vcf/module.cpp:57-62`) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |

**4. State.** Per-instance double state for four poles, cutoff/resonance LinearRamp targets, sample rate and frame limit (`native/modules/vcf/module.cpp:38-43`); prepare/reset clear poles (`native/modules/vcf/module.cpp:46-56`). No module save-state callback (`native/modules/vcf/module.cpp:88-90`); document reader retains original JSON, not internal filter state (`native/document/document_reader.cpp:34-36,50-51,82-85`).

**5. ABI 2 status.** Ported. Port/parameter IDs, per-direction order and parameter ranges/defaults retained (`native/modules/vcf/module.cpp:85-90`; `native/modules2/vcf/dsp.h:6-10`). ABI 2 supplies LOG cutoff descriptor taper (cosmetic metadata), and changes to poly state/cache (`native/modules2/vcf/dsp.h:10-31`) rather than ABI 1 singleton ramped coefficients (`native/modules/vcf/module.cpp:63-83`). No ID-breaking or range/default change to existing fields; behavioural matching UNKNOWN until DSP tests.

**6. Port notes.** Original four-pole trapezoidal/TPT lowpass; cutoff capped to .45 sample rate, resonance feeds algebraically solved zero-delay feedback, low12/low24 taps at stages 2/4 (`native/modules/vcf/module.cpp:21-36,63-83`; `native/modules/vcf/DESIGN.md:3-5`). DSP process is bounded, **no allocation, locks, I/O, mutable globals** (`native/modules/vcf/module.cpp:63-84`); allocation only in `create` (`native/modules/vcf/module.cpp:44-45`). Sample rate validated from 1 to 384000 Hz (`native/modules/vcf/module.cpp:11-15`). `DESIGN.md` confirms original DSP, no external third-party source or pinned revision, inherited GPLv3 LinearRamp contract (`native/modules/vcf/DESIGN.md:7-13`).

**ABI 1 vs ABI 2 full interface (per-direction indices):**

| Element | ABI 1 ID/index/range/default | ABI 2 ID/index/range/default | Classification |
|---|---|---|---|
| Input audio | `audio` 0, WS_AUDIO, null=0 (`native/modules/vcf/module.cpp:63-68,85`) | `audio` 0 WS2_AUDIO (`native/modules2/vcf/dsp.h:7`) | identical |
| Output low12 | `low12` 0 WS_AUDIO (`native/modules/vcf/module.cpp:86`) | `low12` 0 WS2_AUDIO (`native/modules2/vcf/dsp.h:8`) | identical |
| Output low24 | `low24` 1 WS_AUDIO (`native/modules/vcf/module.cpp:86`) | `low24` 1 WS2_AUDIO (`native/modules2/vcf/dsp.h:8`) | identical |
| Cutoff | `cutoff` 0, 20..20000 / 1000 Hz (`native/modules/vcf/module.json:9`; `native/modules/vcf/module.cpp:87`) | `cutoff` 0, 20..20000 / 1000 Hz, LOG taper (`native/modules2/vcf/dsp.h:10`) | identical range/default, cosmetic taper descriptor |
| Resonance | `resonance` 1, 0..0.95 / 0 (`native/modules/vcf/module.json:10`; `native/modules/vcf/module.cpp:87`) | `resonance` 1, 0..0.95 / 0 (`native/modules2/vcf/dsp.h:10`) | identical |

## vco

**1. Identity.** `essentials.vco`; display name from `native/modules/vco/module.json:1`; source `native/modules/vco/module.cpp:43-61`, `native/modules/vco/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `audio` | audio | n/a | `native/modules/vco/module.cpp:43-61`; `native/modules/vco/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `frequency` | frequency | .01 | 20000 | 220 | Hz / raw frequency Hz LinearRamp, then Nyquist cap (no logarithmic module taper) (`native/modules/vco/module.cpp:24-38`) | No: continuous float from parameter or MIDI exponential note mapping (`native/modules/vco/module.cpp:24-31`) | `native/modules/vco/module.cpp:43-61`; `native/modules/vco/module.json:1` |

**4. State.** Per-instance `double` phase and sample rate, frequency LinearRamp; reset zeroes phase and snaps ramp (`native/modules/vco/module.cpp:8-22`). No state save callback in module descriptor (`native/modules/vco/module.cpp:45-60`); document inspector retains JSON object and original bytes but does not capture oscillator phase (`native/document/document_reader.cpp:34-36,50-51,82-85`).

**5. ABI 2 status.** Ported, existing output `audio` and frequency ID 0/range/default retained (`native/modules/vco/module.cpp:43-60`; `native/modules2/vco/dsp.h:6-11`). Added pitch, FM and notes inputs; ABI 2 supports 16 channel phases and event/choke semantics (`native/modules2/vco/dsp.h:12-31`) while ABI 1 takes note event value and directly jumps smoothed frequency (`native/modules/vco/module.cpp:24-31`). Additions are **added** rather than numerically ID-breaking where source uses string port IDs. Full behaviour equivalence UNKNOWN.

**6. Port notes.** ABI1 produces `sin(double_phase)` and increments phase by `tau*min(frequency, 0.49*sample_rate)/sample_rate`; note-on maps MIDI value to `440*2^((note-69)/12)` (`native/modules/vco/module.cpp:24-42`). No audio input port (`native/modules/vco/module.cpp:43-54`). No `new`, locks, file/device I/O or mutable globals in process (`native/modules/vco/module.cpp:33-42`); `new` only in create (`native/modules/vco/module.cpp:12-13`). Prepare sets rate and ramp (`native/modules/vco/module.cpp:18-22`). `native/modules/vco/DESIGN.md` absent at inspected ref; third-party pinned source and licence UNKNOWN (looked at DESIGN.md and `native/modules/vco/module.cpp:1-5`).

**ABI1 vs ABI2 full interface (per-direction indices):**

| Element | ABI 1 ID/index/range/default | ABI 2 ID/index/range/default | Classification |
|---|---|---|---|
| Input pitch | none (`native/modules/vco/module.cpp:45-54`) | `pitch` 0 WS2_PITCH (`native/modules2/vco/dsp.h:7-9`) | added |
| Input FM | none (`native/modules/vco/module.cpp:45-54`) | `fm` 1 WS2_AUDIO (`native/modules2/vco/dsp.h:7-9`) | added |
| Input notes | none (`native/modules/vco/module.cpp:45-54`) | `notes` 2 WS2_EV_NOTES event (`native/modules2/vco/dsp.h:7-9`) | added |
| Output audio | `audio` 0 WS_AUDIO (`native/modules/vco/module.cpp:43`) | `audio` 0 WS2_AUDIO (`native/modules2/vco/dsp.h:10`) | identical |
| Frequency | `frequency` 0, .01..20000 / 220 Hz (`native/modules/vco/module.json:1`; `native/modules/vco/module.cpp:44`) | `frequency` 0, .01..20000 / 220 Hz (`native/modules2/vco/dsp.h:11`) | identical |

## Native patch v1 document representation (reader evidence)

- Recognized native patch shape is `format: "wirescape.native.patch"`, integer `version: 1` (`native/document/document_reader.cpp:52-59`); top-level `modules` and `connections` must be arrays; native `name` is a string and `outputNode` is a nonnegative integer (`native/document/document_reader.cpp:67-71`).
- Every native module entry must be an object with positive **numeric node `id`** and non-empty **string `type`** (the module's type ID); numeric node IDs must be unique (`native/document/document_reader.cpp:73-81`). The inspector **does not interpret** parameter values, state or coordinates (`native/document/document_reader.cpp:82-85`). The checked-in native v1 demo represents module parameters under `values` using **numeric-string parameter indices** (`"0"`, `"1"`, etc.), not semantic names (`native/examples/Six_Essentials_Demo.json:7-26,29-47,49-76`). The reader itself does not validate or translate these keys (`native/document/document_reader.cpp:73-85`); the full consumer's behavior remains UNKNOWN (reader and fixture inspected, engine/CLI ingestion not traced).
- Each native cable uses integer `fromNode` and `toNode` **node IDs**, nonempty **string `fromPort` and `toPort` port IDs**, and nonempty string `kind` (`native/document/document_reader.cpp:86-92`). It does **not** use numeric port indices in this native v1 schema. Legacy cables are instead `module:port` strings in `from` and `to` (`native/document/document_reader.cpp:19-25,93-95`). Consequently new ABI 2 input ports are **additions**, not automatically numeric-index collisions; semantic patch mapping and defaults still require verification.
- Unknown fields: `inspect()` stores **unmodified original bytes** and the entire parsed top-level `QJsonObject` (`native/document/document_reader.cpp:34-36,45-51`); it explicitly avoids interpreting fields not required for identity/structure (`native/document/document_reader.cpp:82-85`). For unknown formats/versions it retains the bytes and returns without parsing the schema (`native/document/document_reader.cpp:60-65`). **Re-emission with unknown fields preserved is UNKNOWN** (this reader never writes documents; inspect writer/export/import pipeline). The reader saves legacy `sequence` and `module_state` fields separately, but only on non-native formats (`native/document/document_reader.cpp:82-85`).
- This is a **structure inspection**, not successful native graph import: diagnostic explicitly says full graph validation remains (`native/document/document_reader.cpp:97-99`). No inspector code serializes live module-private phase, filter poles or RNG (`native/document/document_reader.cpp:73-85`).

## Summary

| Module | ABI 2 ported? | Changes to existing IDs | Added ABI 2 interface | Risk (planning) | Evidence |
|---|---|---|---|---|---|
| adsr | yes | none identified | gate input, notes event input | high | `native/modules/adsr/module.cpp:94-108`; `native/modules2/adsr/dsp.h:6-10` |
| clock | no in reviewed ABI2 set | n/a | not ported | high | `native/modules/clock/module.cpp:67-73` |
| function_envelope | no in reviewed ABI2 set | n/a | not ported | med | `native/modules/function_envelope/module.cpp:74-80` |
| ladder | no in reviewed ABI2 set | n/a | not ported | high | `native/modules/ladder/module.cpp:85-90` |
| lfo | yes | none identified | sync and beats parameters | high | `native/modules/lfo/module.cpp:53-59`; `native/modules2/lfo/dsp.h:6-15` |
| mixer | no in reviewed ABI2 set | n/a | not ported | med | `native/modules/mixer/module.cpp:62-90` |
| noise | no in reviewed ABI2 set | n/a | not ported | high | `native/modules/noise/module.cpp:49-54` |
| output | no in reviewed ABI2 set | n/a | not ported | med | `native/modules/output/module.cpp:15-32` |
| scale_offset | no in reviewed ABI2 set | n/a | not ported | med | `native/modules/scale_offset/module.cpp:38-44` |
| slew | no in reviewed ABI2 set | n/a | not ported | med | `native/modules/slew/module.cpp:56-61` |
| vca | yes | none identified | none | med | `native/modules/vca/module.cpp:31-50`; `native/modules2/vca/dsp.h:6-9` |
| vcf | yes | none identified | none; log taper descriptor | med | `native/modules/vcf/module.cpp:85-90`; `native/modules2/vcf/dsp.h:6-10` |
| vco | yes | none identified | pitch/FM/notes inputs | high | `native/modules/vco/module.cpp:43-60`; `native/modules2/vco/dsp.h:6-11` |

## Open questions for Claude

1. **Native document parameter consumption:** the checked-in native v1 fixture uses `values` with stringified numeric parameter indices (`native/examples/Six_Essentials_Demo.json:12-25,33-36`). Inspect the parser actually applying those indices to engine events and the patch writer; `document_reader.cpp` validates only module `id`/`type`, not `values` (`native/document/document_reader.cpp:73-85`).
2. **Unknown JSON preservation on output:** original byte/object retention is proven for `inspect`, but later saving without loss is not established (`native/document/document_reader.cpp:34-36,50-51,97-99`). Verify actual write path.
3. **ABI 2 new inputs and parameters:** preserve v1 semantics/defaults for ADSR, VCO and LFO, then test routing by stable string port IDs (`native/document/document_reader.cpp:86-92`; `native/modules2/adsr/dsp.h:7-10`; `native/modules2/lfo/dsp.h:10-15`; `native/modules2/vco/dsp.h:7-11`).
4. **Source and DSP state migration:** validate which patch fields persist musical control targets versus private DSP state; reader does not interpret state (`native/document/document_reader.cpp:82-85`); noise RNG and clock phase are runtime members (`native/modules/noise/module.cpp:21-33`; `native/modules/clock/module.cpp:21-39`).
5. **Behavioural equivalence:** ABI 2 VCF poly cache/log taper, ADSR note/gate precedence, VCO poly note semantics and LFO transport synchronization require focused signal regression tests (`native/modules2/vcf/dsp.h:10-31`; `native/modules2/adsr/dsp.h:18-38`; `native/modules2/vco/dsp.h:20-31`; `native/modules2/lfo/dsp.h:23-30`).

