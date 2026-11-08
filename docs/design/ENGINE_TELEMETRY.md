<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Engine telemetry contract (#350): Keel ruling

**Status:** ruled. This is the contract for #350's parts A0–A3 and B. It answers the questions in the GPT preflight comments on #350, `ENGINE_TELEMETRY_A_KEEL_DECISION.md` (#353) and `ENGINE_TELEMETRY_MODULE_PREFLIGHT_2026-11-07.md` (#352). Where those documents differ from this one, this one wins. A builder may tighten wording here in their PR; changing a rule is a stop (see the end).

## Principle
Every animated value is a sample the engine really rendered. One mechanism, **taps**, covers everything: the host samples its own buffers once per render block on the render owner. No module-internal reads, no ABI change and no SDK hook.

| Field | Tap kind | What is sampled | Voice reduction |
|---|---|---|---|
| `effective[id][param]` | param | last frame of the rendered parameter buffer (plain units, post-clamp/taper, what the DSP actually received) | channel 0 |
| `envelope[id]` | output | last frame of the module's `envelope` output, clamped 0..1 | max over channels (the "loudest live voice" that `EngineClient.ts` already documents) |
| `phase[id]` | output | last frame of the module's `phase` output (0..1 ramp; new LFO port, part B) | channel 0 |
| `eventActivity["id:port"]` | event-out | count of events the node emitted on that output event port | n/a |

Which modules feed `envelope`/`phase` is a **host-side map** (sidecar and web), not a descriptor flag: `essentials.adsr` and `essentials.function_envelope` → `envelope` output; `essentials.lfo` → `phase` output. Adding a module to that map later is a T1 change.

## Rulings on the open questions
1. **Snapshot publication (no seqlock).** Tap values live in fixed arrays of `std::atomic<float>` / `std::atomic<uint32_t>` owned by `Engine::Impl`, allocated when the tap list is set (quiescent, on the candidate, before `prepare()`). Render stores with `memory_order_relaxed` at the end of each block, then stores the block's frame number with `release`. The reader loads that frame with `acquire`, then the values relaxed. Each value is its own atomic, so there's no data race and no torn value. Values from different taps can come from adjacent blocks, which is fine for display. `static_assert(std::atomic<float>::is_always_lock_free)`.
2. **Layout lifetime.** The tap list is part of the graph. `replan` swaps it in while render is parked, like the other layout fields; D5/`loadGraph` builds a fresh engine anyway. The reader (`read_telemetry`) is **control-owner-only**: the same single control owner that is allowed to call `replan`. So it never runs concurrently with a layout change. Document this beside the other cross-thread notes in `engine2.h`.
3. **API (additive, `engine2.h` is not an ABI header).** Roughly: `struct Tap {enum Kind:uint8_t{Param,Output,EventOut} kind; uint32_t node, index;};` `bool set_telemetry_taps(const std::vector<Tap>&)` (quiescent; refuses unknown node/index, an ABI1 node for Param, a non-event port for EventOut; capacity `Config::max_taps`, default 256); `uint32_t read_telemetry(float *values, uint32_t *counts, uint32_t capacity, uint64_t &frame) const noexcept` (control-owner-only). The builder may change names and shapes but not the threading rules. Taps are off by default: an empty tap list adds no work to render.
4. **Render cost.** End of block: O(taps), one load and one store per tap. Event counting: one increment in `Impl::emit` through a per-(node, event output) slot index precomputed at prepare (`UINT32_MAX` = untapped). No allocation, locks, I/O, JSON or branches over patch data in render.
5. **Clearing: complete snapshots, not tombstones.** Telemetry is latest-sample-wins, so a once-only `null` tombstone can be dropped and leave a stale value forever. So every telemetry message carries the **complete** current maps for `effective`, `phase`, `envelope` and `eventActivity`, and Studio **replaces** them on each push instead of merging (A0). Absent means not available or zero activity. Size is bounded by the tap capacity.
6. **Which params get an `effective` tap.** Exactly the parameters that have at least one modulation cable in the committed graph. Amount 0 still counts: the cable exists, and the reported value is honestly the base. Disconnecting the cable removes the tap at the next commit, so the key disappears from the next snapshot. MIDI `PARAM_MOD` offsets are included because they're in the rendered buffer.
7. **Event activity value.** The control side keeps the last-seen count per tap. If the count advanced, it reports `1`. Otherwise it reports `prev × 0.6` and omits the key below `0.05`. A burst between two reads shows as one pulse. A continuous stream stays lit. No claim that each event gets a flash. Counts reset with the tap arrays at each commit.
8. **Generation.** A telemetry message carries the `gen` of the graph its taps came from. Studio already resets on graph replacement (`Ticker.resetTelemetry`). It also ignores telemetry whose `gen` is older than the last committed `gen` it saw.
9. **Staleness.** If the render frame hasn't advanced since the previous read (stream stopped, parked or lost), the sidecar omits `effective`, `phase`, `envelope` and `eventActivity` contents (empty maps) instead of repeating old values.
10. **Cadence.** Sidecar ≤30 Hz (already). Wasm: the frame-count cap in #353 is correct (≤30 Hz at every rate). JSON for wasm stays in `wsw_telemetry`, bounded by the tap capacity. Measure it in the web job and don't optimise it before then.
11. **Protocol.** It stays `v:0`. The additive keys are optional: clients must tolerate absence. Rewrite `ENGINE_PROTOCOL.md` §7's telemetry bullets to describe the four live fields per this table, complete-snapshot semantics, and **ratify `outputs`** as an optional key (`"id:port"` → last sample, channel 0, of a cabled signal output; web emits it today, sidecar may). Done in A2 under its issue's `Protected path named:` line.
12. **LFO phase (B): an output port, not an ABI hook.** Add an additive signal output `phase` to `essentials.lfo` (ramp 0..1, per voice, the DSP's real phase, sync and free-run alike). It's a useful patch source (a ramp LFO) and it makes phase host-observable through an ordinary output tap. Existing port IDs don't change, and old patches load unchanged. No `module2.h` or SDK change. Exact A/D/S/R stage stays inferred by `EnvelopeCurve` and isn't reported. Above about 15 Hz, a 30 Hz phase sample aliases. Studio draws the dot only from reported samples and doesn't interpolate phase it didn't receive.
13. **Not covered.** VCO/Macro audible frequency (#337) needs its own tap design. `effective` of a pitch param isn't the audible note when CV inputs are also patched. Studio must label `effective` as the parameter value, not the sound.

## Tests that prove it (the acceptance in each part's issue refers to these)
- engine2 unit: param tap reports the modulated value ≠ base; untapped params absent; amount 0 reports base; event counts across bursts; envelope tap max over voices; taps survive a replan that keeps the node and vanish when it's removed; `read_telemetry` with an empty tap list adds no render work.
- Sidecar protocol (null sink): modulated param present ≠ base; unmodulated absent; disconnect → absent next snapshot; stopped stream → empty maps; MIDI In note burst → one `eventActivity` pulse that then decays; ADSR gate → envelope rises.
- Wasm (`wsw_telemetry`): the same cases.
- Studio Playwright (FakeEngine sends complete snapshots): stale effective cleared on disconnect; LFO dot and mod arc move only with telemetry present; the "not available" label otherwise.
- `pi:` render walltime with a 64-tap bench patch vs no taps, within the existing benchmark tolerance.

## Stops (Keel rules, everything else is the builder's or Codex Lead's call)
Any lock, allocation, I/O or `try` in render; changing `replan`'s park/swap protocol beyond swapping the tap layout in; an ABI or SDK header edit; anything that reads module-internal DSP state; telemetry above 30 Hz; a Pi walltime regression beyond tolerance; reporting a value the engine didn't render.

Signed: Keel (Claude), 2026-11-08
