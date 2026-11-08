<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape Groove Sequencer: curated build roadmap
Status: architecture recommendation and staged build guide, 2026-11-08.
Baseline inspected: main 971d77668c35a49e971e8542e5d97739aa4c19bd.
Recheck current main and latest issue handoffs before implementation. No implementation or platform certification is claimed.

## Lead recommendation
Build one integrated module, proposed ID sequencing.groove, with one module-owned audio-frame musical clock and bounded track players. Two synchronized tracks first; reserve four tracks and 64 steps internally. Structured, versioned pattern configuration is essential. Keep module ID/schema/command design subject to Keel review before making a public contract.

The main risk is host editing/persistence integration, not the cost of two step counters. Start foundation work now. Product/UI layout exploration may proceed independently, but the complete editor follows the proven foundation. No DAW-wide transport or generic sequencer framework is prerequisite.

Preserve example.step_sequencer and its ABI/state/parameter IDs. #401/#405 native and #408/#410 UI are active, independent work; do not edit their files or duplicate their upgrade. #403/#406 Chord and #421 Notes→CV allocation remain separate. Reuse semantics/tests after checking actual merged state, not by overwriting these branches. Preserve #412 curated patches/catalogues.

## Revised phases
These are sequencer-local phases, independent of application Phase 0–5.

| Phase | Deliverable | Compatibility/limit | Gate |
| --- | --- | --- | --- |
| S0 Foundation | One native pattern, live step edit, truthful position, persistence and undo/recovery proof | T2 document/protocol/realtime seam; no graph reload for step edits | Validated edit applies during uninterrupted rendering and survives save/restart/reload; stale/refused edits reconcile |
| S1 Playable v1+ | Two synchronized 32-step tracks, two modulation lanes, short gate choices and practical editing | Fixed 1x speed, one active pattern/track; T1 module/UI on approved T2 seam | Complete musical workflow on Pi; deterministic native and Windows evidence |
| S2 Phrase performance | Eight pattern memories/track, queued selection, copy/duplicate, simple chains | Bounded bank; atomic bulk commits; no independent timing yet | Continuous phrase changes, offscreen editing and persistence without note loss/rebuild |
| S3 Timing and scale | Four tracks, 1–64 lengths, 1/2x/1x/2x, long gates | Per-track phase and note-off deadlines; explicit master boundaries | No drift/stuck notes across mixed lengths/speeds and long gates; measured Pi load |
| S4 Arrangement | Sixteen scene slots, bar durations, per-track pattern selection/mutes, loop/manual queue/end stop | Master-bar scene timeline; initially restart selected patterns at scene boundary | Verse/fill/chorus playback and full arrangement save/reload |
| S5 Separate expansions | Generative sequencers, external sync/app transport, WASM parity, specialized drums, advanced trigs | Each gets its own scope and evidence; not one omnibus release | Feature-specific compatibility/performance/parity proof |

S2 precedes timing expansion because phrase changes make two tracks substantially more useful without multiplying cycle semantics. S3 can split into separately reviewed gate/length and track/speed increments. S5 research for a concrete generative module may begin earlier after S0; it does not depend on song mode and must not destabilize Groove.

## S0: mandatory foundation gate
Prove one small pattern through native loading, continuous sample-accurate rendering, an individual live edit, an applied-state acknowledgment/reconciliation path, truthful telemetry and document save/load/undo. Demonstrate queue-full/refusal, invalid/oversized edits, stale generation/revision, graph replacement and engine restart. An accepted queue submission must not be mistaken for an applied edit; saving during pending edits must have a documented, tested policy.

Keel reviews the smallest additive configuration/edit/telemetry contract. Existing event messages are candidates only if targeting, bounded delivery and concurrency ownership are sufficient. Do not invent an ABI message type without review. Keep JSON parsing/validation/staging off the render thread. No full pattern copies with unbounded callback work.

Decide live-edit audibility before freezing the contract: future triggers use committed content; changes to an already sounding step must not strand its note. Define reset, BPM changes, simultaneous command precedence and missed/deferred UI telemetry. Multiple Groove instances have independent clocks in v1; equal BPM alone does not synchronize them.

Suggested review-sized milestones, not an implementation prescription:
1. Approved configuration/edit/telemetry contract and small loading/rendering proof.
2. Native bounded live edit plus applied/refused/generation behavior.
3. Studio document transaction, undo/redo, save/restart recovery proof using a minimal editor.
Keep integrated protocol changes together when splitting would leave incompatible clients. Each PR states rollback; revert a seam together with its consumers. Never silently discard authored pattern data on rollback.

## S1: expanded first release
Mandatory: two tracks, 32 active sixteenth-note steps each, two 16-step pages, shared BPM and audio-frame clock, looping, rest/note/tie, velocity, pitch/gate/notes and Mod1/Mod2 per track. Modulation samples/holds on note triggers including velocity-zero behavior defined explicitly; rest/tie retain values in v1. Define reset/stop inactive output values, modulation ranges and gate voltage in the contract.

Include short per-step gate choices (0.25/0.5/0.75/1 step), selected-step and page copy/clear/duplicate, basic undo/redo, current-step indication and a single track-selectable editor. These are more useful than exposing unused future tracks. Treat gate choices as a bounded deadline feature with real release tests, not a free UI addition. No overlapping or multi-step gates yet; ties sustain under documented existing semantics.

Mod2 is planned, not merely reserved: it shares scheduling with Mod1. If implementation reveals a new host blocker, record a scoped deferral in the issue and preserve the original two-track minimum; do not quietly mark the expanded scope complete. Gate choices and page operations likewise may be split into an immediate S1b, explicitly reported, rather than holding the original playable release indefinitely. Do not pull 64-step UI, probabilities, ratchets, swing, chords or songs into S1.

Reserve stable per-track IDs tN_pitch/tN_gate/tN_mod1/tN_mod2/tN_notes for N=1..4 after review. Declaring all twenty ports is within limits but is not mandatory: measure idle-buffer/UI costs. If declared, disabled tracks have deterministic inactive outputs and no events. Numeric port slots must remain stable when enabling later tracks.

### Transport contract
At 120 BPM, 32 sixteenth notes = four seconds/two 4/4 bars. Both tracks use that shared boundary in S1.
- Global START while stopped starts armed tracks at step 1 on the next audio scheduling boundary. While running it cancels pending global stop without retriggering.
- Global STOP queues for the next shared boundary, after step 32 completes; second press cancels the queue.
- Track START while globally stopped arms for global start; while running queues step-1 entry at the next shared boundary, without resetting another track.
- Track STOP queues for its cycle end; second press cancels. STOP before a queued entry cancels that entry; START cancels pending track stop without retriggering an already running track.
- Global/track MUTE releases active notes and suppresses note-ons immediately at engine command application, without stopping phase; unmute waits for the next eligible trigger. Effective mute combines global and local mute.
- RESET releases notes, clears queued actions and resets phase; preserve armed/mute intent and running state, with next eligible start explicitly tested. UI command receipt has control-thread latency; browser button presses are not sample-timestamped guarantees.
- At a coincident boundary, reset/release and pending stop take priority over any new note-on. No extra step-1 trigger. Define ordering of multiple same-frame commands in the reviewed contract.
Mute suppresses musical gates/events, not downstream reverb/release tails. Pitch/mod hold/reset behavior must be documented rather than implied to be audio silence.

## Later-phase constraints
S2: explicit active/editing/pending pattern IDs. Initial changes and chain advances use the shared two-bar boundary; choose a deterministic last-request policy. Editing a nonplaying pattern cannot affect the audible one. Bulk copy is staged/validated off-thread and commits atomically. Measure actual serialized banks with full patch overhead against 2 MiB, not just binary storage.

S3: retain a fixed two-bar master cycle independent of track length/speed; track STOP uses its local cycle, track START enters at next master boundary, global STOP uses master boundary and can truncate a long local cycle with safe release. Never wait for an LCM. Commit running length/speed changes at a documented boundary. Long-gate deadlines must survive ties, wrap, edits, mute, stop and retrigger policy; monophonic tracks do not imply overlapping voices.

S4: scene duration uses master bars, not track completion. First version restarts selected patterns and releases prior held notes at scene boundary. Define precedence between manual track actions and scene mutes before implementation.

S5: app-wide/external clock sync requires a separate transport contract. WASM requires the actual edit/telemetry/persistence path and native conformance, not FakeEngine resemblance. Chord routing depends on separately reviewed event consumers/identity; usable CV patches remain independent of it.

## Source-confirmed compatibility at baseline
Links pin evidence; line numbers drift on later main.
- [module2.h lines 17–20](https://github.com/kalharbi11/wirescape/blob/971d776/native/include/wirescape/module2.h#L17): 16 channels, 32 ports, 64 scalar parameters, 1472-byte event payload. The cap64 change is already merged; no new cap expansion.
- [runtime_module_config.h](https://github.com/kalharbi11/wirescape/blob/971d776/native/document/runtime_module_config.h#L9): specific MIDI config adapters; no Groove adapter. A module descriptor alone cannot load its proposed nonempty config.
- [session.cpp](https://github.com/kalharbi11/wirescape/blob/971d776/native/sidecar/session.cpp#L566): scalar setParam and specialized configuration paths; no established Groove live-edit contract.
- [PatchDoc.ts](https://github.com/kalharbi11/wirescape/blob/971d776/apps/studio/src/patch/PatchDoc.ts), [RealEngine.ts](https://github.com/kalharbi11/wirescape/blob/971d776/apps/studio/src/engine/RealEngine.ts): document ownership/client reconciliation integration points, not ready-made structured sequencer editing.
- [session telemetry](https://github.com/kalharbi11/wirescape/blob/971d776/native/sidecar/session.cpp#L725): generic snapshots do not provide the proposed multitrack status contract.
- [runtime_patch.cpp line 37](https://github.com/kalharbi11/wirescape/blob/971d776/native/document/runtime_patch.cpp#L37): runtime loader ceiling of 64 modules/128 connections. [runtime_patch.h line 6](https://github.com/kalharbi11/wirescape/blob/971d776/native/document/runtime_patch.h#L6) and [protocol.h line 14](https://github.com/kalharbi11/wirescape/blob/971d776/native/sidecar/protocol.h#L14): 2 MiB patch/line bounds; pending command limit 256.
Do not confuse the ABI transport structure or Clock implementation with an already wired app-wide song transport.

## Validation and release evidence
Use repository acceptance tasks and exact-head CI; agents choose implementation commands. T2 applies to protocol, format and engine concurrency/realtime changes; ordinary module DSP/UI is T1 on an approved seam. Do not reclassify all native module work as T2. Cross-family T2 review remains required; document decisions here after approval.

For each DSP phase: deterministic Linux ARM64/Windows renders, appropriate sanitizers and concurrency checks, allocation/event overflow/release recovery tests. Across 44.1/48/96 kHz and blocks 1/17/64/128/256, event frames and signal transitions agree. Exercise 100+ loops, step32→1, ties/rests, simultaneous reset/release, queued controls and rapid commands; verify native refusals do not appear as successful UI edits.

Pi performance: use the private wirescape-pi runner per docs/PI_RUNNER.md, never SSH/devices. Compare exact-SHA baseline/candidate at 48 kHz, blocks64/128/256 with sequencers alone, realistic instruments, both mods and repeated edits. Record callback p50/p95/p99/max, deadline misses, memory, allocations/event overflow with sample counts and method. Use an available runner metric or explicitly record instrumentation gaps; don't invent unsupported certification. Ten-minute paced device-free soaks at major milestones; retain existing app soak requirements. No arbitrary new CPU threshold. Windows claims need Windows evidence.

Release acceptance still needs owner-operated physical Pi audio/control checks. Synthetic runner/FakeEngine/UI success does not certify real audio. No new module may preempt #271 hardware failure fixes.

S1 minimum acceptance: create/edit two 32-step patterns, play synchronized and loop, route pitch/gate/mods and note events appropriately, queue global/track stops and starts, independently mute without losing phase, save/reload and undo without divergence. Expanded S1 acceptance additionally includes Mod2, gate choices and copy/clear. Track minimum and expanded evidence separately.

Signed: Codex Lead (Codex), 2026-11-08.
