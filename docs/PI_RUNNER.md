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
   on the runner. Jobs never use sudo or install system packages. Ensure the
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
   Reuse pinned [Pi nightly summary #100](https://github.com/kalharbi11/wirescape/issues/100).
   If it is missing, create one issue with that title and pin it. If it is
   missing, nightly still runs independent checks and uploads its report, then
   reports the missing summary issue as a setup blocker.
5. Commit a measured Pi benchmark baseline in the private repository as
   `baselines/pi5-benchmark.json`, using the public benchmark JSON schema
   (`rows` with at least five positive finite `repetition_means_ns` per row).
   Record the public source SHA, compiler, CMake, kernel and collection command
   with the baseline. Review its measurements before committing it; cloud
   x86_64 measurements are not a Pi baseline. Until it exists, nightly must
   report the comparison as blocked, rather than invent passing evidence.

## PR evidence

`poll.yml` runs every 20 minutes and supports manual dispatch. It lists open
public PRs, rejects forks, and selects PRs that are ready for review or have
the `ci/pi` label. A draft therefore needs `ci/pi` to opt in. It checks out
each eligible head at its full SHA and runs the headless Release build and
`python tools/dev.py test` only when that SHA has no `pi5` commit status.
The final success/failure status links to the private workflow run. The job
summary records the SHA, compiler, CMake and kernel. Private run links are
visible only to collaborators with access to wirescape-pi.

To request evidence, add `ci/pi` to an eligible draft (or mark it ready), then
open **wirescape-pi → Actions → Pi PR poll → Run workflow** and choose `main`.
Alternatively, with private-repository workflow permission:

```bash
gh workflow run poll.yml --repo kalharbi11/wirescape-pi --ref main
gh run list --repo kalharbi11/wirescape-pi --workflow poll.yml --limit 5
gh api repos/kalharbi11/wirescape/commits/HEAD_SHA/status
```

Replace `HEAD_SHA` with the PR's actual full head SHA. Confirm the `pi5` status
on that SHA; an older status or a running job is not evidence for the new head.
The poll never retests an existing `pi5` status automatically. Investigate failed
runs before explicitly requesting a retry or pushing a correction.

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

Once #103 lands, either job can call `tools/review_pack.py` with `--diff`,
`--task`, `--split 3`, `--cap` and `--output`. The controller supplies a fresh diff
and task for the exact PR head. This offline path needs no API credential in the
public build environment; unavailable CI evidence stays explicitly unavailable.
