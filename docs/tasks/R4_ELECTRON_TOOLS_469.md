<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #469: Electron development tools and Qt-only tooling retirement

R4 can be authored in parallel, but merges only after R3 (#468) and the immutable
`archive/qt-final` tag. This is existing cleanup under D15/D18, not a new feature.
Read first: root `AGENTS.md`, #469 including Codex Lead's inventory approval,
`docs/planner/CLEANUP.md`, `tools/dev.py`, `tools/foundation/helpers.py`, and the
foundation reporting/launch-freshness/build-parallel tests.

## Preserved behavior

Keep doctor/build/test/accept/check and snapshot/report/benchmark/render. The ABI1
benchmark and offline-render baselines stay until R6. Native reports retain source
fingerprints, configuration/cache facts, artifact hashes, unique logs, actual exit
codes and source-change detection. Check remains sequential and stops at the first
failure; functional-only mode still explicitly defers performance evidence.
`helpers.py` and its entire toolchain/private-snapshot/hash/timeout/process-tree/
readiness test file remain unchanged.

Default build compiles native and the existing Electron npm build. Headless build
compiles native only. Electron evidence separately hashes its source inputs,
compiled artifacts and runtime; the native sidecar and ABI2 libraries join the
native artifact inventory. Failed builds invalidate prior desktop certification.
Unchanged native test builds may retain already matching desktop evidence; a
headless build or stale/missing/corrupt evidence cannot certify leftover UI files.

Launch uses the installed Electron executable, the desktop package and an explicit
sidecar path as separate argv entries. It reports only process start, not untested
GUI readiness. No device flag is inferred. With no consent it forwards
`--null-audio`, which disables both audio and MIDI. MIDI-only consent forwards
`--confirm-midi` without `--null-audio`; audio remains unauthorized and uses the
sidecar's null sink. Audio-only and combined consent forward exactly those flags.
All four cases are tested with a mocked child, never real devices.

## Inventory and review gate

The #469 body approves retiring only these Qt-only files:
`tools/foundation/launcher_runtime.py`, `tools/foundation/package_inventory.py`,
`tools/foundation/tests/test_launcher_runtime.py`,
`tools/foundation/tests/test_package_inventory.py`,
`tools/foundation/tests/test_launcher_commands.py`, and
`tools/install_windows_shortcuts.ps1` (W1 replaces the old shortcuts).

In `test_reporting.py`, retire only the six Qt-only portable-smoke/environment/
startup/UI-size/explicit-prefix/recorded-SDK tests. Migrate the two mixed metadata
cases while retaining their native artifact/source-change assertions. Migrate all
three launch-freshness invariants to real Electron/native file fixtures. Retain
all broad reporting, hash, source-change, timeout, benchmark and render assertions.
The sole Qt-prefix-priority case in `test_migration_portability.py` becomes
Node/npm filesystem-layout tests; R1's two Qt-specific build cases become
Electron-default/headless-native and Node/npm-build cases, preserving parallel
limits and failure propagation. These migrations are also explicit in #469.
Remove the Qt ui-test/startup-test/package/updates commands and their Qt-only CLI
switches. There are no CI edits, ABI1 removals, device runs or user-data mutations.
Independent T1 review is required; the removal label is scoped to this inventory.

```acceptance
tier: T1
size: <= 2000 changed lines, <= 12 files
must_pass:
  - python tools/dev.py doctor
  - python -m unittest discover -s tools/foundation/tests -v
  - python tools/test_dev_accept.py
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - ci: R3 merged and archive/qt-final anchored before R4 merge
stops: [unlisted test retirement, helper coverage loss, device access, baseline removal, merge before R3]
```

The acceptance CLI is exercised by `tools/test_dev_accept.py`; acceptance must not
recursively invoke itself from this block. Real check-stage/report behavior also
has retained unit coverage; no GUI or device run is part of this task acceptance.

Signed: Codex (Electron tooling cleanup builder), 2026-11-09 UTC.
