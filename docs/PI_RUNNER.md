<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Private Pi runner bridge

The self-hosted runner with labels `self-hosted`, `linux`, `ARM64`, `pi5`
is registered only in the private `kalharbi11/wirescape-pi` repository.
The public repository never schedules a job on it. Codex cloud uses this bridge
for Pi evidence; it does not SSH into the Pi or the Windows laptop.
All commands keep physical audio and MIDI devices off.

## User setup

1. In **wirescape-pi → Settings → Actions → Runners**, verify the Pi runner is
   online and has the four labels above. Keep this repository private, limit
   write access to trusted maintainers, and do not share its runner with the
   public repository. A same-repository PR is trusted code: review who can push.
2. Install Python 3.12 or newer, a C++17 compiler, CMake 4.2.3 and Ninja 1.13.0
   on the runner user's PATH. On Raspberry Pi OS, install pinned Python packages
   in a venv; system `pip install --user` may be rejected under PEP 668.
   The bridge adds `~/.local/bin` and can find CTest beside the resolved CMake.
   Use the private README's venv setup; never override system Python protections. The runner also needs Qt Core >=6.5 development files for
   document-loader tests and soaks. Jobs never use sudo or install system packages. Ensure the
   runner has Git and can reach GitHub and the package registry. GitHub CLI is
   only needed by operators using the optional dispatch commands below.
3. In **wirescape-pi → Settings → Secrets and variables → Actions**, add the
   repository secret **WIRESCAPE_TOKEN**. Use a token limited to the public
   `kalharbi11/wirescape` repository with Contents read, Pull requests read,
   Commit statuses write, and Issues write permissions. A maintainer pins the
   summary issue; the workflow token only updates its body. Never paste it into an issue,
   log, checked-in file, or chat. Private checkout uses the private repository's
   own Actions token; all public-repository API calls use WIRESCAPE_TOKEN.
4. Enable Actions and scheduled workflows in the private repository. Both
   workflows must be on its default branch for schedules and dispatch to work.
   They share a concurrency group so a poll cannot overlap a nightly soak or
   benchmark. Do not add another workflow that bypasses that group.
   Reuse pinned [Pi nightly summary #101](https://github.com/kalharbi11/wirescape/issues/101).
   If it is missing, create one issue with that title and pin it. Until then,
   nightly still runs independent checks and uploads its report, then
   reports the missing summary issue as a setup blocker.
5. Commit a measured Pi benchmark baseline in the private repository as
   `baselines/pi5-benchmark.json`, using the public benchmark JSON schema
   (`rows` with at least five positive finite `repetition_means_ns` per row).
   Record the public source SHA, compiler, CMake, kernel and collection command
   with the baseline. Review its measurements before committing it; cloud
   x86_64 measurements are not a Pi baseline. Until it exists, nightly must
   report the comparison as blocked, rather than invent passing evidence.

## PR evidence

`poll.yml` has a best-effort 20-minute cron and supports manual dispatch. Each
job polls every five minutes for 330 minutes, avoiding GitHub cron throttling.
It can run anytime on the 24/7 Pi; all Pi work shares one concurrency group.
It lists open
public PRs, rejects forks, and selects PRs that are ready for review or have
the `ci/pi` label. A draft therefore needs `ci/pi` to opt in. It checks out
each eligible head at its full SHA and runs the headless Release build and
`python tools/dev.py test` only when that SHA has no `pi5` commit status.
The final success/failure status links to the private workflow run. The job
summary records the SHA, compiler, CMake and kernel. Private run links are
visible only to collaborators with access to wirescape-pi.

To request evidence, add `ci/pi` to a draft (or mark it ready) and let the
active daemon pick it up. Only start a daemon manually if none is running:
**wirescape-pi → Actions → Pi PR poll → Run workflow**, choosing `main`.
With private-repository workflow permission:

```bash
gh workflow run poll.yml --repo kalharbi11/wirescape-pi --ref main
gh run list --repo kalharbi11/wirescape-pi --workflow poll.yml --limit 5
gh api repos/kalharbi11/wirescape/commits/HEAD_SHA/status
```

Replace `HEAD_SHA` with the PR's actual full head SHA. Confirm the `pi5` status
on that SHA; an older status or a running job is not evidence for the new head.
The poll skips existing statuses by default. After diagnosing and fixing an
infrastructure failure, add **`ci/pi-retry`** to request one rerun on the same
head SHA. It reruns tests and any labeled soak, then removes the retry label
after publishing results, whether success or failure. If publishing fails, the
label remains. Never push an empty commit merely to retrigger. Drafts still
need `ci/pi`, and forks remain ineligible. Prefer the scheduled poll; avoid
duplicate manual runs. All private Pi jobs share the exclusive concurrency group.

For task-specific commands, add `pi-run:` lines to the task acceptance block
and label the PR **`ci/pi-accept`**. The bridge posts **`pi5/accept`** and a PR
comment with output tails. A build/test status alone does not verify these
commands; inspect their result on the exact head.

## Real-document loader soak

Add **`ci/pi-soak`** as well as `ci/pi` to request J1's real-loader evidence.
The bridge posts a separate **`pi5/soak`** status, once per head SHA unless
explicitly retried, from:

```bash
build/native/wirescape_soak --patch native/examples/Six_Essentials_Demo.json --rate 48000 --block 128 --seconds 600
```

These paths are implemented in private main `ebd2229` (including private PRs
#2, #3 and #5). Confirm success for both contexts on the exact PR head; an old
status or a passing `pi5` build/test cannot substitute for the loader soak.
If tests already have a result, a newly requested soak builds that same head
without rerunning tests; `ci/pi-retry` forces both requested stages.

## Nightly evidence

`nightly.yml` runs at 03:17 America/New_York (with dispatch available) and tests
the latest public `main`: the full headless tests, offline
ten-minute soaks at 48 kHz with blocks 64, 128 and 256, the ABI2 benchmark
against the committed Pi baseline, and generated render hashes against the
public fixture manifest. Fuzzing is explicitly deferred until codecs and a
bounded fuzz entry point exist; it is not reported as passed.

The job updates one pinned public issue titled **Pi nightly summary** with a
short results table and the source SHA/run link. New regression issues are
reserved for confirmed regressions; missing setup or a missing baseline must
be reported separately. Workspace cleanup runs after failures as well as
successes. Artifacts and summaries should be used for evidence, never a claim
that audio or MIDI hardware was exercised.

When a job fails, inspect its private run log first. Check runner availability,
the token's permissions and expiry, prerequisite versions and the exact tested
SHA. Keep required checks unchanged; never mark an unrun or blocked stage green.

#103 is merged; either job can call `tools/review_pack.py` with `--diff`,
`--task`, `--split 3`, `--cap` and `--output`. The controller supplies a fresh diff
and task for the exact PR head. This offline path needs no API credential in the
public build environment; unavailable CI evidence stays explicitly unavailable.

## Windows laptop runner

`laptop.yml` uses `self-hosted`, `Windows`, `laptop` labels in the private repo.
Its hourly job builds/tests/benchmarks new public main SHAs and updates one
**Laptop nightly summary** issue. Set system `WIRESCAPE_QT_DIR` to the MSVC Qt
SDK prefix and run the runner as a service; do not also launch `run.cmd`.
The runner name and registration token are not workflow selectors.
