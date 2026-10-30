<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows development UI launch freshness (#154)

The development launcher now refuses to start a Studio executable unless the
latest successful GUI build explicitly certified it. Build metadata records
`ui_verified` only after the requested GUI build completes with stable source,
the UI enabled in CMake's cache, a UI marker and the Studio executable present.

`python tools/dev.py launch --config Release` checks the recorded configuration,
stable source provenance, current source fingerprint, the current
`WIRESCAPE_BUILD_UI=ON` cache value and the exact executable SHA-256 in the
recorded binary inventory. Missing/corrupt evidence fails closed. Failed builds
invalidate previous UI certification; headless builds and test-time inventory
refreshes do not certify existing binaries.

On refusal, rerun `python tools/dev.py build --config Release` (replace
`Release` with `Debug` as needed). There is no automatic build. Portable
packages, startup-test, audio readiness and UI test behavior are unchanged.

## Acceptance

```acceptance
tier: T0
budget: one bounded implementation attempt plus one fix
size: <= 240 changed lines, <= 4 files
must_pass:
  - focused Python unittest tests for launcher/build provenance
  - ruff on changed Python files
  - ci: existing Windows Qt build and named offscreen tests green
stops: [protected path, dependency, scope beyond development launcher freshness, budget exceeded]
```
