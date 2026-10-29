<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W1 (backlog g): Pi runner workflows in the private repo (#84)

**Why:** the Pi is a free 24/7 worker. Agents can't reach it, so every Pi number comes from GitHub Actions jobs on a self-hosted runner (label `pi5`) registered **only** on the private repo `kalharbi11/wirescape-pi`. This repo's workflows never target the Pi. Design: `docs/WORKFLOW.md` §2.

**Do (all files in `wirescape-pi`, except the docs line):**
1. `.github/workflows/pr.yml`: cron `*/15 * * * *` + `workflow_dispatch`. List open PRs of `kalharbi11/wirescape` (REST), keep heads whose repo is `kalharbi11/wirescape` (never forks) and whose head SHA has no `pi5/tests` status; for the oldest one: check out that SHA, `python tools/dev.py build --headless`, `python tools/dev.py test`, set commit status `pi5/tests` (success/failure + short description with pass count), comment once with a ≤30-line summary on failure. `concurrency: pi5` (one job at a time), `timeout-minutes: 60`.
2. `.github/workflows/nightly.yml`: cron 02:00 Europe local + dispatch. On `main`: full tests; `wirescape_soak` 600 s for `poly8` and the Six Essentials demo at 48000/128; `python tools/dev.py benchmark --json`; render hashes (once #92 exists); full clang-tidy (once #87 exists); fuzzing (once #94 exists); review packs for open PRs (once #86 exists, uploaded to the `external-queue` branch of this repo). Opens one issue "Pi nightly YYYY-MM-DD" in `kalharbi11/wirescape` with a table (toolchain, kernel, governor, tests, soak p50/p99/worst, benchmark vs baseline, `external-queue` list) and closes the previous one.
3. `.github/workflows/on-demand.yml`: `workflow_dispatch` inputs `pr` and `commands` (the `pi:` lines of an acceptance block); runs them on the PR head; posts status `pi5/accept` + comment.
4. `README.md` in `wirescape-pi`: runner registration steps (the user runs them on the Pi), the fine-grained token (repo `kalharbi11/wirescape` only: Contents read, Commit statuses write, Issues write, Pull requests write) stored as secret `WIRESCAPE_TOKEN`, and the rules: no `sudo`, no audio/MIDI devices, no LAN listeners, toolchains under `$HOME`.

**Access:** Codex needs write access to `kalharbi11/wirescape-pi` (the user grants it). Workflows can be validated with `actionlint` in the cloud; the first real run needs the user's runner registration.

```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 450 changed lines, <= 6 files (in wirescape-pi)
must_pass:
  - actionlint .github/workflows/*.yml                      # in wirescape-pi
  - python scripts/select_heads.py --self-test             # fork heads skipped, already-statused heads skipped
  - user: runner registered with label pi5; one dispatch of pr.yml on an open PR sets pi5/tests on its head
  - user: one dispatch of nightly.yml opens the "Pi nightly" issue with all table rows filled or marked "not yet"
stops: [workflow in kalharbi11/wirescape targeting the Pi, device access, sudo, token scope wider than above]
```
