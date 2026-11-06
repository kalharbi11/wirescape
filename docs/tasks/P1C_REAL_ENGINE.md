<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1C: `RealEngine.ts` + mock host (Phase 1, after P1.0; parallel with P1B)

Implement the UI's `EngineClient` over `docs/specs/HOST_PROTOCOL` (`.md`, written in P1.0) v0, without needing the C++ host to exist yet.

**Read first:** `docs/specs/HOST_PROTOCOL` (`.md`, written in P1.0), the UI's `src/engine/EngineClient.ts`, `FakeEngine.ts`, `src/app/App.ts` (how edits and the synchronous `EngineRefusal` rollback work today), `src/runtime/Ticker.ts`. Work in `apps/studio/` if P1A has merged; otherwise, branch in `kalharbi11/wirescape-ui` and P1A re-imports.

**Build:**
- `src/engine/RealEngine.ts`: WebSocket client, command ids, ack/refuse handling, `gen` tracking, status and telemetry fan-out to the existing subscribers, the catalogue from `hello` replacing the static catalogue (D9), and reconnect with backoff that re-sends `loadGraph` from the current document.
- The additive `EngineClient` extension from the spec (`onRefusal`). `App` rolls back an edit when its async refusal arrives and shows the existing error UI. `FakeEngine` gets a no-op implementation so nothing else changes.
- `App.loadPatch` awaits `loadGraph` and handles its rejection (PHASE8_HANDOFF gap). The editable patch is never lost on refusal or disconnect.
- Engine selection: `?engine=fake|real`. The default is `real` when the page is served with a host token and `fake` otherwise (the dev server and tests stay on fake). A small status chip shows "engine: real / simulated / disconnected".
- Unavailable telemetry fields render as "not available", never as fake motion (spec §6).
- `tests/mock-host/` uses a `ws` (MIT, devDependency, D12) server that speaks the spec, backed by `FakeEngine` logic, plus scripted refusals, delays and disconnects.

**Tests (Playwright, against the mock host):** all spec test vectors; async refusal rolls back (module add, cable, param); a stale reply after reconnect is ignored; disconnect → banner → reconnect restores the patch; `heldNotes` and meters flow; and the existing FakeEngine suite stays green and untouched.

```acceptance
tier: T1
size: <= 1200 changed lines, <= 14 files
must_pass:
  - ci: web job green (typecheck, build, existing Playwright suite + new real-engine/mock-host specs)
stops: [test removal, patch format change, new dependency other than ws]
```
