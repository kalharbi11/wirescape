<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1.0: host protocol v0 spec (Phase 1, do first, about half a day)

Write `HOST_PROTOCOL.md` in `docs/specs/`: the contract between `wirescape-host` (P1B) and `RealEngine.ts` (P1C). It must be specific enough that P1B and P1C can be built in parallel against it without talking to each other. It follows `docs/PLAN.md` decisions D3–D9 and D12.

**Read first:** `docs/PLAN.md` (Target architecture, Decisions), the UI's `src/engine/EngineClient.ts` and `src/patch/types.ts` (in `kalharbi11/wirescape-ui`, or `apps/studio/` after P1A), `docs/PHASE8_HANDOFF.md` in the UI repo, `native/control/app_engine.h`, `native/document/runtime_patch.h`, `native/control/midi_host.h`. Skim for prior thinking (don't copy wholesale): research PR #217 `docs/research/ENGINE_PROTOCOL.md` §4.

**The spec must define:**
1. **Transport and security:** HTTP static files and `/ws` on 127.0.0.1:<port> (default 7400, overridable). A per-launch token (in the URL fragment or `?t=`, then a cookie), an exact Origin check, max message 2 MiB (same as the patch limit), max 256 queued commands, and the close codes.
2. **Envelope:** `{v:0, id, type, ...}` for commands. Replies are `{v:0, re:id, ok:true, gen}` or `{v:0, re:id, ok:false, code, message, gen}`. Pushes are `{v:0, type:"status"|"telemetry"|"catalogue"|"hello"}`. Error codes extend the UI's `EngineError.code` (keep the existing five; add `invalid`, `busy`, `limit`, `unavailable`).
3. **Commands:** one row per `EngineClient` method (`loadGraph`, `addModule`, `removeModule`, `connect`, `disconnect`, `setParam`, `setModAmount`, `noteOn`, `noteOff`, `panic`, `listAudioDevices`, `selectAudioDevice`, `listMidiDevices`, `setMidiEnabled`, and the optional `setSampleRate`, `setBufferFrames`, `setOsc`), plus `ccMap.set` (D8). Give the payload, which ones are live (event queue) and which go through a graph swap (D5), and the expected latency class.
4. **Ordering and reconciliation:** commands are applied in order. `gen` is the engine graph generation. A refused edit leaves the host graph unchanged; the UI rolls back its own document. On reconnect, the UI re-sends `loadGraph` with its document (the UI is the document owner, D3). Spell out the additive `EngineClient` extension: `onRefusal(cb)` plus how pending optimistic edits are reconciled. It is additive: `FakeEngine` keeps working unchanged.
5. **Hello + catalogue (D9):** protocol version, host version/SHA, capabilities (which optional setters exist), and the module catalogue in the UI's `wirescape_modules.json` shape.
6. **Status (≤10 Hz) and telemetry v0 (≤30 Hz):** map exactly to the UI's `SystemStatus` and `Telemetry`. v0 fills `meters` for Output modules, `heldNotes`, audio state/CPU/dropouts and MIDI activity. Every other field is sent empty (`{}`), and the spec says how the UI displays "not available". It never synthesises values.
7. **Execution view (D3):** how the host fills a missing module `version` and `outputs` routes in its execution copy, and what it refuses (unknown modules or parameters, >64 modules, >128 cables).
8. **Versioning:** `v` bumps only for breaking changes; unknown fields are ignored by both sides.
9. **Test vectors:** a short appendix of 10–15 example exchanges (JSON) that P1B and P1C both turn into tests.

```acceptance
tier: T2
size: <= 450 changed lines, <= 2 files
must_pass:
  - python tools/docs_fresh.py
stops: [ABI header edit, patch format change, LAN binding]
```
Protected path named: `docs/specs/HOST_PROTOCOL*`
