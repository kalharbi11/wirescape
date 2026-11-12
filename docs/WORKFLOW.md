<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Workflow v4 (adopted 2026-11-12; v3 2026-11-06)

`AGENTS.md` holds the rules. This file is the detail behind them. v2 (agent roster, approvals v3, scheduled check-ins, external queue) is in git history and retired.

**Idea:** any agent can pick up any item. Rigor lives in scripts that run for free (GitHub Actions, the Pi runner, `tools/dev.py accept`), not in who is allowed to do what. Each item says what "done" means as commands. Each agent signs its work and leaves a handoff, so switching between Claude, ChatGPT and Codex in the middle of an item costs one comment read.

## 1. Where agents run
- Cloud agents (Claude Code on the web, Codex cloud, ChatGPT with GitHub) can't reach the Pi or the laptop. No agent SSHs anywhere.
- **The Pi 5** runs no AI. It is a self-hosted runner (`pi5`) registered only on the private repo `kalharbi11/wirescape-pi`, which checks out this repo and posts statuses (`pi5`, `pi5/soak`, `pi5/accept`) and nightly summaries (#101). Setup: `docs/PI_RUNNER.md`.
- **The laptop** runs the Windows nightly (#122). Laptop-only work waits for the owner.
- **Devices:** no agent opens an audio/MIDI device or a LAN socket. Localhost is fine. Real devices only behind `--confirm-*`, run by the owner.

## 2. Picking up work
1. Open the board (issue #473's body; its auto section is rebuilt from labels by `.github/workflows/board.yml` + `tools/board.py`). Take your own `agent/<name>` items first, then reviews you may do, then an unassigned item. Items labelled with another agent's name are theirs unless a lead reassigns them.
2. Comment `Claimed: <name>, <date>`, then set `status/in-progress` and `agent/<name>`. Names: `AGENTS.md` "Agents and the board".
3. Read the issue's latest handoff and the task file; then code.
4. If you continue someone else's branch, say so in the claim (`Continuing from <sha7>`). Never force-push over their commits.

Parallel work: sections of a phase marked parallel in `docs/PLAN.md` touch separate files by design. If two items need the same file, the second waits or coordinates in the issue.

## 3. Signing
Every handoff, review and approval comment ends with exactly `Signed: <name>, <YYYY-MM-DD>` (New York date; nothing after it), for example `Signed: gpt-builder2, 2026-11-12`. Names carry the family (`gpt-…`, `claude-…`), so no model goes in the signature. Commits may carry a `Co-Authored-By` or similar trailer from the agent's own tooling. Signatures are for traceability, not permissions: every agent posts through the owner's account.

## 4. Review and merge
| Tier | Applies to | Needs, on the current head |
|---|---|---|
| **T0** | docs, tests only, formatting, CI-red fixes in touched files, planner notes | green required checks + `status/ready-to-merge` |
| **T1** | features, modules, UI, tools, scripts | the above + one review comment from a session other than the author's + an approval line |
| **T2** | engine threading/realtime, patch format/migration, host protocol, ABI/SDK, merge control/CI | the above, with the reviewer from a **different model family** than the author |

- The tier comes from the task file. If the diff touches a higher tier's paths, the higher tier wins.
- **Approval line** (gate v4, `docs/tasks/done/W_GATE_V4.md`): a standalone line `Approved for merge <sha7>` followed by a `Signed:` line, in the same comment. A new push needs a new line. The old `<Name>: approved for auto-merge` lines are legacy; don't post new ones. Who may sign: `docs/planner/LEADS.md` "Authority".
- **Self-review first:** before `status/ready-to-merge` the author runs `python tools/review_pack.py <PR#>`, checks it against the checklist (realtime, IDs, state/versioning, thresholds, licences, devices) and posts a `Self-review` comment. The reviewer starts from the same pack.
- Any reviewer can stop a PR with a High + `status/blocked`. The gate never merges a blocked PR, and an author never approves over an open High.
- Builders and helpers don't click merge; only Compass, Keel and the senior (Codex Lead or Coda) merge directly (`docs/planner/LEADS.md` "Authority"). The merge gate (`.github/workflows/merge_gate.yml` + `tools/merge_gate.py`) enables auto-merge; GitHub waits for the required checks. Stacked PRs wait for retargeting to `main`, then re-run their acceptance against `main`.
- Owner-only: licence changes, `legacy/**`, `docs/history/**`, branch protection, real-device results, `allow-test-removal`.

## 5. Decisions a builder may make alone
Record each under `Self-decisions` in the PR body and the handoff:
- size up to +50% over the task's budget, tests included;
- test expectation updates that follow directly from the intended behaviour (with a one-line reason; never loosen tolerances, thresholds or hashes);
- merging `main` in and resolving conflicts by keeping both sides;
- adding `ci/pi`, `ci/pi-soak`, `ci/pi-retry`, and retrying an infrastructure failure once;
- picking among options the task or plan already lists; small refactors inside touched files;
- design choices the spec leaves open that change no IDs, formats, ABI, protocol or realtime behaviour.

Everything else in `AGENTS.md` "Stops" goes one tier up via the issue (helpers → builders/seniors; builders → Codex Lead or Coda; seniors → Keel for technical stops, Compass for plan stops). Only that item blocks; take another item meanwhile.

## 6. Task files and acceptance blocks
Every issue points to a `docs/tasks/` file ending in a fenced `acceptance` block that `tools/dev.py accept` parses:
````
```acceptance
tier: T1
size: <= 600 changed lines, <= 12 files
must_pass:
  - ctest -R host_protocol_
  - pi-run: ctest --test-dir build/native -R host_protocol_ --output-on-failure
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
````
Only `pi-run: <command>` lines are executable Pi acceptance. They require the PR label
`ci/pi-accept` and a matching exact-head `pi5/accept` result from the private runner.
`pi:` is descriptive, **never executed** or counted as passed. Do not mechanically
migrate existing `pi:` prose into commands: rewrite it when the task is next touched.
`ci/pi` alone runs the Pi build/tests, **not** task-specific acceptance; a green
build never substitutes for required task evidence.

Local `python tools/dev.py accept <issue#>` classifies local commands, `ci:`,
`pi-run:`, `pi:`, and `user:`. External requirements are listed as *not
verified here* and make the aggregate `incomplete`, never `passed`.
The default exit code retains historical local compatibility (zero for
incomplete-only); `--strict` exits nonzero for incomplete evidence. Unknown
prefixes, empty external requirements and failed local checks are errors.
Owner-only checks (real sound, MIDI devices, the display) remain under `owner:`
in the task text and are collected at checkpoints.

## 7. Checkpoints
At the end of Phases 1, 2 and 4 (`docs/PLAN.md`): tag `checkpoint-<X>`, build the review pack for the phase, and hold one owner + Compass session. Decisions go at the top of `docs/PLAN.md`. Work on the next phase continues during the review; a High blocks only the sections it touches.

## 8. Usage hygiene
- **Read less.** Every session pays for what it loads. AGENTS.md is the only always-loaded file; briefs live in `docs/planner/LEADS.md`, area rules in nested guides (`apps/studio/AGENTS.md`). Builders read PLAN "Now" only; leads add "Decisions". Task files stay ≤ ~3 KB (long background goes in a linked design doc). Never read `docs/tasks/done/`, `legacy/`, `docs/history/` or generated files.
- **Queue mode.** When an item is green and handed off, take the next unclaimed item in your lane in the same session. Fewer cold starts means fewer re-reads of the rules. Stop at the run's item limit or near a usage limit.
- **One item per context.** Start a fresh thread or clear/compact between items; long threads re-send everything each turn. The handoff is the memory.
- **Bundle.** Target size/m. Group related small items (one review's doc fixes, packets that touch the same editor) into one issue and PR. If a diff is smaller than its handoff + review comments, it should have been bundled.
- **Batch T2 asks** into one comment per day, not per PR. Any agent of the other family may take it (`AGENTS.md` "Who may review"); Keel-only approvals go in one `Keel: T2 batch`.
- **Model per job:** Claude Opus for rulings and T2 only; Sonnet subagents for mechanical repo work inside a Claude session; Haiku for read-only search and log tails; Codex `-mini` at low/medium for helpers, rebases, formatting, CI triage; the full Codex model at high for realtime and engine bugs; ChatGPT chats for task files, briefs, test plans and log summaries.
- **Briefs live in issue comments**, not chat, so the owner pastes "work on #NNN" instead of a whole brief.
- Don't run the full suite locally when CI will. Run the tests for the files you touched, push, and read the CI summary. Pi evidence only for compiled-code changes; no re-runs after a merge from `main` that touches no compiled code.
- Don't poll CI; the gate merges on green. Near a usage limit: push, post handoffs, and leave paste-ready briefs for agents that still have budget (`docs/planner/LEADS.md`, Codex Lead wrap-up).
- STATE.md is updated inside the session's last real PR, not in a separate docs PR.

## 9. Not worth it
Agent-orchestration dashboards beyond the generated board section, scheduled runs that keep going with nothing to do, and per-agent queues. The board (issues + this plan) is the queue; `lane/helper` is a filter on it, not a separate queue. Scheduled helper runs are fine because they exit within a few calls when their lane is empty (`docs/planner/LEADS.md`, Helpers).
