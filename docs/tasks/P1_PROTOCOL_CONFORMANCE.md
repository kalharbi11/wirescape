<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1H (#250): protocol conformance (T2)

The normative contract is `docs/specs/ENGINE_PROTOCOL.md` v0. The source of
test vectors is `tests/protocol/golden_v0.ndjson`, never separately authored
native and renderer expectations.

**Read first:** `docs/specs/ENGINE_PROTOCOL.md`, `apps/studio/AGENTS.md`,
`apps/studio/src/engine/RealEngine.ts`, `apps/studio/tests/mock-bridge/`,
`docs/tasks/P1B_ENGINE_SIDECAR.md`.

## Work

1. Derive one shared transcript from protocol T01–T15; preserve the explicit
   `>` stdin, `<` stdout and `@` Electron-only direction markers.
   The `session:"*"` placeholder matches the live process token;
   `gen:"+1"` requires exactly one committed graph generation increment,
   and `gen:"="` requires the previous generation. Diagnostic wording and
   measured time may vary. Do not edit the transcript to fit an implementation.
2. Studio Playwright checker drives `RealEngine` and receives all covered
   responses from the fixture. Check request correlation, refusal and rollback,
   load namespace, live vs swap, device IDs, notifications, restart and stale
   sessions. Raw malformed stdin, queue injection and CC-specific wire entry
   points without an `EngineClient` method belong in the native checker.
3. After #247 lands, create `native/sidecar/golden_tests.py` and register
   `sidecar_golden` in CTest. Read the same fixture; exercise stdin/stdout
   with null audio and fake MIDI, parse with duplicate-key rejection, cap
   lines, reject 257th accepted command as busy, check EOF code 0.
4. For any specification/implementation disagreement, post `Keel:` on
   #250 with a minimal diff. The specification wins; do not loosen thresholds.

No network sockets or hardware devices. No new dependencies.

```acceptance
tier: T2
size: <= 500 changed lines, <= 5 files
must_pass:
  - cd apps/studio && npm test -- --grep "golden v0"
  - ctest --test-dir build/native -R sidecar_golden --output-on-failure --no-tests=error
  - pi: ctest -R sidecar_golden (null audio, fake MIDI)
stops: [protocol change, new dependency, device access, weakening checks, oversized diff]
```
