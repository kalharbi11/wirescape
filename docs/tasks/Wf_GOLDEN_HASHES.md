<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-f: golden render hashes (#92)

**Do:** render every `native/examples/*.json` patch through the real app loader
and every built-in module's default mono output (48kHz, 1s, block128, engine seed42).
Assert exact float32 little-endian hashes only between fresh renders within the
same build. Cross-build/platform-family hashes are recorded in reports, never
asserted (planner decision on #92, 2026-10-29). `tools/golden.py check|update`
records evidence; event-only modules hash their canonical default event stream
(without pointers, padding or wall-clock timestamps), never a fabricated audio port.
Update explains that any future committed baseline update
requires its own PR and reason. CTest `golden_render_hashes` runs check.
Previously authorized line cap420 and file cap40 are retained; the refreshed
diff removes obsolete loader/API changes and stays below the original350 lines.
No DSP changes or tolerance comparisons. The existing app/runtime loader and APIs stay unchanged; the offline renderer
sets seed42 on its private normalized document copy, never a saved patch.

Standing #29 item5 moves QtCore-dependent validation from the clean headless
acceptance job to required Windows CI, which installs Qt and executes the exact
`golden_render_hashes` and `golden_event_stream` CTests through the full suite.
The first test invokes
`tools/golden.py check`. Verify it was discovered and passed; zero tests is not
evidence. Run both commands locally too when QtCore is available. No test/bar is
removed; Linux headless and Python tests remain required.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 420 changed lines, <= 40 files
must_pass:
  - python tools/dev.py build --headless
  - ci: windows-x64 logs explicitly list golden_render_hashes and golden_event_stream as Passed; the first executes tools/golden.py check; aggregate check status or zero tests alone is not evidence
  - python -m unittest discover -s tools/foundation/tests -p "test_golden.py"
  - ci: linux-arm64, windows-x64, guard green
  - pi-run: python tools/golden.py check --family arm64
  - pi-run: ctest --test-dir build/native -R "^golden_(render_hashes|event_stream)$" --output-on-failure --no-tests=error
stops: [hash update mixed with a DSP change, tolerance instead of exact hash]
```
