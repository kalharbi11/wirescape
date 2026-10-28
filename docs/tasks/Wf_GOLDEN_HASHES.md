<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-f: golden render hashes (#92)

**Do:** `goldens/<name>.sha256` for every patch in `native/examples/` and each module's default render (mono, 48 kHz, 1 s, block 128, seed 42), produced by the offline render path (`python tools/dev.py render`, or `wirescape play` without `--confirm-audio` once J3 lands). `tools/golden.py check|update`: check compares float32 output hashes per platform family (hashes may differ between x64 and ARM64; store both); update writes new files and prints why it must be its own PR. CTest `golden_render_hashes` runs check. A hash change needs a reason line in the PR.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 350 changed lines, <= 40 files (hash files are tiny)
must_pass:
  - ctest --test-dir build/native -R golden_render_hashes --output-on-failure
  - python tools/golden.py check
  - ci: linux-arm64, windows-x64, guard green
  - pi: python tools/golden.py check --family arm64
stops: [hash update mixed with a DSP change, tolerance instead of exact hash]
```
