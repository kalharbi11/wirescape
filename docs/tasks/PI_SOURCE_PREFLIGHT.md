<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #445 Pi P1a: build-only preflight and diagnostics

Parent #436. READY to claim on `task/445-pi-preflight`. Preparation is authorized
now; owner installation follows the Windows test. No owner action is needed to
start implementation. Read the issue's latest claim before editing.

## Read first
`AGENTS.md`, `docs/WORKFLOW.md`, #436 and #445 handoffs, `docs/PI_SETUP.md`,
`docs/PI_RUNNER.md`, `tools/pi/update-and-build.sh`,
`tools/foundation/helpers.py`, `tools/foundation/tests/test_pi_update.py`,
`tools/foundation/tests/test_build_parallel.py`.

## Deliver
- Add dependency-free `tools/pi/preflight.py --check-only`: report Linux ARM64,
  compiler/Git/Python/CMake/Ninja/Node/npm versions, lockfiles, PortAudio runtime,
  Electron shared-library requirements, disk/permissions and stale build caches.
  Reuse existing discovery/pins; document supported maintained Node LTS versions.
  Check-only is read-only, does not fetch or probe network, and never installs,
  launches, checks out a ref, creates reports, or opens audio/MIDI devices.
- Missing requirements yield a nonzero exit and precise owner-managed next steps.
  Do not require Qt for the Electron source app; distinguish legacy runner tests.
  Detect inherited Qt/CMake contamination without changing the user's environment.
  Detect caches from another checkout/architecture; refuse rather than delete.
- Integrate preflight into the existing updater without replacing its driver,
  immutable-ref resolution, dirty-tree guards or locked npm builds. Validate the
  selected revision's tooling after checkout as well, before building. No mutable
  ref may be silently substituted. Preserve the historical positional-ref CLI.
- Default Pi builds to two jobs; validate any explicit override. Keep all build
  assets local. Emit source SHA, tool versions and native/Studio/Desktop stage
  outcomes in sanitized generated logs and a summary. Choose a checkout-local
  ignored output directory. Preserve true child exit codes; stop on first failure
  and name the failing stage/log/next command. No environment/token dump.
- Test check-only zero mutation, missing/wrong tools and architecture, unsupported
  Node, dirty tree, stale caches, native/npm failure, spaces in paths, default
  concurrency and correct success provenance. Use mocks; do not provision tools.
- Update `docs/PI_SETUP.md` with separate check/build/test/install steps and report
  locations. Installation commands stay owner-only and deferred, not automatic.

## Boundaries and evidence
Allowed: updater, new preflight, focused tests, Pi setup docs and this task.
Coordinate updater edits with any newer claim; leave Windows #443 and app #446
files alone. No dependencies, services, workflows, packaging or engine changes.
Independent-session T1 COMMENT review. Hosted checks and exact-head private Pi
evidence are required for build changes. Add `ci/pi` and `ci/pi-accept` to the
draft before its final push; `pi5` alone does not execute the preflight command.
Missing Pi prerequisites remain BLOCKED; mock/cloud success cannot cover them.

Signed: Codex Lead (Codex), 2026-11-08 America/New_York.

```acceptance
tier: T1
size: <= 600 changed lines, <= 8 files
must_pass:
  - python tools/foundation/tests/test_pi_preflight.py
  - python -m unittest discover -s tools/foundation/tests -p test_pi_update.py
  - python -m unittest discover -s tools/foundation/tests -p test_build_parallel.py
  - pi-run: python3 tools/pi/preflight.py --check-only
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the final head
stops: [new dependency, workflow edit, automatic installation, device or SSH access, threshold weakening, deleting owner files]
```
