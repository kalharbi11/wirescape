<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M0-4: Guard hardening and CI trigger cleanup

Branch `task/<issue#>-guard-hardening`, draft PR. Follow AGENTS.md. **This task is explicitly allowed to change `.github/**` and `tools/ci/**`.** Size: s (about 120 changed lines). Source: the planner review on PR #4.

**Read first:** `.github/workflows/ci.yml`, `tools/ci/guard.py`, `tools/ci/test_guard.py`. Nothing else.

## Deliverables
1. **The guard judges with the base branch's rules, never the PR's.**
   - Move the `guard` job out of `ci.yml` into a new `.github/workflows/guard.yml`. Trigger it only on `pull_request_target` with types `[opened, synchronize, reopened, ready_for_review, labeled, unlabeled]`, under `permissions: contents: read`.
   - Check out the **base** commit (the default for `pull_request_target`) with `fetch-depth: 0`. Then fetch the PR head as data only: `git fetch --no-tags origin +refs/pull/${{ github.event.pull_request.number }}/head:refs/remotes/pr/head`.
   - Run the base's `tools/ci/guard.py` and its unit tests. **Never check out, build, import or execute files from the PR head.**
   - The job keeps the name `guard`, so branch protection doesn't change. There must be exactly one `guard` check per PR commit: no skipped duplicate from `push` runs.
2. **Merge-base diffs.** `guard.py` compares the merge base with the head: `git diff --merge-base BASE HEAD` (or `BASE...HEAD`), for both the name-status list and the per-file patches. Add a test that builds a temporary git repo where `main` moves after branching (for example, `main` edits `docs/PLAN.md` while the branch edits another file) and assert the guard passes.
3. **Protect the guard.** Add `tools/ci/**` to `PROTECTED` in `guard.py`, and add `/tools/ci/ @kalharbi11` to `.github/CODEOWNERS`.
4. **Wider binary/private list:**
   - Add `.so .dylib .lib .pdb .obj .a .woff .woff2 .flac .mp3 .aif .aiff` and the `local_backups` directory.
   - Allow audio extensions under `native/tests/fixtures/**` only. These are reviewed test fixtures: `.wav` and `.flac`. No other binary type may be added there.
   - Tests for each.
5. **Fewer duplicate runs in `ci.yml`:**
   - `push` runs only for `main`. `pull_request` and `workflow_dispatch` stay as they are.
   - On `labeled` or `unlabeled` events, the native jobs run only when the changed label is `ci/windows` (`github.event.label.name`). Every other event type behaves as today.
6. **Benchmark numbers in the log.** After `dev.py test` on `linux-arm64`, append the content of `build_reports/abi2/benchmark.json` to `$GITHUB_STEP_SUMMARY` as a small Markdown table: row name, mean ns, p99 ns, worst ns, plus the three ratios. The step must not fail if the file is missing. Print a warning instead.
7. Update the "CI" section in `docs/ARCHITECTURE.md` (2–4 lines): the guard runs base rules via `pull_request_target`, and push CI runs only on `main`.

## Acceptance
- `linux-arm64` and `windows-x64` are green on the PR (label it `ci/windows` while it's a draft). `python -m unittest discover -s tools/ci -v` passes locally; paste the output in the handoff.
- **Bootstrap note:** `pull_request_target` always runs the workflow file from the PR's *base* branch. `main` has no `guard.yml` until this PR merges, so **no `guard` check runs on this PR**. That's expected. Say so in the handoff. The user merges this one PR with the admin "merge without waiting for requirements" option after review.
- **Prove the new guard before merge** with a temporary base branch:
  1. Push this PR's head as `ci-test/<issue#>-guard-base`.
  2. Open a throwaway draft PR **into that branch** containing (a) an edit to `tools/ci/guard.py` that makes `violations()` return `[]`, plus an edit to `AGENTS.md`. `guard` must be **red**, which proves the base rules judge the PR.
  3. Push (b) a deleted file under `native/tests/`. `guard` must be red without `allow-test-removal`.
  4. Exactly one `guard` check appears per commit.
  5. Record the run links. Close the throwaway PR and delete `ci-test/<issue#>-guard-base`. Never touch `main`.
- The `linux-arm64` run's summary page shows the benchmark table.

## Out of scope
Branch-protection settings (the user sets those), sanitizer jobs (M1-3 adds them), and any change to native code.
