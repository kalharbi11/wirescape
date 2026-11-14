<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #733 OSC Out O4: patch guide, example patch, receiver script, release notes

Read first: `docs/specs/OSC_OUT.md`, `docs/PATCH_DESIGNER_GUIDE.md` (OSC section), `apps/studio/patches/README.md`, `docs/releases/v1.0.md`.

- `tools/examples/osc_out_receiver.py`: **stdlib only** (socket + struct; no python-osc), binds `127.0.0.1:9001` by default, decodes `f`/`i` messages, prints `address value` lines and a per-second count. Add `--count-seconds N` (prints a JSON summary and exits; O5 uses it) and a forwarding hook comment for serial/I²C. SPDX header, plus a unit test that feeds it encoded bytes (no socket).
- Patch guide: what OSC Out sends, the timing (≤ one block late; ~5 ms at 256 frames), loopback only, rows read channel 0, and the Pi recipe ("run the receiver, load the example, see values").
- Example patch `osc-out-01` (LFO → row1, clock gate → row2, keyboard notes → notes) in the patch library, once O1 merges.
- `docs/releases/v1.0.md`: replace "There is no OSC Out" with the real availability.

```acceptance
tier: T1
size: <= 400 changed lines, <= 7 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [new dependency, non-loopback default, patch-library ID change]
```

Patch guide impact: this PR is the guide update.
Signed: Keel, 2026-11-13
