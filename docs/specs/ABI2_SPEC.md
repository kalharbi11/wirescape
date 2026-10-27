# Wirescape module contract 2 — draft specification

**Status:** design draft for Track B1. This document specifies an exposed target contract; it does not claim that every item is implemented. ABI 1 remains unchanged. The accompanying `patch_v2_draft.schema.json` and example are draft format artifacts and are not implemented, loaded, or applied by the application.

Normative terms **MUST**, **MUST NOT**, **SHOULD**, and **MAY** express requirements for a conforming ABI 2 implementation. Unless an item is called current below, treat it as target/future behavior. The current implementation subset is summarized after the requirements.

## D1 — Signal representation and block rules

Signal buffers MUST contain planar float32 samples and support every frame count from 1 through the prepared maximum, including odd sizes. A signal has 1 through `max_channels` voice channels; each channel pointer MUST meet the declared alignment. A `CONSTANT` signal MUST have identical values throughout the current process span, represented by its first sample. Unpatched signal inputs MUST be constant at their declared unpatched value. Implementations MAY use constant knowledge to avoid per-sample work and MUST NOT set `CONSTANT` when later samples differ.

## D2 — Channels and polyphony

Channels represent voices, never stereo sides. A poly input contributes to the node's channel count; a mono source MUST broadcast to poly consumers. A zero-channel source behaves as unpatched. A mono-declared input MUST apply its declared `SUM` or `FIRST` policy; `POLY` preserves channels. Channel changes MUST be handled without allocation after prepare. A module MAY declare a fixed channel policy. The portable fixed-storage allocator implements Rotate, Reuse, Reset and Mono (last/low/high, legato), oldest stealing, note/source identity, sustain and source release. The notes_to_cv test module exposes pitch/gate/velocity/pressure/timbre and mapped note events. MPE is an interface stub, not implemented channel management. Oscillator/LFO/VCF/ADSR loops are grouped in fours but retain scalar precision-sensitive math; VCA uses f4.

## D3 — Stereo

Stereo MUST use two signal ports linked by stable `stereo_pair` identity. Voice channel count remains independent for left and right. A host MAY render a multichannel output bus; mono-to-stereo routing and bus layouts are host policy. ABI 2's header exposes paired ports and output-channel buffers, but product-level stereo UI/device behavior is a separate implementation item.

## D4 — Units and hints

Hints are descriptive and MUST NOT prohibit an otherwise valid connection. The canonical digital conventions are: audio ±1.0 = full scale; pitch 0 = C4 (261.6256 Hz), 1 = one octave, MIDI note n maps to `(n-60)/12`; bipolar CV −1..+1; unipolar CV 0..1; gate 0/1; trigger is a rising pulse of at least 1 ms; `any` has no asserted range. Gate detection uses Schmitt thresholds: on above 0.5, off below 0.25. Hardware adapters MAY scale hints to physical units (the plan's Eurorack convention is ±5 V for bipolar/audio and +5 V for gate/trigger); the desktop engine MUST keep digital values and MUST NOT silently rescale them.

## D5 — Connection policy

Signal inputs MUST accept at most one source unless a future descriptor explicitly defines another rule. Event inputs MAY accept multiple sources and MUST merge events in stable timestamp/frame order, using a stable tie-break. Outputs MAY fan out. Signal kind and event kind MUST match; signal hints MAY differ and should yield a soft warning only. The present implementation still rejects signal/event-kind mismatches and does not implement the full policy/UI warning surface.

## D6 — Parameters and taper

Each parameter MUST have a stable string ID, display name, unit, finite minimum/maximum/default, taper, optional curve, optional step/options, formatting metadata and flags. Numeric legacy indices MAY be retained for migration. Non-stepped base changes SHOULD use a 5 ms sample-counted linear ramp in normalized parameter space; repeated identical targets MUST NOT restart the ramp. Stepped parameters jump. Modules receive effective parameter signals in plain units after smoothing, modulation and tapering.

The inverse taper normalizes a plain base value: linear `u=(x-min)/(max-min)`, log `u=log(x/min)/log(max/min)`, exponential `u=log1p(((x-min)/(max-min))*expm1(c))/c`, clamped to [0,1]. A degenerate min=max maps to u=0. Linear taper is `x=min+u(max-min)`. Log taper requires `min>0` and is `x=min*(max/min)^u`. Exponential taper with positive finite curve `c` is `x=min+(max-min)*(exp(c*u)-1)/(exp(c)-1)`; for `c=0`, it is linear. Invalid ranges, nonfinite values, or invalid taper metadata MUST be rejected during descriptor validation. A descriptor's `step` quantizes plain values after taper mapping; named options define explicit enum values and MUST be stable.

## D7 — Parameter modulation

Only `MODULATABLE` parameters accept modulation connections. Each cable amount is finite and in [−1,+1]. For each voice/sample, normalize the smoothed base plain value to `base_norm`; interpret each CV sample as bipolar and compute `effective_norm=clamp(base_norm + Σ(amount_i*cv_i),0,1)`, then map through the parameter taper to plain units. The base value MUST remain unchanged. Mono CV broadcasts to poly parameter signals. A parameter lacking `AUDIO_RATE_OK` MAY be sampled at a documented lower rate through an SDK helper; it MUST NOT silently imply audio-rate modulation. Exact pitch/FM behavior SHOULD use explicit musical ports.

## D8 — Events

Events are typed, sorted, offset-indexed records delivered per process call. Event output ports MAY route to downstream modules within the same block where graph order permits, and MUST also be available to the host egress path. Event payload pointers are borrowed only for the current process call and MUST NOT be retained. Messages MUST respect the payload limit (1472 bytes by default) and the host's bounded per-block arena.

The target event set includes NOTE_ON/OFF/CHOKE, note expression, PARAM, PARAM_MOD, transport, MIDI/UMP, and MESSAGE. Note events carry note ID, channel, key, velocity [0,1], and source identity. MIDI storage reserves UMP words. The engine's finite event budget MUST reserve release capacity for NOTE_OFF/CHOKE, coalesce eligible parameter updates latest-wins, and count drops/arena exhaustion. **No implementation may promise lossless NOTE_OFF delivery under unbounded ingress or output pressure.** If a release cannot be admitted to its reserved capacity, the host MUST request a generation change/emergency choke and signal it by counters/acknowledgement; global reset is the final safety mechanism, not a lossless queue guarantee.

Current implementation recognizes the header event records and has bounded queues, release reserve, coalescing, counters, emergency generation reset and acknowledgement. Its emergency response is global reset/choke semantics, not per-note lossless delivery. Stable fan-in uses (absolute frame, producer index, producer serial), with module output ties assigned scheduler traversal serials after ingress. Payload bytes are copied into fixed packets then the block arena. The global budget counts admitted deliveries and emitted events, so fan-out spends additional capacity. PARAM coalesces only on budget pressure for matching destination/index/source/channel. It may remove intermediate automation points. NOTE_ON may drop with a counter. Transport event payload interpretation, MIDI/UMP protocol decoding and external I/O remain reserved.

## D9 — Process context and transport

Every process call MUST provide frame count, absolute frame position, generation and sample rate. Transport MAY be absent. When present it carries tempo in BPM, beat in quarter notes, bar start, time signature, loop bounds, and playing/recording/looping flags. Modules MUST handle absent transport and MUST remain deterministic for a fixed input/event sequence.

## D10 — Feedback

Cycles MAY be admitted only when the host can prepare a deterministic acyclic schedule after selecting back-edges. Edge selection MUST be deterministic from stable node/port identities, independent of insertion order. Each selected signal back-edge receives exactly `feedback_block` frames of delay; loop components process in chunks no larger than that setting. The approved default is 32 frames; valid setting is 1 through the prepared maximum block. Selected event back-edges MUST deliver at source absolute frame plus feedback_block; the fixed future queue carries events crossing the current callback. They do not use float delay buffers. Sample-exact internal feedback belongs inside a module.

At prepare, Tarjan computes SCCs. Nodes sort by numeric ID and edges by (from, output, to, input, kind, amount); DFS marks edges to grey nodes as back-edges. The remaining graph is topologically ordered within each SCC and across condensed SCCs. Loop nodes run in spans at most feedback_block, while other components run the whole callback. Signal and modulation back-edges use persistent circular buffers of exactly feedback_block samples. Persisting the setting in the app remains future work.

## D11 — Memory and resource reporting

Modules MUST allocate processing storage during create/prepare through the host allocator, and MUST NOT allocate, lock, perform I/O, log, throw, or use unbounded work in process/event/reset. A module MUST report fast and large memory requirements; the report MUST be at least the storage actually requested. `FAST` denotes hot/small state and `LARGE` denotes long buffers or sample memory. Host memory limits and placement are adapter policy. Allocation-count tests SHOULD cover processing, event handling, reset, and channel-count changes.

## D12 — State, configuration, and latency

State save/load MUST occur quiescently and use bounded readers/writers. State is module-versioned; unknown future state SHOULD be preserved opaquely by the document/host layer rather than discarded. `query_ports(config)` MAY define configuration-dependent ports; graph replacement MUST be staged, preserve cable endpoints by stable port ID where possible, and report dropped connections. Modules MUST report latency in frames after prepare. The host MUST report accumulated latency along signal paths; automatic compensation is not part of this contract.

## D13 — Threading, ingress, egress, timestamps, generation

Construction, registration, graph/configuration/state changes, reset and destruction are quiescent operations. Each producer handle MUST have exactly one writer; the render thread is the sole processor-state owner. Cross-thread operations are limited to the explicitly documented enqueue, panic, egress-pop, and meter surfaces. Ingress uses bounded SPSC rings per registered producer (target maximum 32), merged within a bounded render budget. Egress is bounded and has a separate release/emergency policy; its capacity is finite.

Frame scheduling supports `Immediate` (next render block) and `Timestamped`. Timestamp conversion uses monotonic nanosecond timestamps and published `(frame_position, callback_time_ns)` anchors. The **current fake-anchor implementation** uses nominal sample-rate conversion initially, then an EWMA (1/16) of measured anchor slope, bounded to nominal ±1000 ppm. Timestamped adds one prepared `max_block` of safety offset, independent of actual callback size. Real driver timestamp accuracy and jitter are unverified. Timestamped scheduling is therefore not a real-device guarantee.

`panic()` MUST advance generation immediately, invalidating stale queued work. Processor reset and `applied_generation` update occur at a render boundary; consumers MUST wait for the acknowledgement before resuming generation-bound input. If a critical release cannot be queued, emergency behavior MUST advance generation and request global reset/choke. Shutdown MUST stop producers and render before destroying queues or modules.

## D14 — ABI, SDK, and language boundary

`module2.h` is the C ABI boundary. A C++17 SDK MAY provide descriptor generation, validation, signal helpers, event dispatch, state helpers, and an exception barrier. DSP code SHOULD depend only on portable value types and MUST keep Qt, OS services, filesystem/network access and STL allocation out of processing callbacks. Adapters are separate host integrations. The user-approved split is MIT for module2.h, the original SDK/template and voice allocator; GPL-3.0-only for engine2 and ports derived from the current GPL modules. Check each source licence before reuse. No third-party DSP was imported in this slice.

## D15 — Numeric and performance policy

float32 is the default. A module using double state MUST document why. Builds MUST NOT globally enable fast-math options that invalidate finite-value guards. Denormal handling MAY use flush-to-zero modes configured by the host. SIMD MUST have a scalar fallback and remain swappable; the approved backend is the project's small f4 wrapper (SSE2, NEON and WS_SIMD_SCALAR), without a new library dependency. NEON/ARM compilation is unverified on this machine. Approximate fast math MUST document error bounds and be opt-in. Random behavior MUST use deterministic per-instance/per-channel seeds and reset reproducibly. Performance claims require representative measured worst-case block time and equivalence evidence.

## D16 — Reserved

D16 is reserved for host-level features that need no ABI change (for example macros, snapshots, flattened subpatches, exports, and script hosting). It is not normative in this document and confers no current implementation status.

## Surface and status summary

The exposed header surface includes channel-aware signals, parameter descriptors, events, transport, host allocation, memory/state callbacks, config-dependent port queries, latency, feedback configuration, ingress/egress queues and generation acknowledgement. The current engine2 slice implements portions of D1–D3, D5–D13; it is experimental and headless. Portable voice allocation and fake-clock drift mapping are implemented. Hardware/plugin adapters, full MIDI semantics, real driver timing validation, patch v2 integration, stereo UI/device behavior and D16 host features remain future work. ABI 1 binaries/contracts are unchanged and are not interchangeable with ABI 2 binaries.

## Experimental limits and draft patch

All exposed structures begin with struct_size. ABI version 2 and ws_module2_entry are exported; both symbol names may be overridden for static linking. Accepted size is at least the current structure size; future trailing fields are ignored, not inferred. SDK prepare caches queried port counts and process rejects mismatched counts. Zero-frame calls are no-ops. Validated finite inputs/parameters/outputs and an exception barrier limit accidental errors; native plugins are trusted code, not isolated processes.

Current engine limits: 16 voice channels, 32 ports/parameters/producers, 1–4096 prepared block frames, 1024 nodes maximum, configurable bounded queues, default event budget1024/release reserve256, payload1472, and 1 MiB quiescent state blob cap. Host FAST/LARGE requests are capped at128 MiB per allocation and64 allocations per node. Default arena is1472 KiB. Channel zero on a source is a quiescent between-block override and freezes that node until reenabled; it is not a note-release command. Bus output sums voices without clipping or gain normalization. Parameters without PER_VOICE can still receive poly modulation in this experimental host; flags describe author intent, not a separate allocator.

Non-PER_VOICE PARAM_MOD targeting and modulation aggregation are experimental; note_id is carried but current parameter targeting uses the explicit voice channel. Adapters must resolve note identity before submitting channel modulation. Default module ports report zero latency; engine currently reports the maximum scheduled forward dependency path across all nodes, includes event-edge dependencies, and excludes feedback recirculation. That is broader than an output-only signal latency calculation and needs review before adapters use it. No latency compensation is implemented.

Patch schema: [patch_v2_draft.schema.json](patch_v2_draft.schema.json); [one example](patch_v2_draft.example.json). Unknown fields are allowed for future preservation. Neither artifact changes the current app document format. Relational checks (unique IDs, cable endpoint existence, feedback_block <= max_block) require a future document validator. Module IDs in the draft must be positive to match engine2.
