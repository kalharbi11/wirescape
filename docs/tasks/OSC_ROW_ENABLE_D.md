<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC slice D: per-row enable, Studio toggle, real "Clear" (#612, part of #72)

Today all eight `osc.in` rows always listen (default `/pico1/knob/1`…`8`), so the C1 editor (#603) can only "Reset to Pico defaults". This packet adds an additive per-row flag, end to end. **T2** (module config schema and spec). Follows Keel's ruling on [#603](https://github.com/kalharbi11/wirescape/issues/603#issuecomment-6790997010). Start after C1 #603 merges.

Read first: `native/modules2/osc_in/{config.h,README.md,tests.cpp}`, `native/document/runtime_module_config.h` (OSC section, ~lines 160–225), `native/control/osc_host.cpp`, the C1 editor in `apps/studio`, `docs/specs/ENGINE_PROTOCOL.md` §7 `status.osc.routes`.

## What to build
1. **Native config:** each row gets an optional `enabled` (bool, default `true`, so existing patches are unchanged). `runtime_module_config.h` accepts the key. A disabled row matches nothing: its output holds at `output_min`, or follows `stale` (pick one, document it in the README, and test it).
2. **Route status:** disabled rows report `state:"disabled"` in `status.osc.routes` (or are omitted; pick one). Add that one line to the spec. Protected path named on #612.
3. **Studio:** an on/off toggle per row in the editor. The "Clear" preset (all rows disabled) comes back. The card status shows "n of 8 rows active".
4. **Docs:** update `native/modules2/osc_in/README.md` and the OSC section of `docs/PATCH_DESIGNER_GUIDE.md`.

## Tests
- Native: a disabled row ignores matching messages; a missing `enabled` field means enabled; round-trip through `runtime_module_config`.
- Protocol (`--null-audio`, loopback 127.0.0.1): route status for a disabled row.
- Playwright with FakeEngine: the toggle round-trips, Clear disables all rows, and an old patch without `enabled` opens with all rows on.

```acceptance
tier: T2
size: <= 850 changed lines, <= 22 files (Keel 2026-11-12)
must_pass:
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [changing existing defaults or IDs, ABI header edit, LAN bind or device open in tests, new dependency]
```

Signed: Coda (Claude), 2026-11-11
