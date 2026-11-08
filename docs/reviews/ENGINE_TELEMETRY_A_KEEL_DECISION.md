<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #350 / T2: implementation contract for engine-authentic telemetry (Keel decision)

**Status:** requested decision; NOT an approved protocol change or a claim of telemetry working.
**Read with:** issue #350, `docs/reviews/ENGINE_TELEMETRY_MODULE_PREFLIGHT_2026-11-07.md` in PR #352, `docs/specs/ENGINE_PROTOCOL.md`, `native/engine2/engine2.cpp`, and `apps/studio/src/runtime/Ticker.ts`.

## Verified implementation seams

- `engine2` computes the post-clamp, post-taper modulation parameter buffers in the **render owner**. The native `Engine::output_sample` readback is currently **render-thread-only**; it is not a control-thread telemetry accessor.
- `native/sidecar/session.cpp::telemetry()` emits empty `effective`, `eventActivity`, `phase` and `envelope` objects.
- `native/web/web_engine.cpp::wsw_telemetry()` emits `outputs` and `scope` but no genuine host-param/event/internal-phase data. wasm rendering and command handling share the AudioWorklet thread.
- Studio `Ticker.push` **merges** sparse effective updates; disappearance from a later packet cannot clear a stale knob/filter display. `PatchCanvas` reacts to activity threshold crossings.
- The AudioWorklet previously emitted once per 12 × 128 frames (31.25 Hz at 48 kHz and 62.5 Hz at 96 kHz). The minimal implementation in this draft PR now counts frames and caps emission at 30 Hz, tested at 44.1/48/96/192 kHz. This alone does **not** make the four missing telemetry fields live.

## Proposed A contract for Keel

1. **Snapshot ownership:** record data only on the engine render owner, once per completed render block. Publish with a bounded per-engine preallocated **atomic payload** and atomic generation/sequence, using release/acquire and at most two control-thread snapshot retries. Do **not** use a plain-float seqlock: it is a C++ data race even if its sequence detects torn reads. Never read node DSP buffers on the sidecar control thread. Store only fixed slot indices; resolve to string module/parameter IDs and build JSON off the audio callback.
2. **Effective values:** publish actual post-modulation **plain parameter values**, not normalized UI knob positions, only for parameters with connected modulation. Take the last rendered sample of the selected reduced voice. A disconnected/zero-amount modulation needs an explicit invalidation; never report its base knob as an apparently active modulation. Keep `RealEngine`'s unavailable semantics when no truthful value is present.
3. **Proposed clearing semantics:** stay compatible with Studio's sparse-delta consumer by sending `effective: { "42": {"cutoff": null} }` **once** on disconnect/replan/panic and have `Ticker.push` delete that cached key and make `FilterDisplay` fall back to the saved base. Alternative: switch the protocol to **complete sparse snapshots**, replacing the map every tick, with corresponding FakeEngine and Studio consumer migrations. **Keel must choose one**; neither is implemented in this draft.
4. **Event activity:** count real events dispatched by the render owner per **output event port**, including multiple events between telemetry reads. Retain counters in the preallocated snapshot and generate a numeric pulse in [0,1] when a counter advances, returning to 0 in the following report. A monotonically increasing counter plus a control-thread last-seen cursor avoids losing a burst while the UI is at most 30 Hz; no claim that each event gets its own visible flash. Reset counters on graph-generation replacement.
5. **Voice reduction:** an effective value is a single scalar per parameter, but the engine supports polyphony. Suggested policy is **voice 0** (with visible/accurate `effective` reduction semantics documented), not an arbitrary "max" that might flicker between voices. Keel must decide the reduction rule before publishing it as meaningful voice state.
6. **Generation/staleness:** key snapshots by engine graph generation; on a successful `loadGraph`, replan, module removal or ID reuse, clear cached keys before new-generation telemetry. On every telemetry read, verify stable generation and bounded frame freshness; return explicit unavailable values rather than a stale last buffer after a halted stream.
7. **Render budget:** no heap/locks/JSON/syscalls in render. Bounded work in `modulate`/event dispatch and once per block; no iteration over arbitrary patch JSON. Capped array capacities come from prepared graph limits. Keep existing callback load/dropout metrics separate; compare telemetry enabled/disabled Pi5 48k/128f within existing tolerance, and do not claim a Pi pass without actual evidence.
8. **Wasm parity:** the existing worklet is single-threaded, so it may read render-owned snapshots immediately after `wsw_render` without control-thread concurrency, but must use the same effective-value/event definitions. The JSON work in `wsw_telemetry` already runs inside the worklet callback; bound length/work separately from Pi native RT and consider precomputed bounded payload if measured costly.
9. **Wire version:** retain `v:0` command compatibility and add optional keys only after the protected `docs/specs/ENGINE_PROTOCOL.md` revision. Ratify existing optional web `outputs` or explicitly distinguish web-only extensions. Absence means genuinely unavailable, not an animation substitute.

## B split and ownership (not part of A)

- **B1 envelope amplitude:** Essentials ADSR and Function Envelope already expose `envelope` CV output; sample rendered channel level in the render owner and publish a bounded amplitude, not a claimed exact A/D/S/R stage.
- **B2 LFO phase:** `phase[voices]` is private to the Essentials LFO DSP. A generic C++ SDK helper alone cannot cross the host ABI. Options: (a) optional, versioned host-visible display-tap ABI; (b) a tightly scoped built-in Essentials observer with no external-module ABI; (c) decline phase until a real observer exists. Do not infer phase from sine output (ambiguous at zero depth, polyphony and sync).
- **OSC-3 / #337:** an effective pitch-parameter value is not the VCO/Macro's true audible note when independent CV inputs are active. Exact observed frequency requires a separately defined DSP/host tap.

## Precise decisions requested from Keel

1. **A:** atomic snapshot shape, bounded retries and lifecycle on live replan; choose tombstones vs complete sparse snapshots; choose voice reduction and event activity semantics.
2. **Protocol:** add an issue-specific `Protected path named: \u0060docs/specs/ENGINE_PROTOCOL.md\u0060` authorization and decide additive wire-version treatment for `outputs` and nullable effective keys.
3. **B:** approve output-tap B1 first and choose built-in observer vs optional ABI for B2. Defer any ABI edit until a separate migration note and explicit approval.
4. **Gate:** T2 requires genuinely independent Claude/Keel cross-family review, sidecar null-sink and wasm tests, Studio tests and actual Pi performance evidence.

**Already implemented in this draft:** only the wasm 30-Hz cap and its audio-frame simulation test. All other items remain architecture proposals; no fabricated effective, event, envelope or phase readings.
