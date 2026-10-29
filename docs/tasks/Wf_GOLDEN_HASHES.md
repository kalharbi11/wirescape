<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-f: golden render hashes (#92)

**Do:** render every `native/examples/*.json` patch through the real app loader
and every built-in module's default mono output (48kHz, 1s, block128, engine seed42).
Assert exact float32 little-endian hashes only between fresh renders within the
same build. Cross-build/platform-family hashes are recorded in reports, never
asserted (planner decision on #92, 2026-10-29). `tools/golden.py check|update`
records evidence; update explains that any future committed baseline update
requires its own PR and reason. CTest `golden_render_hashes` runs check.
No DSP changes or tolerance comparisons. The internal app loader accepts a
seed with its existing default retained; no public ABI changes.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 350 changed lines, <= 40 files (hash files are tiny)
must_pass:
  - ctest --test-dir build/native -R golden_render_hashes --output-on-failure
  - python tools/golden.py check
  - python -m unittest discover -s tools/foundation/tests -p "test_golden.py"
  - ci: linux-arm64, windows-x64, guard green
  - pi: python tools/golden.py check --family arm64
stops: [hash update mixed with a DSP change, tolerance instead of exact hash]
```
