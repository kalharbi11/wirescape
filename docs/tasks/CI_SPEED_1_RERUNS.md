<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #685 — CI speed 1: no rerun on draft→ready, Windows on drafts, clang-format setup + Static format patch

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

## Part B: Static prints a ready-to-apply format patch (Coda's report, 2026-11-12)
Formatting blocked GPT PRs four times today: connector-only GPT builders can't run clang-format, and Static only prints
`--dry-run` warnings.
6. `tools/ci/static_checks.py`: when a `clang-format <file>` job fails, run the same `clang-format-18`
   with the same `--lines` (no `-i`) and build a unified diff against the file (`difflib`, paths
   `a/<file>` / `b/<file>`). Collect every file's diff into one `build/static/format.patch`.
   - Print it to the log between `=== format.patch begin ===` and `=== format.patch end ===`. Agents read
     logs through the API; step summaries aren't reachable that way.
   - Also add it to `$GITHUB_STEP_SUMMARY` as a `diff` block, capped at about 60 KB.
   - Applying it is `git apply format.patch`, or the same hunks applied by hand through a file edit.
7. `static.yml`: upload `build/static/format.patch` as the `format-patch` artefact when it exists
   (`if: always()`). Pin `actions/upload-artifact` the way other workflows do.
8. Tests in `tools/ci/test_static_checks.py`: a badly formatted fixture gives a patch that `git apply --check`
   accepts and that, once applied, passes `--dry-run --Werror`. Skip the test cleanly if clang-format isn't installed.

Part B changes no check result: Static stays advisory and only its output grows. If part B hits a stop, ship
part A (steps 1–5) alone and hand off part B.

**Self-review must say:** "Required checks unchanged; draft PRs now report windows-x64",
plus `Patch guide impact: none — CI only`.

**Stops:** any change to required checks, the gate, guard or board; a new secret or token;
dropping a job.

```acceptance
tier: T2
size: <= 220 changed lines, <= 8 files
must_pass:
  - actionlint
  - bash -n tools/ci/codex_cloud_setup.sh
  - python -m unittest discover -s tools/foundation/tests -p test_ci_triggers.py
  - python -m unittest discover -s tools/ci -p test_static_checks.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green on the draft head
stops: [required-check change, gate/guard/board change, new secret or token, job removed, a bot pushing commits]
```
