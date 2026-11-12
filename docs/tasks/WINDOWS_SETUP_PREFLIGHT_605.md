<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #605 Windows source-setup preflight and launcher follow-up

Keel's issue #605 ruling and the 2026-11-12 narrow Ninja-wheel ruling on PR #608
are binding. This T1 follow-up does not change the Windows W1 architecture.

## Read first
- #605, its current state, and Keel's latest PR #608 ruling.
- `docs/tasks/W1_WINDOWS_SETUP.md`, `docs/INSTALL.md`,
  `tools/windows/setup.ps1`, `tools/windows/Wirescape.cmd`.
- `tools/foundation/tests/test_windows_setup.py`,
  `apps/desktop/tests/windows-setup.spec.cjs`.

## Deliver
- One preflight table (tool, found, required, OK), then aggregate failure.
  Check x64 compiler shell, SDK/rc, Python 3.12 x64, Node 22 x64,
  exact CMake 4.2.3 and pinned Ninja 1.13.0. Recover x64 VS DevShell and
  Python user Scripts on process-local PATH only.
- For Ninja, accept `1.13.0` or the exact pip-wheel vendor suffix
  `1.13.0.git.kitware.jobserver-pipe-<digits>` using one helper at both sites.
  Reject missing, 1.12.1, 1.14.0, 1.13.01, 1.13.0-rc1, 1.13.0.git.other.
  Keep the displayed actual version and Required `1.13.0`.
- Preserve read-only `-CheckOnly`, clean source, output guards and
  device-off W1 test. Exercise the actual `Wirescape.cmd --null-audio`
  smoke, bound the process, and print the launch path and summary.
- No Node 24, CMake 4.4, Ninja >=1.12 ranges or any new dependency:
  broader toolchain support is deferred pending separate Keel evidence.

Patch guide impact: none — setup/bootstrap tooling, not patch behavior.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_windows_setup.py
  - ci: guard, linux-arm64, windows-x64 and sanitizers green
  - ci: Windows PowerShell -CheckOnly and real W1 native/launcher smoke unskipped on exact head
stops: [new dependency, protected CI edit, broad version range, unsafe install, device use, test relaxation, false-positive smoke]
```

Signed: builder-chat-2 (GPT-6), 2026-11-12
