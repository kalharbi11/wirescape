<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T381 MIDI Learn (#381): bounded transient MIDI CC observation

**Tier:** T2. **Issue:** #381. **Branch/PR:** `task/381-midi-learn` / #390.
**Read first:** `AGENTS.md`, #381 (Keel's normative ruling), `docs/PLAN.md` D3/D8,
`docs/specs/ENGINE_PROTOCOL.md` §§4/7/T16, `native/sidecar/devices.h`,
`native/sidecar/devices.cpp`, `native/sidecar/session.cpp`,
`apps/studio/src/engine/EngineClient.ts`, `apps/studio/src/engine/RealEngine.ts`,
`apps/studio/src/engine/FakeEngine.ts`, and `apps/studio/tests/mock-bridge/`.

**Scope:** Implement only Keel's additive v0 `midiLearn {armed:boolean}` capability,
command and `status.midi.learn` observation. It is transient: no engine Learn
persistence, mapping writeback, CC history/queue, or added wire injection.
Observe CCs through a bounded, lock-free, allocation-free fake/real MIDI
callback path; resolve endpoint identity and expire an uncaptured arm after 30 s
on the control thread. Preserve the existing D8 document ownership; production
Learn UI and `config.rows` editing stay in #287 after #381 merges.

**Verification:** Native fake-MIDI/null-sink protocol tests cover unarmed/armed
status, capture/latest sequence, burst without allocation, rearm/timeout,
removed endpoint, and UMP. Studio mock-bridge and FakeEngine tests cover the
capability and preservation of unknown document keys. Hosted required checks
and an independent Claude cross-family T2 COMMENT review are additional merge
conditions; this task does not require Pi or real devices.

**Self-decision (AGENTS.md allowance):** The complete PR touches 13 files
(including this acceptance task) versus the issue's 10-file budget: +30%, below
the authorized +50% ceiling. The extra surfaces are the golden protocol
conformance fixture, the focused Studio mock-bridge spec, and this now-named
task document. The normative tier, commands, thresholds and stops below are
reproduced verbatim from #381; no acceptance checks are removed or weakened.

```acceptance
tier: T2
size: <= 400 changed lines, <= 13 files
must_pass:
  - ctest --test-dir build/native -R "sidecar_|midi" --output-on-failure --no-tests=error
  - ci: cd apps/studio && npm run build && npx playwright test tests/mock-bridge
stops: [ABI header edit, persisted Learn state in engine, queue/history of CCs, real devices in tests]
```
