<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #732 OSC Out O3: Studio OSC Out card

Read first: `docs/specs/OSC_OUT.md` §1, §2, §5, `apps/studio/AGENTS.md`, the OSC In card/editor (`apps/studio/src/app/OscEditor.ts` and its tests), `apps/studio/src/engine/FakeEngine.ts`.

- An OSC Out card and editor: port, send rate, 8 rows (address, cv/gate, deadband, enable) and the notes row. Validation mirrors spec §2, and the editor never writes an invalid config. A line under the title: "Sends to this computer (127.0.0.1) only".
- Shows `status.osc.out` (state, sent, dropped) when present. On Web it's greyed out with "Desktop and Pi only".
- Unknown config keys survive edit → save → load (Playwright).
- FakeEngine: `osc.out` catalogue entry, `"osc.out"` capability and a simulated status. Build against FakeEngine; no engine needed. Merge order doesn't matter, because the catalogue drift check lands with O1.

```acceptance
tier: T1
size: <= 600 changed lines, <= 8 files
must_pass:
  - bash -c 'cd apps/studio && npm ci && npm test'
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [config shape differs from the spec, LAN/host field added, new dependency]
```

Patch guide impact: deferred to #733.
Signed: Keel, 2026-11-13
