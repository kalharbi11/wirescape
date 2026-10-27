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

**5. ABI 2 status.** Ported. `native/modules2/adsr/dsp.h:6-10`: existing parameter and envelope IDs/ranges/defaults retained; added gate and notes input ports (**ID-breaking** port layout). Notes ignored while gate patched and per-voice state changes (`native/modules2/adsr/dsp.h:11-38`; `native/modules/adsr/module.cpp:38-60`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** `process` has only fixed arithmetic and output writes over `frames`, with no allocation, lock, I/O or mutable global; `new` appears only in `create` (`native/modules/adsr/module.cpp:16-17,62-93`). `DESIGN.md`: **UNKNOWN (file absent at this ref)**; no third-party pin/licence recoverable from inspected module.cpp/module.json.

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

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** BPM ramps, ticks rounded, duty direct, run thresholded; output high window and sample-rate phase from (`native/modules/clock/module.cpp:40-65`). Process has bounded loop, no allocation, locking, I/O or mutable globals (`native/modules/clock/module.cpp:51-66`); only create allocates (`native/modules/clock/module.cpp:27-28`). `DESIGN.md:9,13` describes fixed instance state, original DSP, no external third-party DSP code or pinned revision; sample rate 1..384000 Hz (`native/modules/clock/module.cpp:11-15`).

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

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** Rise/fall are seconds converted with `llround(time * sample_rate)` into a discrete *duration*, but parameter values remain continuous; Curve is a continuous `expm1(4*Curve*u)/expm1(4*Curve)` shape, not log parameter taper (`native/modules/function_envelope/module.cpp:40-52,61-71`). Bounded per-frame work; no allocation, locks, I/O or globals inside `process` (`native/modules/function_envelope/module.cpp:54-73`), allocation only in create (`native/modules/function_envelope/module.cpp:28-29`). Sample rate accepted 1..384000 Hz (`native/modules/function_envelope/module.cpp:11-15`). `native/modules/function_envelope/DESIGN.md:3-13`: original gated AR DSP with no external third-party code/pinned revision, fixed instance state.

## ladder

**1. Identity.** `essentials.ladder`; display name from `native/modules/ladder/module.json:1`; source `native/modules/ladder/module.cpp:85-91`, `native/modules/ladder/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `cutoff` | cutoff | 20 | 20000 | 1000 | Hz / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| 1 | `resonance` | resonance | 0 | 1 | 0 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |
| 2 | `drive` | drive | 0 | 2 | 0 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/ladder/module.cpp:85-91`; `native/modules/ladder/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/ladder/module.cpp:1-84`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/ladder/module.cpp, native/modules/ladder/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/ladder/module.cpp:1-91`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/ladder/module.cpp and native/modules/ladder/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## lfo

**1. Identity.** `essentials.lfo`; display name from `native/modules/lfo/module.json:1`; source `native/modules/lfo/module.cpp:53-61`, `native/modules/lfo/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `control` | CV | n/a | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `rate` | rate | .01 | 20 | 1 | Hz / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |
| 1 | `depth` | depth | 0 | 1 | 1 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |
| 2 | `offset` | offset | -1 | 1 | 0 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/lfo/module.cpp:53-61`; `native/modules/lfo/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/lfo/module.cpp:1-52`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/lfo/module.cpp, native/modules/lfo/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Ported. `native/modules2/lfo/dsp.h:6-15`: existing rate/depth/offset parameters retained; added `sync` index 3 and `beats` index 4 (**ID-breaking** schema extension, previous indices stable). Tempo transport mode changes behaviour (`native/modules2/lfo/dsp.h:24-29`; `native/modules/lfo/module.cpp:35-50`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/lfo/module.cpp:1-61`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/lfo/module.cpp and native/modules/lfo/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## mixer

**1. Identity.** `essentials.mixer`; display name from `native/modules/mixer/module.json:1`; source `native/modules/mixer/module.cpp:62-92`, `native/modules/mixer/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `a` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| input 1 | `b` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `level_a` | level a | 0 | 2 | .5 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |
| 1 | `level_b` | level b | 0 | 2 | .5 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/mixer/module.cpp:62-92`; `native/modules/mixer/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/mixer/module.cpp:1-61`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/mixer/module.cpp, native/modules/mixer/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/mixer/module.cpp:1-92`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/mixer/module.cpp and native/modules/mixer/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## noise

**1. Identity.** `essentials.noise`; display name from `native/modules/noise/module.json:1`; source `native/modules/noise/module.cpp:49-55`, `native/modules/noise/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `audio` | audio | n/a | `native/modules/noise/module.cpp:49-55`; `native/modules/noise/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `level` | level | 0 | 1 | .2 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/noise/module.cpp:49-55`; `native/modules/noise/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/noise/module.cpp:1-48`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/noise/module.cpp, native/modules/noise/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/noise/module.cpp:1-55`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/noise/module.cpp and native/modules/noise/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## output

**1. Identity.** `essentials.output`; display name from `native/modules/output/module.json:1`; source `native/modules/output/module.cpp:15-33`, `native/modules/output/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/output/module.cpp:15-33`; `native/modules/output/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/output/module.cpp:15-33`; `native/modules/output/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| — | none | — | — | — | — | — | — | `native/modules/output/module.cpp:15-33` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/output/module.cpp:1-14`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/output/module.cpp, native/modules/output/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/output/module.cpp:1-33`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/output/module.cpp and native/modules/output/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## scale_offset

**1. Identity.** `essentials.scale_offset`; display name from `native/modules/scale_offset/module.json:1`; source `native/modules/scale_offset/module.cpp:38-46`, `native/modules/scale_offset/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `control` | CV | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |
| output 0 | `control` | CV | n/a | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `scale` | scale | -2 | 2 | 1 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |
| 1 | `offset` | offset | -1 | 1 | 0 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/scale_offset/module.cpp:38-46`; `native/modules/scale_offset/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/scale_offset/module.cpp:1-37`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/scale_offset/module.cpp, native/modules/scale_offset/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/scale_offset/module.cpp:1-46`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/scale_offset/module.cpp and native/modules/scale_offset/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## slew

**1. Identity.** `essentials.slew`; display name from `native/modules/slew/module.json:1`; source `native/modules/slew/module.cpp:56-62`, `native/modules/slew/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `control` | CV | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |
| output 0 | `control` | CV | n/a | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `rise` | rise | 0 | 10 | .1 | s/unit / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |
| 1 | `fall` | fall | 0 | 10 | .1 | s/unit / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/slew/module.cpp:56-62`; `native/modules/slew/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/slew/module.cpp:1-55`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/slew/module.cpp, native/modules/slew/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Not in named ABI 2 set (ADSR/LFO/VCA/VCF/VCO: `native/modules2/adsr/dsp.h:5-8`; `native/modules2/lfo/dsp.h:5-8`; `native/modules2/vca/dsp.h:5-8`; `native/modules2/vcf/dsp.h:5-8`; `native/modules2/vco/dsp.h:5-10`). ABI 2 differences **UNKNOWN** until port exists.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/slew/module.cpp:1-62`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/slew/module.cpp and native/modules/slew/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## vca

**1. Identity.** `essentials.vca`; display name from `native/modules/vca/module.json:1`; source `native/modules/vca/module.cpp:31-51`, `native/modules/vca/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |
| input 1 | `gain_cv` | CV | unity (`native/modules/vca/module.cpp:24-30`) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |
| output 0 | `audio` | audio | n/a | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `gain` | gain | 0 | 2 | 1 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/vca/module.cpp:31-51`; `native/modules/vca/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/vca/module.cpp:1-30`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/vca/module.cpp, native/modules/vca/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Ported. `native/modules2/vca/dsp.h:6-9`: IDs, order, gain range/default retained; poly/constant/SIMD implementation differs (**cosmetic** at interface, sound equivalence UNKNOWN) (`native/modules2/vca/dsp.h:11-19`; `native/modules/vca/module.cpp:24-34`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/vca/module.cpp:1-51`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/vca/module.cpp and native/modules/vca/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## vcf

**1. Identity.** `essentials.vcf`; display name from `native/modules/vcf/module.json:1`; source `native/modules/vcf/module.cpp:85-91`, `native/modules/vcf/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| input 0 | `audio` | audio | UNKNOWN (engine buffer or process fallback not fully traced) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| output 0 | `low12` | audio | n/a | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| output 1 | `low24` | audio | n/a | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `cutoff` | cutoff | 20 | 20000 | 1000 | Hz / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |
| 1 | `resonance` | resonance | 0 | .95 | 0 | none / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/vcf/module.cpp:85-91`; `native/modules/vcf/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/vcf/module.cpp:1-84`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/vcf/module.cpp, native/modules/vcf/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Ported. `native/modules2/vcf/dsp.h:6-10`: IDs, order, ranges/defaults retained; log taper descriptor and per-voice coefficient caching differ (**cosmetic** descriptor and behaviour pending equivalence tests) (`native/modules2/vcf/dsp.h:10-31`; `native/modules/vcf/module.cpp:63-87`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/vcf/module.cpp:1-91`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/vcf/module.cpp and native/modules/vcf/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## vco

**1. Identity.** `essentials.vco`; display name from `native/modules/vco/module.json:1`; source `native/modules/vco/module.cpp:43-61`, `native/modules/vco/module.json:1`.

**2. Ports** (per-direction zero-based index; ABI 1 `WS_CONTROL` is labelled CV unless process establishes gate semantics).

| Direction/index | ID | Signal kind | Unpatched default | Source |
|---|---|---|---|---|
| output 0 | `audio` | audio | n/a | `native/modules/vco/module.cpp:43-61`; `native/modules/vco/module.json:1` |

**3. Parameters** (ID mapping: `module.json`; bounds/defaults/units: C++ descriptor).

| Index | ID | Name | Min | Max | Default | Units/taper | Stepped | Source |
|---|---|---|---:|---:|---:|---|---|---|
| 0 | `frequency` | frequency | .01 | 20000 | 220 | Hz / UNKNOWN taper | UNKNOWN (not encoded in ABI 1 descriptor; inspect event handler) | `native/modules/vco/module.cpp:43-61`; `native/modules/vco/module.json:1` |

**4. State.** Runtime state and reset/prepare/event/process implementation: `native/modules/vco/module.cpp:1-42`. Extra serialized state/version beyond parameter values: **UNKNOWN (looked in native/modules/vco/module.cpp, native/modules/vco/module.json; patch document saving and loading not fully audited)**.

**5. ABI 2 status.** Ported. `native/modules2/vco/dsp.h:6-11`: existing frequency and audio output retained; new pitch, FM and notes inputs (**ID-breaking** port layout). Frequency mode, note handling and polyphony differ (`native/modules2/vco/dsp.h:20-30`; `native/modules/vco/module.cpp:24-44`). Classification **range/default change:** none found for retained parameters. Full behavioural equivalence **UNKNOWN**.

**6. Port notes.** DSP, lifecycle and runtime assumptions: `native/modules/vco/module.cpp:1-61`. Third-party dependencies, pinned revisions and licence: **UNKNOWN (looked in native/modules/vco/module.cpp and native/modules/vco/module.json; DESIGN.md not comprehensively audited)**. Process-time allocation, locks and mutable globals: **UNKNOWN (requires full audit)**.

## Summary

| Module | ABI 2 ported? | ID-breaking difference | Risk (planning) | Source |
|---|---|---|---|---|
| adsr | Yes | New gate/notes ports | high | `native/modules/adsr/module.json:1`; `native/modules2/adsr/dsp.h:5-15` |
| clock | No (named set) | UNKNOWN | high | `native/modules/clock/module.json:1` |
| function_envelope | No (named set) | UNKNOWN | med | `native/modules/function_envelope/module.json:1` |
| ladder | No (named set) | UNKNOWN | high | `native/modules/ladder/module.json:1` |
| lfo | Yes | New sync/beats parameters | high | `native/modules/lfo/module.json:1`; `native/modules2/lfo/dsp.h:5-15` |
| mixer | No (named set) | UNKNOWN | med | `native/modules/mixer/module.json:1` |
| noise | No (named set) | UNKNOWN | high | `native/modules/noise/module.json:1` |
| output | No (named set) | UNKNOWN | med | `native/modules/output/module.json:1` |
| scale_offset | No (named set) | UNKNOWN | med | `native/modules/scale_offset/module.json:1` |
| slew | No (named set) | UNKNOWN | med | `native/modules/slew/module.json:1` |
| vca | Yes | None found | med | `native/modules/vca/module.json:1`; `native/modules2/vca/dsp.h:5-15` |
| vcf | Yes | None found | med | `native/modules/vcf/module.json:1`; `native/modules2/vcf/dsp.h:5-15` |
| vco | Yes | New pitch/FM/notes ports | high | `native/modules/vco/module.json:1`; `native/modules2/vco/dsp.h:5-15` |

## Open questions for Claude

1. Confirm how new ADSR gate/notes and VCO pitch/FM/notes ports map from numeric ABI 1 connections into stable string IDs (`native/modules/adsr/module.cpp:94-108`; `native/modules2/adsr/dsp.h:7-10`; `native/modules/vco/module.cpp:43-52`; `native/modules2/vco/dsp.h:7-11`).
2. How should LFO sync/beats be defaulted for migrated patches? (`native/modules/lfo/module.cpp:54-60`; `native/modules2/lfo/dsp.h:10-15`).
3. Where exactly does `native/document/` persist unknown patch fields and module-private state? UNKNOWN (document path not comprehensively audited); see `docs/specs/ABI2_PLAN.md` section 5 B4.
4. Confirm all unpatched input buffer defaults using graph and module execution rather than descriptor inference: UNKNOWN (graph source not audited).
5. Verify all 12 DESIGN.md source licence and pinned revision records: UNKNOWN (DESIGN.md set not audited).
6. Confirm whether the ABI 1 noise RNG, clock phase, and envelope state should be serialized during migration (`native/modules/noise/module.cpp:23-32`; `native/modules/clock/module.cpp:21-36`; `native/modules/adsr/module.cpp:7-30`).
