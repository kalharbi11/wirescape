<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #435: Macro Oscillator reset and pinned vendor analysis noise
Owner requested checking Groove first, then fixing persistent Macro/vendor warnings.
Groove head8d254cf passes required/native/Pi checks; Static exposes inherited reports.

Read AGENTS.md, issue435/handoff, native/modules2/macro_oscillator/DESIGN.md,
tools/ci/{static_checks,test_static_checks}.py and exact Static log128948170118.
Bounded change: raw dead-storage reset clarification; exact known-vendor diagnostic
baseline and tests. No synthesis changes, source pin updates or blanket suppression.
Stack on #433 / task/430-groove-foundation until it merges; then retarget main.

Review: T2 reporting/baseline policy and protected tools/ci authorization by Keel.
Codex Lead cannot grant allow-protected. Do not merge before that authorization
and cross-family review. Required Linux/Windows/sanitizers and exact Static pass;
Pi native Macro tests required for reset edit. Real hardware is not required for
this storage-lifetime/reporting correction.

Rollback: revert baseline/helper/tests/docs/reset edit together. Original full
diagnostics return. Never alter pinned DSP or weaken render/reference tests.

```acceptance
tier: T2
size: <= 1000 changed lines, <= 8 files
must_pass:
  - python -m unittest tools.ci.test_static_checks -v
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - ci: static
  - pi-run: ctest --test-dir build/native -R 'abi2_macro_oscillator_tests|abi2_macro_reference' --output-on-failure
stops: [new source pin, new dependency, unreviewed baseline expansion, sanitizer suppression]
```
