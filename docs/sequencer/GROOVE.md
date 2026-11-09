<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape Groove Sequencer: curated build roadmap
Status: architecture recommendation and staged build guide, 2026-11-08.
Baseline inspected: main 971d77668c35a49e971e8542e5d97739aa4c19bd.
Recheck current main and latest issue handoffs before implementation. No implementation or platform certification is claimed.

## Lead recommendation
Build one integrated module, proposed ID sequencing.groove, with one module-owned audio-frame musical clock and bounded track players. Two synchronized tracks first; reserve four tracks and 64 steps internally. Structured, versioned pattern configuration is essential. Keep module ID/schema/command design subject to Keel review before making a public contract.

Owner revision: build and check the instrument quickly with stopped editing first. S0 is a small configuration/loading proof inside the S1 delivery effort, not a live-edit infrastructure project. No DAW-wide transport, live-edit protocol or generic sequencer framework is prerequisite.

Preserve example.step_sequencer and its ABI/state/parameter IDs. #401/#405 native and #408/#410 UI are active, independent work; do not edit their files or duplicate their upgrade. #403/#406 Chord and #421 Notes→CV allocation remain separate. Reuse semantics/tests after checking actual merged state, not by overwriting these branches. Preserve #412 curated patches/catalogues.

## Revised phases
These are sequencer-local phases, independent of application Phase 0–5.

| Phase | Deliverable | Compatibility/limit | Gate |
| --- | --- | --- | --- |
| S0 Configuration proof | One native pattern loads, renders, saves/reloads | Structured config adapter still required; use existing stopped apply/load path | Valid/invalid configurations handled safely; persisted values match playback |
| S1 Playable v1+ | Two synchronized 32-step tracks, two mods, practical stopped editing | Fixed 1x speed, one pattern/track; edits apply only after stopped | Stop → edit → apply → start from step 1 works on Pi and Windows |
| S2 Live editing | Individual edits during playback, applied-state reconciliation and undo/recovery | T2 bounded edit/concurrency seam; no graph rebuild for live step edits | Uninterrupted rendering; stale/refused edits reconcile; no stuck notes |
| S3 Phrase performance | Eight memories/track, queued selection and simple chains | Atomic bounded pattern commits; build on S2 | Continuous phrase changes and offscreen editing with persistence |
| S4 Timing and scale | Four tracks, 1–64 lengths, independent speeds and long gates | Explicit master/local boundaries and release deadlines | No drift/stuck notes; measured Pi load |
| S5 Arrangement | Sixteen bar-based scenes with loop/manual queue/end stop | Master-bar timeline; restart selected patterns at boundaries initially | Complete arrangement playback and save/reload |
| S6 Separate expansions | Generative modules, app/external sync, WASM, specialized drums, advanced trigs | Each has its own scope/evidence | Feature-specific compatibility/performance proof |

S2 is the default home for live editing. It may move to S3 by an explicit roadmap update if another bounded module feature is more valuable after owner testing; do not quietly make live editing a first-release dependency. Generative work may start independently after the stopped configuration model is proven; it does not depend on song mode.

## S0/S1: stopped-edit contract
Prove one pattern through validated native configuration loading, sample-accurate rendering and Studio save/load/undo. Then build the two-track instrument. Keep authored patterns as versioned structured config, not scalar parameters.

For the first release:
- Pattern editing is available after global sequencer STOP has actually completed, not merely been queued. Disable pattern edits while playing or stop-queued; live performance START/STOP/MUTE controls remain available.
- Edit the Studio document while stopped. Use an explicit Apply, or a clearly documented apply-on-Start transaction. The existing graph/configuration load path and stop–swap–start behavior are permitted for this milestone.
- Applying a changed patch can interrupt/reset other modules if the existing host load path replaces the graph. Surface that limitation; seamless unrelated audio and held-note continuity across Apply are not S1 promises. Choose the smallest safe existing mechanism.
- Validate before activation. Show pending/refused state truthfully, retain the editable draft on refusal, and never restart a rejected candidate. Start is enabled only after the intended configuration is confirmed loaded.
- Restart armed tracks from step 1 after successful Apply. No phase preservation across edits is required. Release old gates/notes on stop/reload and prevent accidental auto-start or stale note-offs.
- Save/reload, basic undo/redo and engine restart must preserve authored values and make unapplied changes clear. Saving a draft does not mean it is already audible. Loading a patch starts stopped.
- JSON parsing, configuration allocation and graph preparation stay off the render thread. A stopped sequencer alone does not make it safe to mutate an engine graph still rendering downstream tails.

Keel reviews new configuration/schema or transport mechanisms as required; deferring live editing removes its realtime command/concurrency work, not the structured config requirement. Basic truthful native position/state remains necessary; no invented UI playhead.

Suggested milestones: small config/render proof → two-track native instrument/transport → stopped editor and persistence → platform/Pi acceptance. Split into ordinary review-sized PRs; agents choose exact tasks. Each PR states rollback without silently dropping authored data.

Multiple module instances have independent clocks in v1; matching BPM alone does not synchronize instances.

## S2: live-edit follow-up
Add the smallest bounded individual-edit protocol, applied acknowledgments, generation/revision guards, queue-full/refusal recovery, and consistent save/undo/restart behavior. Decide edit audibility for already sounding steps; no stranded notes. Existing events are only candidates if targeting, ownership and bounded delivery suffice. No new ABI message without review. Gate this phase on edits during uninterrupted native rendering without graph replacement. The stopped-edit workflow remains a supported fallback.

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
S3: explicit active/editing/pending pattern IDs. Initial changes and chain advances use the shared two-bar boundary; choose a deterministic last-request policy. Editing a nonplaying pattern cannot affect the audible one. Bulk copy is staged/validated off-thread and commits atomically. Measure actual serialized banks with full patch overhead against 2 MiB, not just binary storage.

S4: retain a fixed two-bar master cycle independent of track length/speed; track STOP uses its local cycle, track START enters at next master boundary, global STOP uses master boundary and can truncate a long local cycle with safe release. Never wait for an LCM. Commit running length/speed changes at a documented boundary. Long-gate deadlines must survive ties, wrap, edits, mute, stop and retrigger policy; monophonic tracks do not imply overlapping voices.

S5: scene duration uses master bars, not track completion. First version restarts selected patterns and releases prior held notes at scene boundary. Define precedence between manual track actions and scene mutes before implementation.

S6: app-wide/external clock sync requires a separate transport contract. WASM requires the actual edit/telemetry/persistence path and native conformance, not FakeEngine resemblance. Chord routing depends on separately reviewed event consumers/identity; usable CV patches remain independent of it.

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

Pi performance: use the private wirescape-pi runner per docs/PI_RUNNER.md, never SSH/devices. Compare exact-SHA baseline/candidate at 48 kHz, blocks64/128/256 with sequencers alone, realistic instruments and both mods. S1 tests repeated stop/apply/start; S2 adds repeated edits during playback. Record callback p50/p95/p99/max, deadline misses, memory, allocations/event overflow with sample counts and method. Use an available runner metric or explicitly record instrumentation gaps; don't invent unsupported certification. Ten-minute paced device-free soaks at major milestones; retain existing app soak requirements. No arbitrary new CPU threshold. Windows claims need Windows evidence.

Release acceptance still needs owner-operated physical Pi audio/control checks. Synthetic runner/FakeEngine/UI success does not certify real audio. No new module may preempt #271 hardware failure fixes.

S1 minimum acceptance: create/edit two 32-step patterns, play synchronized and loop, route pitch/gate/mods and note events appropriately, queue global/track stops and starts, independently mute without losing phase, edit while stopped, apply/restart from step 1, and save/reload/undo without document divergence. Live edits, uninterrupted Apply and preservation of unrelated graph state are explicitly deferred to S2. Expanded S1 acceptance additionally includes Mod2, gate choices and copy/clear. Track minimum and expanded evidence separately.

Signed: Codex Lead (Codex), 2026-11-08.
