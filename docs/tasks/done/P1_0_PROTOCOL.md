<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1.0 (#228): engine protocol v0 spec (Phase 1, do first, about half a day)

Write `ENGINE_PROTOCOL.md` in `docs/specs/`: the contract between `wirescape-engine` (P1B), the Electron main process (P1F) and `RealEngine.ts` (P1C). It must be specific enough that those three can be built in parallel against it. It follows `docs/PLAN.md` decisions D1, D3–D9.

**Read first:** `docs/PLAN.md` (Target architecture, Decisions), the UI's `src/engine/EngineClient.ts` and `src/patch/types.ts` (in `kalharbi11/wirescape-ui`, or `apps/studio/` after P1A), the UI repo's `docs/PHASE8_HANDOFF.md`, `native/cli/play.cpp`, `native/control/app_engine.h`, `native/document/runtime_patch.h`, `native/control/midi_host.h`. Skim, don't copy: research PR #217 `docs/research/ENGINE_PROTOCOL.md` §4.

**The spec must define:**
1. **Transport:** UTF-8 newline-delimited JSON on the engine's stdin (commands) and stdout (replies and pushes); stderr is free-form logs. Max line 2 MiB (the patch limit) and max 256 queued commands. Behaviour on a malformed line (reply `invalid`, keep running) and on stdin EOF (stop audio, exit 0).
2. **Path through the app:** renderer → `window.wirescape.send(obj)` / `onMessage(cb)` (preload, `contextBridge`) → main process → engine stdin, and back. The main process forwards lines unchanged, except for app-level messages it owns (`app.restartEngine`, `app.engineExited`).
3. **Envelope:** commands `{v:0, id, type, ...}`. Replies `{v:0, re:id, ok:true, gen}` or `{v:0, re:id, ok:false, code, message, gen}`. Pushes `{v:0, type:"hello"|"status"|"telemetry"|"devices"}`. Error codes keep the UI's five `EngineError` codes and add `invalid`, `busy`, `limit`, `unavailable`.
4. **Commands:** one row per `EngineClient` method (`loadGraph`, `addModule`, `removeModule`, `connect`, `disconnect`, `setParam`, `setModAmount`, `noteOn`, `noteOff`, `panic`, `listAudioDevices`, `selectAudioDevice`, `listMidiDevices`, `setMidiEnabled`, plus the optional setters) and `ccMap.set` (D8). For each: payload, live (event queue) or swap (D5), and latency class.
5. **Real devices:** `listAudioDevices` returns PortAudio stereo outputs (stable id = name; index is informational). `listMidiDevices` returns libremidi inputs with `name` and `system_name` (as `play --list-midi` prints). Selection is persisted by the engine in `user_data/settings.json`. Device changes are pushed (`devices`). The `midi.in` module's `config.endpoint` options are defined here ("all enabled" or one endpoint), for the P1G dropdown.
6. **Ordering and reconciliation:** commands apply in order, and `gen` is the graph generation. A refusal leaves the graph unchanged and the UI rolls back. After an engine restart, the UI re-sends `loadGraph` (the UI owns the document, D3). Define the additive `EngineClient` extension (`onRefusal(cb)` plus pending-edit reconcile) so `FakeEngine` keeps working unchanged.
7. **Hello:** protocol version, engine version/SHA, capabilities (which optional setters exist), the module catalogue in the UI's `wirescape_modules.json` shape, and the current audio/MIDI state.
8. **Status (≤10 Hz) and telemetry v0 (≤30 Hz):** map exactly to the UI's `SystemStatus` and `Telemetry`. v0 fills audio state/device/CPU/dropouts, MIDI activity, `meters` for Output modules and `heldNotes`. Every other field is sent empty, and the spec says how the UI shows "not available". Values are never synthesised.
9. **Execution view (D3):** filling a missing `version` and `outputs` in the engine's copy only, and the refusals (unknown modules or parameters, >64 modules, >128 cables).
10. **Startup flags:** `--confirm-audio`, `--confirm-midi`, `--device`, `--block`, `--rate`, `--settings <file>`, `--null-audio` (the test mode, paced by a steady clock, no device opened).
11. **Test vectors:** an appendix of 12–15 example exchanges that P1B and P1C both turn into tests.

```acceptance
tier: T2
size: <= 450 changed lines, <= 2 files
must_pass:
  - python tools/docs_fresh.py
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
stops: [ABI header edit, patch format change, network socket in the design]
```
Protected path named: `docs/specs/ENGINE_PROTOCOL*`
