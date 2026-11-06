<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Workflow v3 (adopted 2026-11-06)

`AGENTS.md` holds the rules. This file is the detail behind them. v2 (agent roster, approvals v3, scheduled check-ins, external queue) is in git history and retired.

**Idea:** any agent can pick up any item. Rigor lives in scripts that run for free (GitHub Actions, the Pi runner, `tools/dev.py accept`), not in who is allowed to do what. Each item says what "done" means as commands. Each agent signs its work and leaves a handoff, so switching between Claude, ChatGPT and Codex in the middle of an item costs one comment read.

## 1. Where agents run
- Cloud agents (Claude Code on the web, Codex cloud, ChatGPT with GitHub) can't reach the Pi or the laptop. No agent SSHs anywhere.
- **The Pi 5** runs no AI. It is a self-hosted runner (`pi5`) registered only on the private repo `kalharbi11/wirescape-pi`, which checks out this repo and posts statuses (`pi5`, `pi5/soak`, `pi5/accept`) and nightly summaries (#101). Setup: `docs/PI_RUNNER.md`.
- **The laptop** runs the Windows nightly (#122). Laptop-only work waits for the owner.
- **Devices:** no agent opens an audio/MIDI device or a LAN socket. Localhost is fine. Real devices only behind `--confirm-*`, run by the owner.

## 2. Picking up work
1. Find an open issue in the current phase (`phase/<n>` label, or the plan's checkboxes) with `status/todo`, or one whose last claim is stale (24 h with no push or handoff; 6 h for Codex Lead on critical-path work).
2. Comment `Claimed: <agent + model>, <date>`, then set `status/in-progress`.
3. Read the issue's latest handoff and the task file; then code.
4. If you continue someone else's branch, say so in the claim (`Continuing from <sha7>`). Never force-push over their commits.

Parallel work: sections of a phase marked parallel in `docs/PLAN.md` touch separate files by design. If two items need the same file, the second waits or coordinates in the issue.

## 3. Signing
Every handoff, review and approval comment ends with `Signed: <agent + model>, <date>` (for example, `Signed: Codex (GPT-5.x), 2026-11-07`). Commits may carry a `Co-Authored-By` or similar trailer from the agent's own tooling. Signatures are for traceability, not permissions: every agent posts through the owner's account.

## 4. Review and merge
| Tier | Applies to | Needs, on the current head |
|---|---|---|
| **T0** | docs, tests only, formatting, CI-red fixes in touched files, planner notes | green required checks + `status/ready-to-merge` |
| **T1** | features, modules, UI, tools, scripts | the above + one review comment from a session other than the author's + an approval line |
| **T2** | engine threading/realtime, patch format/migration, host protocol, ABI/SDK, merge control/CI | the above, with the reviewer from a **different model family** than the author |

- The tier comes from the task file. If the diff touches a higher tier's paths, the higher tier wins.
- **Approval line** (gate v4, `docs/tasks/W_GATE_V4.md`): a standalone line `Approved for merge <sha7>` followed by a `Signed:` line. A new push needs a new line. Until gate v4 merges, use the old `<Name>: approved for auto-merge <sha7>` line with your **own** name. Protected paths need a `Claude:` line until then.
- **Self-review first:** before `status/ready-to-merge` the author runs `python tools/review_pack.py <PR#>`, checks it against the checklist (realtime, IDs, state/versioning, thresholds, licences, devices) and posts a `Self-review` comment. The reviewer starts from the same pack.
- Any reviewer can stop a PR with a High + `status/blocked`. The gate never merges a blocked PR, and an author never approves over an open High.
- Builders don't click merge; only Compass and Keel merge directly (`AGENTS.md`). The merge gate (`.github/workflows/merge_gate.yml` + `tools/merge_gate.py`) enables auto-merge; GitHub waits for the required checks. Stacked PRs wait for retargeting to `main`, then re-run their acceptance against `main`.
- Owner-only: licence changes, `legacy/**`, `docs/history/**`, branch protection, real-device results, `allow-test-removal`.

## 5. Decisions a builder may make alone
Record each under `Self-decisions` in the PR body and the handoff:
- size up to +50% over the task's budget, tests included;
- test expectation updates that follow directly from the intended behaviour (with a one-line reason; never loosen tolerances, thresholds or hashes);
- merging `main` in and resolving conflicts by keeping both sides;
- adding `ci/pi`, `ci/pi-soak`, `ci/pi-retry`, and retrying an infrastructure failure once;
- picking among options the task or plan already lists; small refactors inside touched files;
- design choices the spec leaves open that change no IDs, formats, ABI, protocol or realtime behaviour.

Everything else in `AGENTS.md` "Stops" goes to a lead via the issue (Codex Lead first; Keel for technical stops, Compass for plan stops). Only that item blocks; take another item meanwhile.

## 6. Task files and acceptance blocks
Every issue points to a `docs/tasks/` file ending in a fenced `acceptance` block that `tools/dev.py accept` parses:
````
```acceptance
tier: T1
size: <= 600 changed lines, <= 12 files
must_pass:
  - ctest -R host_protocol_
  - pi: build + ctest -R host_protocol_ (null audio)
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
````
`pi:` lines run on the Pi runner. Owner-only checks (real sound, MIDI devices, the display) are listed under `owner:` in the task text, not in `must_pass`; they're collected at checkpoints.

## 7. Checkpoints
At the end of Phases 1, 2 and 4 (`docs/PLAN.md`): tag `checkpoint-<X>`, build the review pack for the phase, and hold one owner + Compass session. Decisions go at the top of `docs/PLAN.md`. Work on the next phase continues during the review; a High blocks only the sections it touches.

## 8. Usage hygiene
- Start a fresh thread per issue. Read narrowly (the read map in `AGENTS.md`). Never paste whole logs; tail on failure.
- Don't run the full suite locally when CI will. Run the tests for the files you touched, push, and read the CI summary.
- Pi evidence only for compiled-code changes. No re-runs after a merge from `main` that touches no compiled code.
- Use the cheaper model for mechanical edits and the stronger one for T2 work, reviews and architecture.
- Delegate, don't wait: hand mechanical work to a cheaper agent (ChatGPT chat, another Codex chat, a subagent) with a self-contained brief, and keep working. Don't poll CI; the gate merges on green.
- Near a usage limit: push, post handoffs, and leave paste-ready briefs for agents that still have budget (`docs/planner/LEADS.md`, wrap-up mode).

## 9. Not worth it
Agent-orchestration dashboards, scheduled agent check-ins that run with nothing to do, and per-agent queues. The board (issues + this plan) is the queue.
