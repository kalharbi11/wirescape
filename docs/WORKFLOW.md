<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Workflow v2 (adopted 2026-10-28)

The rules every agent follows are in `AGENTS.md`. This file holds the detail. Source proposal: `docs/reviews/WORKFLOW_PROPOSAL_2026-10-27.md`.

**Idea:** rigor lives in scripts that run for free (public-repo GitHub Actions + the Pi runner). Every task states acceptance as commands. AI reads only what scripts can't judge, and the expensive reviewer sees only risky changes, through a compact review pack.

## 1. Where agents run
- **Claude Code on the web** and **Codex cloud tasks** run in cloud containers. Neither can reach the Pi or the laptop. No agent SSHs anywhere.
- **The Pi 5** runs no AI. It is a 24/7 worker: a self-hosted GitHub Actions runner (label `pi5`) registered **only** on the private repo `kalharbi11/wirescape-pi`. Its workflows check out this public repo and post results back (commit status `pi5/*` on PR heads, comments, one nightly summary issue). This repo's own workflows never target the Pi.
- **The laptop** is used only by the user. Laptop-only work (#64, real audio/MIDI runs) waits until the user does it.
- **Devices:** no agent ever opens an audio or MIDI device or a LAN socket. Localhost tests are fine. Real-device paths sit behind `--confirm-*` flags that only the user runs.

## 2. Pi runner jobs (in `wirescape-pi`)
| Job | Trigger | Does | Posts |
|---|---|---|---|
| `pr` | cron every 15 min: open PRs from this repo's own branches whose head SHA has no `pi5/tests` status yet | headless Release build + CTest on the exact head; `accept` if the task has `pi:` lines | commit status `pi5/tests` (+ `pi5/accept`), one comment with the summary table on failure or on the first green |
| `nightly` | 02:00 local | full tests on `main`, soak (10 min, Six Essentials + poly8), benchmark vs baseline, golden render hashes, full clang-tidy, fuzzing (after J5), review packs for open PRs | one rolling issue "Pi nightly YYYY-MM-DD" (closes the previous one) |
| `on-demand` | `workflow_dispatch` with a PR number and a command list | what a task's acceptance block names under `pi:` | commit status + comment |

Safety: the private repo has no outside collaborators; fork PRs never run (the poller skips heads not in `kalharbi11/wirescape`); jobs never use `sudo` or devices; one job at a time (`concurrency`); the token is fine-grained to this repo only (Contents: read, Commit statuses: write, Issues: write, Pull requests: write).

## 3. Review tiers
| Tier | Applies to | Needs before the user merges |
|---|---|---|
| **T0** scripts only | docs, tests-only, CI tweaks, `size/s` code with no protected paths | CI green + `accept` green. User skims and merges |
| **T1** + one cheap review | `size/m`/`size/l` modules, ports, tools, UI | T0 + a "ChatGPT pre-review" comment on the review pack, every High fixed or answered |
| **T2** Claude | ABI/SDK headers, engine2 scheduler, threading/rings, realtime callback, services thread model, patch format, CI guard, milestone gates, or a T1 with an unsettled High | T1 + Claude reads only the pack and the pre-review, then spot-checks named lines. Concurrency/DSP: a second ChatGPT pass in a fresh chat first |

When `accept` is green on the exact head, the builder sets `status/ready-to-merge` and posts the report. It never approves or merges.

## 4. Task files and the acceptance block
Every issue points to a `docs/tasks/` file that ends with a fenced `acceptance` block. If the planner can't write one, the task isn't ready.
````
```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 600 changed lines, <= 12 files
must_pass:
  - ctest -R chaos_lite_
  - pi: soak --graph examples/chaos_two.json --seconds 120 --block 128 --p99-under 50%
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
````
Until `tools/dev.py accept` exists (#85), the builder runs the `must_pass` lines by hand and pastes the results. Lines starting `pi:` run on the Pi runner (on-demand job); until it exists, they are marked "pending Pi runner" and don't block T0/T1.

## 5. Stops (post a handoff, label `status/blocked`, take the next item)
ABI header edit · non-zero equivalence delta without a planner decision · test removal or tolerance/threshold change · diff past the task's size budget · two failed attempts at the same failure · **design choice not in the spec** · **new dependency** (library, FetchContent, tool) not named in the task · **attempt budget** reached.

## 6. Usage hygiene
- **Codex:** one issue per thread, fresh thread per issue. Narrow read map. Never paste whole logs: read tool summaries, tail logs only on failure. Don't run the full suite locally when CI will: run the tests for touched files, push, read the CI summary. No evidence re-runs after merging `main` when no compiled file changed. Sol medium by default, Luna for mechanical edits, Sol high only after two failed attempts.
- **Claude Code:** a fresh session per check-in or review. Sonnet for routine check-ins, issue writing and plan edits; Opus only for T2 reviews and architecture decisions; Haiku for search sub-agents. Start from PLAN's "Now"; update it at the end. Reviews start from the review pack.
- **ChatGPT:** attach the review pack; ask for findings by severity with file:line and a test that would catch each. Post as "ChatGPT pre-review".

## 7. External agent queue (optional, replaces any other outside reviewer)
For work of at most 3 files that benefits from an independent strong model (second opinions on T2, DSP math checks, test brainstorming). No agent depends on it.
1. The agent builds the pack (`tools/review_pack.py --split 3`) with the exact prompt at the top of the first file and asks in the issue: "External-queue task: run it outside, or should I do it myself?"
2. **Blocking work:** ask once, then do it internally and note that an external pass is still available. **Non-blocking:** label `external-queue` and move on.
3. Packs live as `.md` files on the `external-queue` branch (never `main`), downloadable from the GitHub phone app.
4. The user pastes the answer as a comment headed **"External result"**; the next agent session acts on it.
5. The Pi nightly summary lists everything labelled `external-queue`.

## 8. Weekly rhythm
1. Planner session (Claude Sonnet, fresh): update "Now", shape the next issues with acceptance blocks.
2. Codex works the queue; ChatGPT builds self-contained modules on separate branches.
3. ChatGPT pre-reviews T1/T2 PRs (a second fresh pass on T2).
4. The user merges T0/T1 when green.
5. Gate review (Claude Opus, fresh) only at milestone ends or on a T2 escalation.

## 9. Not worth it
Agent-orchestration dashboards (they multiply spend). Copilot as a sub-agent for real work (no model control). Routing anything to Copilot while its credits are nearly gone.
