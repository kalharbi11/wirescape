<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC closeout: combined end-to-end check, CONTROLLERS.md, close J5 (#672, parent #72)

**Start when** #645 (row enable, merged) **and** #659 (unknown keys + `config.version`) are both on `main`. Work from `main`; this packet adds tests and docs, not features. If a check fails, fix it here only if the fix is small and inside the OSC files; otherwise open a follow-up issue for the owning slice and say so in the handoff.

Read first: `native/modules2/osc_in/README.md`, `docs/PATCH_DESIGNER_GUIDE.md` (OSC section), `docs/specs/ENGINE_PROTOCOL.md` §7 `status.osc.routes`, `native/sidecar/protocol_tests.py` (existing OSC loopback tests), `apps/studio/tests/osc-*.spec.ts`.

## What to check (one test each, added where it's missing)
1. **Save → reload:** a patch with edited rows, one row disabled, and an unknown future key keeps all of them after save and reload (Studio with FakeEngine, and the native document round-trip).
2. **Presets:** "Reset to Pico defaults" and "Clear" give the documented rows; Clear means `n of 8 rows active` = 0 and no socket is bound.
3. **Real loopback ingress** (`--null-audio`, 127.0.0.1 only): a message to an enabled row moves its output and appears in Monitor; a disabled row's message is dropped; an unmatched message appears in Monitor only.
4. **Truthful status:** `status.osc.routes` and the Studio card agree for enabled, disabled and idle rows.
5. **`config.version`:** absent or 1 opens; 2, 0, "1", null refuse with the documented message.

## Docs
- New `docs/CONTROLLERS.md` (owner-facing, ≤ ~2 KB): how to point a controller (e.g. Pico) at Wirescape over OSC, localhost by default, LAN only with the module toggle and `--confirm-lan`; the row/address table; what Monitor shows; known limits. Link it from `docs/PATCH_DESIGNER_GUIDE.md`.
- Then close #72 with a comment listing slices A–D, #659 and this PR. #94 (fuzzing) runs separately and does not block closing #72.

```acceptance
tier: T1
size: <= 400 changed lines, <= 8 files
must_pass:
  - ctest --test-dir build/native -R "abi2_sidecar_protocol_tests|osc" --output-on-failure
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [new OSC feature or config key, LAN bind or device open in tests, changing defaults or IDs, new dependency]
```

Signed: Keel, 2026-11-12
