<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Workflow v2 (adopted 2026-10-28)

The rules every agent follows are in `AGENTS.md`. This file holds the detail. Source proposal: `docs/reviews/WORKFLOW_PROPOSAL_2026-10-27.md`.

**Idea:** rigor lives in scripts that run for free (public-repo GitHub Actions + the Pi runner). Every task states acceptance as commands. AI reads only what scripts can't judge, and the expensive reviewer sees only risky changes, through a compact review pack.

## 1. Where agents run
- **Claude Code on the web** and **Codex cloud tasks** run in cloud containers. Neither can reach the Pi or the laptop. No agent SSHs anywhere.
- **The Pi 5** runs no AI. It is a 24/7 worker: a self-hosted GitHub Actions runner (label `pi5`) registered **only** on the private repo `kalharbi11/wirescape-pi`. Its workflows check out this public repo and post results back (commit status `pi5/*` on PR heads, comments, one nightly summary issue). This repo's own workflows never target the Pi.
- **The laptop** is used only by the user. Laptop-only work (#64, real audio/MIDI runs) waits until the user does it.
- **Devices:** no agent ever opens an audio or MIDI device or a LAN socket. Localhost tests are fine. Real-device paths sit behind `--confirm-*` flags that only the user runs.

## 2. Pi runner jobs (in `wirescape-pi`; its README has setup and troubleshooting)
| Job | Trigger | Does | Posts |
|---|---|---|---|
| `poll.yml` | every 20 min + manual | open PRs whose head is in this repo (never forks), **non-draft or labelled `ci/pi`**, head SHA without a `pi5` status: headless Release build + `tools/dev.py test` on the exact SHA | commit status `pi5` |
| `nightly.yml` | 03:17 New York + manual | `main`: build, full tests, 600 s soaks (poly8, blocks 64/128/256), benchmark vs `baselines/pi5-benchmark.json`, frozen render hashes; later fuzzing (#94), clang-tidy (#87), review packs (#86) | overwrites the open issue **Pi nightly summary** (#101); opens "Pi nightly regression: <sha>" when a passing check starts failing |

CodeQL (C/C++, Python and Actions) runs through `.github/workflows/codeql.yml` on GitHub Actions, **not** on the Pi runner; advanced setup is already configured, so do not also enable default setup.

Builders: add `ci/pi` to a draft PR when its acceptance block has `pi:` lines; the `pi5` status appears within about 20 minutes plus build time. Safety: public code runs with a scrubbed environment (no token, no Actions credentials); fork heads are skipped; no `sudo`, devices or LAN listeners; one job at a time.

## 3. Review tiers
| Tier | Applies to | Needs before the user merges |
|---|---|---|
| **T0** scripts only | docs, tests-only, CI tweaks, `size/s` code with no protected paths | CI green + `accept` green. User skims and merges |
| **T1** + one cheap review | `size/m`/`size/l` modules, ports, tools, UI | T0 + a "ChatGPT pre-review" comment on the review pack, every High fixed or answered |
| **T2** second review | engine2 scheduler, threading/rings, realtime callback, services thread model, patch format, or a T1 with an unsettled High | T1 + a second ChatGPT pass in a fresh chat that ends `ChatGPT: approved for auto-merge <sha7>`, plus the Pi evidence the task names. Claude audits merged T2 PRs at the weekly check-in |
| **T2-Claude** | ABI/SDK headers, CI guard and workflows, specs, label-only paths, milestone gates (`milestone-review`) | Claude reads only the pack and the pre-review, then spot-checks named lines, and comments `Claude: approved for auto-merge <sha7>` |

When `accept` is green on the exact head, the builder sets `status/ready-to-merge` and posts the report.

## 3b. Who merges what
| PR | Who | When |
|---|---|---|
| T0, base `main`, no label-only paths | builder enables auto-merge | after `status/ready-to-merge`; GitHub merges when required checks are green |
| T1, base `main` | builder enables auto-merge | additionally a "ChatGPT pre-review" with every High fixed or answered |
| T2, base `main` | merge gate | `ChatGPT: approved for auto-merge <sha7>` on the current head |
| T2-Claude (ABI, CI/guard, specs, label-only, milestones) | merge gate | `Claude: approved for auto-merge <sha7>` on the current head |
| Stacked PR (base is another task branch) | nobody | only after its base merges and GitHub retargets it to `main` |
| Planner docs PRs | user (or planner-enabled auto-merge for docs-only T0) | green |

**Details settled 2026-10-29:**
- Auto-merge may use any merge method; squash is fine.
- A stacked PR's acceptance measures its **own** diff against its base branch (`accept --base <PR base ref>`), not against `main`; after the parent merges it is re-run against `main`.
- Mechanical, behaviour-free type fixes in `native/sdk/**` (no new API, no ABI header) are T0; anything else in the SDK is T2.
- Exact output hashes are asserted only **within one build** (same compiler and flags), for example patched vs. original in the same binary. Hashes from other compilers or architectures are recorded in reports, never asserted.

**Merge gate (2026-10-29):** `.github/workflows/merge_gate.yml` runs `tools/merge_gate.py` whenever a PR is labelled or the owner's account comments on it, and enables squash auto-merge when the rows above hold. So any agent that can label and comment can finish a PR; nobody needs the auto-merge button. Approval phrases count only from the owner's account and only with the first 7 characters of the current head SHA, so a new push needs a new phrase. The phrases are `ChatGPT: pre-review passed <sha7>` (T1), `ChatGPT: approved for auto-merge <sha7>` (T2), `Claude: approved for auto-merge <sha7>` (anything). Merges made by the gate use the workflow token, so they don't start push workflows on `main`; the Pi and laptop poll `main` themselves and CodeQL runs weekly.

**The user's rule of thumb:** a PR with base `main`, all checks green and no `tier/T2` label can be merged without asking Claude. Non-required checks (the static job, the optional acceptance job) may be red only if the PR didn't touch what they report on. Auto-merge needs the repository setting *Allow auto-merge* (Settings → General → Pull Requests).

## 3c. Autonomous mode (the default; the user is often away 04:00–11:00 New York)
Goal: agents keep moving without the user or Claude in the loop, and every shortcut stays reviewable later.

**Never idle.** While a PR waits for CI, the Pi or a review, take the next queue item and re-check pending ones between items. Stacked branches are fine for dependent work. Questions go into the issue with `status/blocked` on *that* item only; the queue continues.

**Self-decisions** a builder may make alone. Record each one under a `Self-decisions (review later)` heading in the PR body and the handoff:
- Size overruns up to +50% of the task's line or file cap, tests included.
- Test expectation updates that follow directly from the intended new behaviour (for example a catalogue count 13 → 14), with a one-line reason. Never loosen tolerances, thresholds or hashes.
- Merging `main` (or the parent branch) in and resolving conflicts by keeping both sides.
- Adding `ci/pi`, `ci/pi-soak` or `ci/pi-retry`, and retrying an infrastructure failure once.
- Picking among options the task file already lists; small refactors inside touched files needed to get CI green; fixing bugs in its own earlier PRs.

**Still a stop** (ask on the issue, block only that item): ABI header edit, new dependency, equivalence or hash delta beyond a planner-set bound, deleting or skipping tests, label-only paths, a security finding, or a design choice that changes IDs, file formats or user-visible behaviour not in the spec.

**Self-review before `status/ready-to-merge`:** run `python tools/review_pack.py <PR#> --prompt chatgpt`, read it against its checklist (realtime, IDs, state/versioning, thresholds, licences, devices) and post a `Self-review` comment with each item ticked or explained. T1 additionally needs the ChatGPT pre-review (posted by ChatGPT's scheduled watch).

**T2 and label-only auto-merge:** when Claude's review comment on a T2 PR, or on a PR touching label-only paths, says **`Claude: approved for auto-merge`**, the builder may enable auto-merge on it like a T0, as long as the head hasn't changed since that comment (any new push needs a fresh one). Stacked children are retargeted to `main` automatically by GitHub once their parent merges; then the builder re-runs their acceptance against `main` and treats them as normal PRs.

**J stack merge order** (stacked PRs never auto-merge while stacked): #78 J1 → GitHub retargets #79 to `main` → builder merges `main` in, re-runs acceptance against `main`, gets green + pre-review → auto-merge → same for #80 → then #81 and #82 are refreshed onto `main` (they're stale) and follow. A child's own diff is reviewed while stacked, so retargeting needs only a green re-run, not a second review, unless the merge changed its code.

**Overnight timeline (New York time):**
| When | Who | What |
|---|---|---|
| every 5 min | Pi runner daemon | `pi5`, `pi5/soak`, `pi5/accept` (task `pi-run:` lines) on eligible PR heads |
| hourly | ChatGPT reviewer watch | for each open PR on `main`: pre-review (T1) or second review (T2) on the current head, ending with the gate phrase |
| hourly | ChatGPT steward watch | stuck PRs, labels, retargeted stacks, Pi labels; one summary on #101 at 07:00 |
| hourly | Laptop runner | once per new `main` SHA: full Windows (Qt) build, tests, benchmark → "Laptop nightly summary" issue |
| 03:17 | Pi runner | nightly: full tests, 600 s soaks, benchmark, render hashes → #101 |
| see §3d | Claude scheduled check-in | T2-Claude reviews, plan and queues |
| any time | Codex long task | works the queue in order, never idle (above) |

## 3d. Check-ins and agent hand-offs (from 2026-10-29: Claude usage is scarce)
**Claude:** one check-in a day at **17:30 New York**, a fresh session each time (Sonnet; Opus only for a T2-Claude review). It reads `docs/planner/STATE.md` first and only what that file points to, then:
1. Reviews PRs waiting on T2-Claude and posts the gate phrase when sound.
2. Fixes what the steward reported as blocked that needs a planner decision.
3. Adds the next issues (with acceptance blocks) so the queues never run dry.
4. Rewrites `docs/planner/STATE.md` (merged via its own T0 PR) and posts the Codex queue on #29.
Weekly (Mondays): audits PRs merged on ChatGPT approvals since the last audit.

**Codex:** started by the user; it works the latest planner queue on #29 under §3c.

**ChatGPT (two scheduled agents, prompts in `docs/planner/CHATGPT_AGENTS.md`):** the reviewer and the steward, both hourly. They act only through labels and comments; the merge gate does the merging.

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
