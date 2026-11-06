<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1C: `RealEngine.ts` + mock bridge (Phase 1, after P1.0; parallel with P1B and P1F)

Implement the UI's `EngineClient` over the preload bridge defined in the engine protocol spec (`docs/specs/`, from P1.0), without needing the C++ engine or the Electron shell to exist yet.

**Read first:** the engine protocol spec, the UI's `src/engine/EngineClient.ts`, `FakeEngine.ts`, `src/app/App.ts` (edits and today's synchronous `EngineRefusal` rollback), `src/runtime/Ticker.ts`, `src/system/` (Settings panels). Work in `apps/studio/` if P1A has merged; otherwise branch in `kalharbi11/wirescape-ui`, and P1A re-imports.

**Build:**
- `src/engine/RealEngine.ts`: uses `window.wirescape` (the bridge) for command ids, ack/refuse, `gen` tracking, status and telemetry fan-out to the existing subscribers, the catalogue from `hello` replacing the static catalogue (D9), real `devices` pushes feeding the Settings lists, and a re-sent `loadGraph` after `app.engineExited` + restart.
- The additive `EngineClient` extension from the spec (`onRefusal`). `App` rolls back an edit when its async refusal arrives and uses the existing error UI. `FakeEngine` gets a no-op implementation.
- `App.loadPatch` awaits `loadGraph` and handles rejection; the editable patch is never lost.
- Engine selection: `RealEngine` when `window.wirescape` exists (inside the app), `FakeEngine` otherwise (dev server, tests, plain browser). A small status chip shows "engine: real / simulated / restarting".
- Unavailable telemetry fields show as "not available", never as fake motion.
- `tests/mock-bridge/`: an injected fake `window.wirescape` (a Playwright init script) that speaks the spec, backed by `FakeEngine` logic, plus scripted refusals, delays and engine exits. No new dependency.

**Tests (Playwright, mock bridge):** the spec test vectors; async refusal rolls back module add, cable and param; a stale reply after restart is ignored; engine exit → banner → restart restores the patch; real device lists appear in Settings and in the MIDI In dropdown (with P1G); meters and `heldNotes` flow. The existing FakeEngine suite stays green and untouched.

```acceptance
tier: T1
size: <= 1100 changed lines, <= 14 files
must_pass:
  - ci: web job green (typecheck, build, existing Playwright suite + new mock-bridge specs)
stops: [test removal, patch format change, new dependency]
```
