# AGENTS.md: rules for every agent (workflow v2, 2026-10-28)

Detail behind these rules: `docs/WORKFLOW.md`. Plan and current state: `docs/PLAN.md` ("Now" at the top).

## Read map: only this, in this order
1. This file.
2. Your issue and its latest handoff comment, then the `docs/tasks/` file it names (in full; it ends with an `acceptance` block).
3. `docs/PLAN.md`, the "Now" section only.
4. Files the task lists under "Read first". Building a module: `docs/SDK_AUTHOR_GUIDE.md` and `native/sdk/examples/` (Claude Code: the `new-module` skill).

Never read `legacy/`, `docs/history/`, `build/`, `build_reports/`, third-party sources or generated files unless the task asks. Search narrowly.

## Project in brief
Wirescape is an open-source modular patching environment for music: a native C++17 realtime engine and a Qt Quick UI. Windows is the primary platform; Linux ARM64 (Raspberry Pi 5) is the performance target; module DSP stays portable to plugins and Daisy-class hardware. Contract: `docs/specs/ABI2_PLAN.md`. Built today: `docs/ARCHITECTURE.md`. Evidence: `docs/STATUS.md`.

## Agents and routing
| Agent | Runs | Use for | Label |
|---|---|---|---|
| Codex cloud tasks | cloud | building, CI fixes, the automation backlog, anything that needs a build | `agent/codex` |
| ChatGPT (GitHub-connected) | cloud | docs, triage, self-contained modules (CI is its build), first-round "ChatGPT pre-review" | `agent/chatgpt` |
| Claude Code (web) | cloud | plan, task files with acceptance blocks, T2 reviews, gate decisions | `agent/claude` |
| Pi 5 runner | Pi, no AI | PR checks, nightly tests, soaks, benchmarks, fuzzing, static analysis, review packs | — |
| Copilot | — | nothing for now (credits nearly gone) | — |

No agent reaches the Pi or the laptop. Pi work goes through the `pi5` runner on the private repo `kalharbi11/wirescape-pi`; this repo's workflows never target the Pi. Laptop-only work waits for the user. Pick the cheapest agent that can meet the acceptance block, and say why in one line.

## Work rules
- One issue = one branch `task/<issue#>-<slug>` = one draft PR with `Closes #<issue#>`. Work only on your issue.
- Push small commits; WIP commits may use `[skip ci]`, the final push never does.
- **Done is decided by the acceptance block:** run its `must_pass` lines (`python tools/dev.py accept <issue#>` once it exists), post the report, set `status/ready-to-merge` when green on the exact head. `pi:` lines run on the Pi runner.
- **Review tiers** (`docs/WORKFLOW.md` §3): T0 scripts only; T1 adds a ChatGPT pre-review of the review pack; T2 adds Claude. The tier is in the acceptance block.
- **Merging** (`docs/WORKFLOW.md` §3b): a builder may **enable GitHub auto-merge** (any method; squash is fine) on its own PR only when all hold: base is `main`; not a draft; tier T0, or T1 with a "ChatGPT pre-review" whose Highs are all fixed or answered; no label-only path in the diff; `status/ready-to-merge` set after a green acceptance run. GitHub then merges only when the required checks pass. T2 PRs and PRs touching label-only paths may auto-merge only after Claude's comment says `Claude: approved for auto-merge` on the current head; stacked PRs wait until GitHub retargets them to `main`. Never click merge directly, approve, dismiss reviews or change branch protection. Agent reviews are `COMMENT` only.
- **Check-ins** (`docs/WORKFLOW.md` §3d): Claude's scheduled check-ins are listed there. Each ends with an `@codex` comment on #29 that starts Codex on the next queue; if a listed check-in passed without one, Codex re-checks 30 minutes later and continues anyway.
- **Autonomous mode** (`docs/WORKFLOW.md` §3c) is the default: never idle while waiting, make the listed self-decisions and record them, self-review with the review pack before `status/ready-to-merge`.
- Before you stop (done, near a limit, blocked): post the handoff below and set the status label.
- Don't widen scope; ideas go under "Follow-ups".

## Stops: post a handoff, label `status/blocked`, take the next item
ABI header edit · equivalence delta without a planner decision · test removal or tolerance/threshold change · diff past the task's size budget · two failed attempts at one failure · design choice not in the spec · new dependency not named in the task · attempt budget reached. When unsure, ask in the issue.

## Usage hygiene
- One issue per agent thread; start fresh per issue. Don't paste whole logs; tail on failure.
- Don't run the full suite locally when CI will; run tests for touched files, push, read the CI summary.
- Pi evidence only through the Pi runner and only for engine, DSP, audio-path or build changes. No re-runs after a merge from `main` that touches no compiled code.
- Claude: fresh session per check-in; Sonnet for routine work, Opus only for T2/architecture, Haiku for search.

## External agent queue (optional)
For ≤3-file work that benefits from an independent strong model: build a split review pack, ask in the issue "External-queue task: run it outside, or should I do it myself?". Blocking work: ask once, then do it yourself. Non-blocking: label `external-queue` and move on. The user replies with a comment headed "External result". Detail: `docs/WORKFLOW.md` §7.

## Safety rails
- **Protected paths** need either a linked issue (`Closes #N`) with a line starting `Protected path named:` followed by the path or glob in backticks (the guard reads it; no label needed), or the `allow-protected` label (user or planner). Only the planner writes those issue lines; builders never edit them. Always label-only: `AGENTS.md`, `CLAUDE.md`, `tools/ci/**`, `.github/workflows/guard.yml`, `.github/CODEOWNERS`, ABI headers, licences, `docs/history/**`, `legacy/**`. Protected: `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/**`, `LICENSE*`, `LICENSES/**`, `.github/**`, `native/include/wirescape/*.h`, `docs/history/**` (frozen), `legacy/**` (read-only).
- Never make CI pass by weakening it. Never force-push, rewrite published history, commit to `main` or merge.
- Never commit generated or binary files, fonts, `user_data/`, `local_backups/`, `.tools/`, credentials or machine-specific absolute paths.

## Engineering rules (non-negotiable)
- **Realtime:** no allocation, locks, I/O, exceptions or logging in process, event or reset. Bounded work. Create/prepare/destroy run while the engine is quiescent.
- **Devices off:** tests, CI, agents and the Pi runner never open audio/MIDI devices or LAN sockets (localhost is fine). Real-device paths only behind `--confirm-*`, run by the user.
- **Contract:** follow `docs/specs/ABI2_PLAN.md`. ABI headers change only in an ABI issue. ABI soft freeze from the Jam checkpoint: additive SDK changes only, otherwise a migration note.
- **Compatibility:** module, port and parameter IDs never change; patch migration is lossless; unknown data is preserved.
- **Tests:** every change ships tests.
- **Licences:** SPDX header on every new file. MIT for `native/sdk/`, `native/include/wirescape/module2.h`, `native/voice/`; GPL-3.0 elsewhere. Third-party sources, pinned revisions and licences go in the module's `DESIGN.md`.

## Labels
Status `status/todo|in-progress|review|ready-to-merge|handoff-ready|blocked` · Agent `agent/codex|chatgpt|claude` · Tier `tier/T0|T1|T2` · Size `size/s|m|l` · `external-queue` · `milestone-review` · overrides `allow-protected`, `allow-test-removal` (user or planner only).

## Handoff comment template
```
**Status:** done | handoff-ready | blocked   **Branch/PR:** …   **Last commit:** …
**Done:** bullets
**Next:** ordered bullets
**Tests:** commands + results (acceptance report link)
**Gotchas/decisions:** bullets
**Follow-ups (not done):** bullets
```

## Commands (all keep audio off)
`python tools/dev.py doctor | build [--headless] | test | check --package | launch`. Qt: `WIRESCAPE_QT_DIR`, then `CMAKE_PREFIX_PATH`.
**Cloud containers (Codex, Claude):** if `cmake` or `ninja` is missing, run the repo's setup script first (`tools/cloud/setup.sh`, or `tools/ci/codex_cloud_setup.sh` until #83 moves it) and use the PATH it prints. Fix setup problems in that script, never by asking the user to change environment settings.

## Pi evidence from Codex cloud
- Codex cloud cannot reach the Pi or the Windows laptop. Do not use SSH or open audio/MIDI devices.
- Pi evidence runs through the private `kalharbi11/wirescape-pi` bridge on the `pi5` runner. Public-repository workflows never target that runner. See `docs/PI_RUNNER.md` for setup and dispatch instructions.
- Use `pi5` for exact-head build/tests. Label real-loader soak requests `ci/pi-soak` (drafts also need `ci/pi`) and require `pi5/soak`. After fixing an infrastructure failure, `ci/pi-retry` requests one rerun on the same head; the bridge consumes the label after publishing results. Link the private workflow run; queued runs are not evidence. Never run fork PRs on the Pi.
- Run tests for the code touched locally and let CI run the remaining suites. Merge-only changes need no new Pi evidence unless compiled code conflicted.

## Docs upkeep
`docs/PLAN.md` belongs to the planner (others may tick checkboxes). Update `docs/ARCHITECTURE.md` in the PR that changes the architecture; `docs/STATUS.md` when a milestone completes. Tasks in `docs/tasks/`, reviews in `docs/reviews/`.

## If you are Codex
Sol medium by default, Luna for mechanical edits, Sol high only after two failed attempts. At most one extra worker, only for independent work with stated file ownership.

## If you are ChatGPT
Take `agent/chatgpt` issues and review-pack requests. You can't run builds: CI on your branch is your build; read its summary and fix. Post reviews as "ChatGPT pre-review" with severity and file:line.

## If you are Claude
Planner and T2 reviewer. A check-in reads "Now", open issues/PRs, latest handoffs and CI summaries, then updates "Now", the issues and task files (each with an acceptance block). Don't write feature code unless asked. Don't build in routine check-ins; gate reviews (`milestone-review`) may. After every review, end with "Next steps for you": the user's click-level steps in order, a paste-ready prompt per agent that acts next (issue, branch, task file, stop condition), and what can run in parallel.
