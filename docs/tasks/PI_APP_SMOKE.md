<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #446 Pi P1b: isolated Electron profile and device-off app smoke

Parent #436; related #235 and #386. READY for implementation on
`task/446-pi-app-smoke`. #445 is not a coding dependency. Final integration must
exercise both. Installation remains after the owner's Windows test.

## Read first
`AGENTS.md`, `docs/WORKFLOW.md`, latest #436/#386/#446 handoffs,
`apps/desktop/DESIGN.md`, `apps/desktop/main.ts`,
`apps/desktop/tests/app-e2e.spec.cjs`, `apps/desktop/playwright.config.cjs`,
`apps/desktop/package.json`, `tools/pi/update-and-build.sh`, `docs/PI_RUNNER.md`.

## Deliver
- Extend the existing real Electron/preload/native-sidecar test path, never a
  parallel harness. Coordinate shared file edits with #386 before implementation;
  #386 owns additional save/reopen/rollback/reconnect/telemetry behavior. This
  task owns ARM64 execution, isolation and a bounded orchestration entrypoint.
- Verify isolated Electron `userData` AND `sessionData`, native `--settings`,
  window state and default patch directory. Do not assume Chromium's
  `--user-data-dir` changes Electron's app paths. Add an explicit opt-in app
  profile-directory option before readiness if needed; prove reported paths and
  writes stay inside the temporary profile, and default launches retain behavior.
  Do not migrate, rename or delete old profiles/patches or reuse normal settings.
- Supply `python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke`.
  Reuse the checkout's native build and npm lockfiles. Verify real ARM64 engine
  and Electron artifacts on Pi; fail on missing/mixed outputs. Report actual
  source SHA, architecture, versions and build provenance. An optional explicit
  build stage may reuse current build commands; never invoke the updater's fetch
  or checkout during runner acceptance. Existing checkout is the exact PR head.
- Run real UI readiness, native hello/catalogue and command round trip plus clean
  shutdown using null audio and no `--confirm-*`. Verify actual engine argv and
  real-engine mode, not just moving meters/FakeEngine. Fail for skips, zero tests,
  missing metrics, unavailable display or a wrong engine/profile. Use only
  existing fake MIDI seams, with no device enumeration or physical access.
- Bound owned-child lifetime and the entire command below the private bridge's
  1800-second per-command limit. Timeout/failure must clean up only owned children
  and temporary state, retain actionable sanitized logs/traces/screenshots, and
  return nonzero. JSON/Markdown reports distinguish passed/failed/BLOCKED.
- Use a configured display or existing Xvfb; do not install one or alter the
  runner's session. Record backend. Xvfb establishes functional behavior only.
  Missing prerequisites are a setup blocker, never a green skipped test.
  Include mocked isolation, failure, timeout and cleanup regressions in
  `tools/foundation/tests/test_pi_app_smoke.py`; actual UI assertions stay in
  the existing desktop suite, with appropriate profile regression coverage.

## Boundaries and evidence
Allowed: app main/profile integration, existing desktop tests/config, bounded
smoke entrypoint, focused Python tests, Pi setup docs and this task. Coordinate
the common Pi setup docs with #445. Reuse existing Python/Node/Electron/Playwright;
no new dependencies, public/private workflow edits, launcher install, service,
autostart, packaging, SSH, devices or new protocol. If runner changes are required,
post the precise blocker for Keel; finish independent code first.
Independent-session T1 COMMENT review; add `ci/pi` and `ci/pi-accept` before final
push. Require actual unskipped exact-head `pi5/accept` and artifact/report links.
Do not close for cloud-only results. Existing #274 physical-display contention,
#296 long edit soak and #101 baseline are separate requirements.

Signed: Codex Lead (Codex), 2026-11-08 America/New_York.

```acceptance
tier: T1
size: <= 850 changed lines, <= 8 files
must_pass:
  - python tools/foundation/tests/test_pi_app_smoke.py
  - pi-run: python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke
  - ci: existing desktop real-engine tests execute unskipped on Linux, plus guard, linux-arm64, windows-x64 and sanitizers green
stops: [new dependency, workflow edit, new protocol, automatic installation, device or SSH access, counting skipped tests as passed]
```
