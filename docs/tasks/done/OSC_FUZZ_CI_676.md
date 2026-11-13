<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #676 — W-h2: non-required OSC fuzz CI job, 60 s per target

Spec: Keel's CI-policy ruling on #676 (comment 6823484751), option (a). Owner: `claude-builder1`. PR #710.

- New `.github/workflows/osc_fuzz.yml`: `pull_request` and `push: main` path-filtered to `native/io/osc/**` and the workflow itself, plus `workflow_dispatch`. **Never a required check**, never in the merge gate or branch protection.
- `permissions: contents: read` only; no secrets, PAT or `pull_request_target`; per-ref concurrency with cancel; `timeout-minutes: 15`.
- Same hosted Linux image as `ci.yml`, pinned pip cmake/ninja, `clang-18 libclang-rt-18-dev` with the bounded apt retry. No other dependency.
- Runs exactly the fuzz commands of #94's acceptance block (`docs/tasks/Wh_FUZZ.md`): one source of truth, enforced by the test.
- Crash: upload input and log (`retention-days: 14`) and fail. No automatic issue (would need `issues: write`).
- Pi nightly (30 min per target, device-off) is a separate `wirescape-pi` PR reporting into #101; not part of this repo.

Budget: Keel set ≤ 120 lines / 3 files. The 14-line overage is the run-block-only command check Codex Lead asked for on #676 (task files are not counted); Keel to confirm.
```acceptance
tier: T2
size: <= 140 changed lines, <= 3 files
must_pass:
  - python -m unittest tools/foundation/tests/test_osc_fuzz_workflow.py
  - ci: guard, osc-fuzz, linux-arm64, sanitizers green
stops: [making osc-fuzz a required check, any write permission or secret, pull_request_target, a new dependency]
```
