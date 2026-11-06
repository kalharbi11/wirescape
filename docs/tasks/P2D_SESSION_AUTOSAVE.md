<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2D-b (#277): last-session autosave and immediate-reload recovery

**Scope:** Browser-local recovery only. Disk-backed patch history, file browsing and engine behavior are separate work.

## Diagnosis

The four-second autosave ran only when `PatchDoc.dirty`. Selecting a clean
starter, imported document or empty NEW patch marked it clean without
replacing the previous recovery snapshot. A fast reload also lost edits made
before the next interval. Save could mark a document clean before its latest
state reached recovery storage.

## Behavior

- Persist each selected editable document immediately after its synchronous
  `loadPatch` state change, even when the document is clean.
- Flush the live document and patch name synchronously on `pagehide`; retain
  the four-second dirty-document fallback.
- Refresh recovery when a patch is renamed or explicitly saved, including
  the browser export and Electron save paths.
- Store the unchanged `wirescape.autosave.v2` envelope (`{ name, patch }`)
  from `doc.toJSON()`, preserving unknown patch fields.
- Recovery writes must never mark the document clean. Malformed, denied or
  quota-exceeded localStorage is best-effort and must not block editing.
- No PatchDoc, format, bridge, engine, dependency or filesystem changes.

## Tests

`apps/studio/tests/session-autosave.spec.ts` covers clean starter switches,
immediate pagehide/reload after an edit, NEW empty patch, dirty marker and
explicit Save, clean import with unknown data, invalid recovery data, denied
storage and quota-exceeded storage. All browser tests use FakeEngine; no
hardware testing is claimed. A conditional case for deferred #271 belongs
there only after merge.

Full UI validation runs in the existing Web job, which provisions npm, Chromium,
Playwright's recorder and Electron. The native task-acceptance job does not
install that toolchain. Local equivalent: `cd apps/studio && npm run typecheck &&
npm test`; missing recorder support is pending evidence, never a test pass.

```acceptance
tier: T1
size: <= 250 changed lines, <= 4 files
must_pass:
  - bash -c "git diff --check"
  - ci: web job typecheck, production build, full Playwright and desktop Electron suites green
stops: [PatchDoc semantics change, format change, new dependency, test removal, device access]
```
