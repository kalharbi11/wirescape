<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #685 — CI speed 1: no rerun on draft→ready, Windows on drafts, clang-format in cloud setup

Side lane (owner, 2026-11-12), priority. Owner: `gpt-builder3`. Reviewer: Keel.
It must not disturb main work: no required-check, gate or board changes.

**Read first:** `.github/workflows/ci.yml` (`on:` block, and the matrix `include:` at about line 135),
`.github/workflows/portability.yml` (`on:`), `tools/ci/codex_cloud_setup.sh` (pip line),
`docs/ARCHITECTURE.md` (the `ci/windows` sentence, about line 184).

## Steps
1. `ci.yml` and `portability.yml`: drop `ready_for_review` from `pull_request.types`.
   Keep `opened, synchronize, reopened`. Leave `guard.yml`, `merge_gate.yml` and `board.yml` as they are.
2. `ci.yml` matrix: every `pull_request` run includes `windows-x64`, drafts too. Drop the
   `draft`/`ci/windows` condition. Push, dispatch and main runs stay as they are. Remove the now-stale
   comment about `ci/windows`.
3. `codex_cloud_setup.sh`: add `clang-format==18.1.8` (or the newest 18.x on PyPI) to the
   existing pip line. Print `clang-format --version` next to the cmake/ninja version lines.
4. A stdlib-only test, `tools/foundation/tests/test_ci_triggers.py` (text checks, no PyYAML):
   - `ready_for_review` is absent from the `pull_request` types of both files;
   - the PR matrix no longer mentions `draft` or `ci/windows`;
   - the setup script pins `clang-format==18.`.
5. `docs/ARCHITECTURE.md`: Windows runs on every PR run, drafts included; `ci/windows` is no longer needed.

**Self-review must say:** "Required checks unchanged; draft PRs now report windows-x64",
plus `Patch guide impact: none — CI only`.

**Stops:** any change to required checks, the gate, guard or board; a new secret or token;
dropping a job.

```acceptance
tier: T2
size: <= 90 changed lines, <= 6 files
must_pass:
  - actionlint
  - bash -n tools/ci/codex_cloud_setup.sh
  - python -m unittest discover -s tools/foundation/tests -p test_ci_triggers.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green on the draft head
stops: [required-check change, gate/guard/board change, new secret, job removed]
```
