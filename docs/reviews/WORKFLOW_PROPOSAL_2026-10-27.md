<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Workflow v2 proposal: same rigor, much less AI usage (2026-10-27)

For: the planner (main Claude Code), to fold into `AGENTS.md` / `docs/PLAN.md` after the user agrees.
Companion to `FRESH_EYES_REVIEW_2026-10-27.md` (the Jam checkpoint).

## 0. The idea in one paragraph

Move rigor **out of AI reading and into scripts that run for free**. The repo is public, so GitHub
Actions (including Linux ARM64 runners) cost nothing. Every task states its acceptance as
**commands with pass/fail output**. CI and a one-command acceptance script decide "done". AI reviews
only what scripts can't judge, and the expensive reviewer (Claude) only sees risky changes and
milestone gates, reading a compact **review pack** that cheaper reviewers have already gone over.

## 1. Agents and what each one is for

| Agent | Cost to the user | Use for | Never use for |
|---|---|---|---|
| **Codex cloud tasks** (chatgpt.com/codex, phone via the ChatGPT app), Sol medium | main budget | Building, fixing CI failures, the automation backlog (§5). Never SSHs into the Pi: Pi work goes through the Pi runner. Laptop-only work waits until the user opens the laptop. Luna for mechanical edits. Sol high only after two failed attempts | Reviewing its own risky changes; planning |
| **ChatGPT chat (GitHub-connected)** | flat subscription | **First-round reviewer** on every non-trivial PR (reads the review pack). **Independent module builder** for self-contained modules (it writes code + tests on a branch; free CI builds and tests it; it reads the CI result and fixes) | Anything that needs the Pi, the laptop or a device |
| **Claude Code** | most expensive per use | Planning, task files with machine acceptance, gate reviews, final call on High findings | Routine PR reviews, re-deriving what ChatGPT or the external queue already found |
| **Copilot (Student: Auto only, 200 credits/mo)** | nearly used up | Optional: tiny, fully test-specified fixes only | Anything where a weak model could pass tests by accident; never as Codex's sub-agent for real work |
| **Free Google CLI** (Gemini CLI / Antigravity CLI free tier, check current quota) | free | Optional extra reviewer Codex can call on a review pack | Writing code into the repo |

## 2. Review tiers (who must look before the user merges)

| Tier | Applies to | Required before merge |
|---|---|---|
| **T0: scripts only** | docs, tests-only, CI tweaks, `size/s` code with no protected paths | All CI green + `accept` report green. User skims and merges |
| **T1: scripts + one cheap review** | `size/m` modules, ports, tools, UI | T0 + ChatGPT first-round review on the review pack, with any High finding fixed or answered. No Claude |
| **T2: Claude** | ABI/SDK headers, engine2 scheduler, threading/rings, realtime callback, services thread model, patch format, CI guard, milestone gates, or any T1 where ChatGPT flagged a High it couldn't settle | T1 + Claude reads **only** the pack and ChatGPT's findings, then spot-checks named lines. For concurrency/DSP, ask ChatGPT for a second pass in a fresh chat with its strongest reasoning model before Claude |

Expected effect: Claude reviews drop from roughly one per PR to **one per milestone plus one or two
risky PRs**. Quality holds because the rules that matter (realtime, IDs, equivalence, no-alloc,
thresholds) are enforced by tests, sanitizers and the guard, not by someone remembering to look.

**"Codex approves itself" means:** when `accept` is green on the exact head, Codex sets
`status/ready-to-merge` and posts the report. It still never clicks Approve or merges. The user's
merge click stays the approval (unchanged rule).

## 3. Stops that stay (and new ones)

Keep every existing hard stop (ABI header edit, non-zero equivalence delta, test removal or
tolerance change, diff past budget, two failures on the same problem). Add:
- **Design choice not in the spec** → stop and ask in the issue. Don't pick silently.
- **New dependency** (library, FetchContent, tool) → stop and ask.
- **Attempt budget:** each task file states `budget: N Codex sessions`. Hitting it → handoff, stop.
- **No evidence re-runs** after merging `main` into a branch when no compiled file changed
  (CI on the new head is enough).
- **No local full test runs when CI will run them anyway.** Codex runs only the tests for the files
  it touched locally, pushes, and reads the CI summary. Pi runs go to the Pi runner (§5g).

## 4. Usage hygiene

**Codex**
- One issue per thread; start a fresh thread per issue. Long threads re-read everything each turn.
- Read map stays narrow (AGENTS.md → issue → task file → listed files).
- Never paste whole logs into context: tools print a short JSON summary; read full logs only on failure, tail first.

**Claude Code**
- **Fresh session per check-in or review**, `/clear` between unrelated topics. One long session
  keeps paying for old context and eventually auto-compacts away details.
- Default model **Sonnet** for routine check-ins, issue writing and plan edits. **Opus (medium)**
  only for T2 reviews and architecture decisions. Search/sweep sub-agents on **Haiku**.
- Start every session from a ≤30-line **"Now"** section at the top of `docs/PLAN.md` (current
  milestone, open PRs and their state, next 3 actions). Update it at the end of every session.
  That alone stops the stale-queue problem (items already merged being re-assigned).
- Reviews start from the review pack, never from a full-repo read.

**ChatGPT**
- Paste or attach the review pack; ask for findings by severity with file:line and a suggested
  test that would catch each one. Findings go into the PR as "ChatGPT pre-review".

## 5. Automation backlog for Codex (one-time, mostly non-AI; do in this order)

| # | Item | Why it saves usage |
|---|---|---|
| a | **`tools/dev.py accept <issue#>`**: reads a fenced `acceptance` block from the task file (commands + expected results: CTest names, thresholds, hashes, benchmark limits), runs them, writes `build_reports/accept-<issue>.json` + a Markdown summary. CI runs it and posts the summary as the job summary | "Done" is decided by a script, not by AI reading |
| b | **`tools/review_pack.py <PR#>`**: one Markdown file under a size cap: PR intent, diff stat, the diff (generated/test-fixture noise trimmed), the task's acceptance block, the CI/accept summary, and a fixed checklist (realtime, IDs, state, thresholds, licences). | Every reviewer (ChatGPT, Claude) reads the same small file instead of exploring |
| c | **Static analysis job**: clang-tidy (diff-only via `clang-tidy-diff`), cppcheck, clang-format check, `actionlint`, `ruff` for tools. Start non-required; make required once clean | Catches a class of review findings for free |
| d | **CodeQL** default setup (free for public repos) | Free security/UB-ish scanning |
| e | **Benchmark regression gate**: commit a baseline JSON per runner; CI compares and fails past a stated threshold, with the threshold change itself protected by the guard | Perf "reviews" become automatic |
| f | **Golden render hashes** for every example patch and each module's default render (extends what exists) | Equivalence checks without listening or reading |
| g | **Self-hosted Pi 5 runner** (the Pi is a 24/7 worker, never an agent host): PR jobs on own branches (headless build + tests, exact head); **nightly** full tests, soaks, benchmark tracking vs baseline, render-hash checks, long fuzzing, full clang-tidy; builds review packs for every PR; posts one nightly summary issue. "Require approval for all outside collaborators"; never runs fork PRs; no audio/MIDI devices | Pi evidence with zero agent usage |
| h | **Fuzz job** (libFuzzer on clang, 60 s time box) for the OSC/UDP codecs once they exist | Network robustness without AI review |
| i | **pre-commit hooks** mirroring the guard (SPDX, format, binaries, protected paths) | Fails fast locally before a CI round trip |
| j | **PR template** with the acceptance checklist and tier (T0/T1/T2) | Makes the tier explicit |
| k | **Docs freshness check**: script that flags PLAN "Now" entries pointing at merged/closed PRs | Prevents stale queues being handed to agents |

## 6. Task-file format (what Claude writes from now on)

Each task file ends with:
```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 600 changed lines, <= 12 files
must_pass:
  - ctest -R chaos_lite_            # all
  - python tools/dev.py bench --compare baseline/linux-arm64.json --max-regress 5%
  - render examples/chaos_two.json --hash-match goldens/chaos_two.sha256
  - pi: soak --graph examples/chaos_two.json --seconds 120 --block 128 --p99-under 50%
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
If Claude can't write the acceptance block for a task, the task isn't ready to hand out.

## 6b. External agent queue (a few times a week, optional)

For work that fits in **at most 3 files** and benefits from an independent strong model
(second opinions on T2 PRs, DSP math checks, test-case brainstorming): the user runs it in
any outside AI chat they have access to. No agent depends on it.
1. The agent builds the pack (`review_pack.py --split 3`), with the exact prompt at the top
   of the first file, and **asks the user** in the issue: "External-queue task: run it outside,
   or should I do it myself?"
2. **Blocking work:** ask once, then do it internally and note that an external pass is still
   available. **Non-blocking work:** label the issue `external-queue` and move on.
3. Packs live as `.md` files on a separate `external-queue` branch (never `main`), so the user can
   download them from the GitHub phone app.
4. The user pastes the answer back as a comment headed **"External result"**; the next agent
   session acts on it.
5. A weekly issue lists everything waiting.

## 7. Weekly rhythm (suggested)

1. **Planner session** (Claude Sonnet, fresh): update "Now", write/shape the next issues with acceptance blocks.
2. **Codex** works the queue; ChatGPT builds independent modules in parallel on separate branches.
3. **ChatGPT** first-round reviews T1/T2 PRs (a second fresh pass on T2).
4. **User** merges T0/T1 when green.
5. **Gate review** (Claude Opus, fresh) only at milestone ends or on a T2 escalation.

## 8. Things not worth it

- Agent-orchestration apps (parallel agent dashboards, kanban runners): with $20 plans they mainly
  multiply spend by running more agents at once. GitHub Issues + a free Projects board view already
  give the board.
- Using Copilot Auto as a sub-agent for real work: its model choice is not controllable, so the
  result needs a stronger review, which costs more than it saves.
