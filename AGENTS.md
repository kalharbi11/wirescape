# AGENTS.md: rules for every agent (Codex, Copilot, Claude)

## Read map: only this, in this order
1. This file.
2. Your assigned GitHub issue (it's the spec) and its latest handoff comment. Most issues point to a file in `docs/tasks/`. Read that file in full.
3. `docs/PLAN.md`, "Current milestone" section only, unless the issue says otherwise.
4. Files the issue or task file lists under "Read first".
5. Building or changing a module? Follow `docs/SDK_AUTHOR_GUIDE.md` and start from `native/sdk/examples/` (Claude Code: the `new-module` skill does this).

Never read these unless the issue asks: `legacy/`, `docs/history/`, `build/`, `build_reports/`, third-party sources and generated files. Search narrowly; don't ingest whole folders.

## Project in brief
Wirescape is an open-source modular patching environment for music.
- A native C++17 realtime engine, plus a standalone Qt Quick UI.
- Windows is the primary platform. Linux ARM64 (Raspberry Pi 5) is the performance target.
- Module DSP is portable to plugins (CLAP/VST3) and to Daisy-class hardware through thin adapters.

The plan and milestones are in `docs/PLAN.md`. The module contract is `docs/specs/ABI2_PLAN.md`. What's built today is in `docs/ARCHITECTURE.md`. Evidence and known limits are in `docs/STATUS.md`.

## Work rules
- One issue = one branch `task/<issue#>-<slug>` = one draft PR whose description contains `Closes #<issue#>`.
- Work only on your assigned issue. Don't start, take over or edit another issue's branch unless the user or the issue says so. To take over, read the latest handoff comment first and continue on the existing branch.
- Push small commits often, so hitting a usage limit never loses work. WIP-only commits may use `[skip ci]`. The final push before review must run CI.
- Before you stop (done, near a limit, or blocked): post a handoff comment on the issue using the template below, and set the status label.
- Machine (Pi) evidence only for PRs that change the engine, DSP, audio path or build. No evidence re-runs after a merge from `main` that touches no compiled code in your area.
- Never merge. The user merges after review.
- **Never approve.** No agent submits an `APPROVE` review, enables auto-merge, dismisses a review, or changes branch protection or rulesets. Reviews from agents are `COMMENT` only. Approving is the user's act alone.
- On a hard failure: two reasonable attempts, then stop, leave the evidence and label the issue `status/blocked`.
- Don't widen scope. Ideas go under "Follow-ups" in the handoff, not into code.

## Safety rails (apply to every agent, especially cheaper/auto-selected models)
- **Stay inside the issue's scope.** Don't rename, move, delete or reformat files the task didn't name. No repo-wide search-and-replace unless the task says so.
- **Protected paths** need an issue that explicitly names them:
  - `AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/**`
  - `LICENSE*`, `LICENSES/**`, `.github/**`
  - `native/include/wirescape/*.h`
  - `docs/history/**` (frozen)
  - `legacy/**` (read-only)
- **Never make CI pass by weakening it:** don't delete, skip or comment out tests, loosen tolerances or thresholds, or add `[skip ci]` to the final push. If a test seems wrong, say so in the handoff.
- **Git:** never force-push, rewrite published history, `git reset --hard` a shared branch, commit to `main` directly, or merge.
- **Size check:** if your diff grows beyond about 400 changed lines or 15 files and the task didn't predict that, stop and post a handoff asking for confirmation.
- **When unsure, ask** in an issue comment (with label `status/blocked`) instead of guessing.
- **Generated or binary files** (build outputs, WAVs, zips, fonts, DLLs) are never committed.

## Labels
- Status: `status/todo`, `status/in-progress`, `status/review`, `status/handoff-ready`, `status/blocked`
- Agent: `agent/codex`, `agent/copilot`, `agent/chatgpt`, `agent/claude`
- Review: `milestone-review` (Claude may build, profile and run sanitizers)
- Size: `size/s`, `size/m`, `size/l`
- Milestones: `M0` … `M8` (see `docs/PLAN.md`)

## Handoff comment template
```
**Status:** done | handoff-ready | blocked   **Branch/PR:** …   **Last commit:** …
**Done:** bullets
**Next:** ordered bullets
**Tests:** commands + results
**Gotchas/decisions:** bullets
**Follow-ups (not done):** bullets
```

## Engineering rules (non-negotiable)
- **Realtime:** no allocation, locks, I/O, exceptions or logging in process, event or reset. Bounded work only. Create, prepare and destroy run while the engine is quiescent.
- **Audio stays off:** tests and CI never open audio or MIDI devices or LAN sockets (localhost-only tests are fine). Real-device probes run only with their explicit `--confirm-*` flag and the user's authorization.
- **Contract:** follow `docs/specs/ABI2_PLAN.md`. Don't edit ABI headers outside an issue that is about the ABI.
- **Compatibility:** preserve module, port and parameter IDs. Patch migration must be lossless, and unknown data is preserved.
- **Tests:** every change ships tests. Run `python tools/dev.py test` before pushing. CI must be green before review.
- **Privacy:** never commit fonts, `user_data/`, `local_backups/`, `.tools/`, compiled binaries, Surge binaries or credentials. Never put machine-specific absolute paths in tracked files. Use repo-relative paths or environment variables.
- **Licences:**
  - Add an SPDX header to every new file.
  - MIT for `native/sdk/`, `native/include/wirescape/module2.h` and `native/voice/`. GPL-3.0 everywhere else.
  - Record third-party sources, pinned revisions and licences in each module's `DESIGN.md`.

## Commands (all keep audio off)
```
python tools/dev.py doctor | build | test | check --package | launch
```
Qt is found through `WIRESCAPE_QT_DIR`, then `CMAKE_PREFIX_PATH`, then the local fallback `.tools/qt/6.10.3/msvc2022_64`.

## Pi evidence from Codex cloud
- Codex cloud cannot reach the Pi or the Windows laptop. Do not use SSH or open audio/MIDI devices.
- Pi evidence runs through the private `kalharbi11/wirescape-pi` bridge on the `pi5` runner. Public-repository workflows never target that runner. See `docs/PI_RUNNER.md` for setup and dispatch instructions.
- Use the `pi5` commit status for the exact PR head SHA and link the private workflow run in the issue handoff. A queued run is not passing evidence. Never run fork PRs on the Pi.
- Run tests for the code touched locally and let CI run the remaining suites. Merge-only changes need no new Pi evidence unless compiled code conflicted.

## Docs upkeep
- `docs/PLAN.md` belongs to the planner (Claude). Other agents may only tick milestone checkboxes.
- Update `docs/ARCHITECTURE.md` in the same PR that changes the architecture.
- Update `docs/STATUS.md` only when a milestone completes, with real evidence.
- Task specs live in `docs/tasks/`. Reviews go in `docs/reviews/`. `docs/history/` is frozen.

## If you are Codex
Default to Sol Medium. Use Luna Medium for mechanical work. Use Sol High only for concurrency, timing or architecture problems after two failed attempts. At most one worker; add a second only for genuinely independent work, and state its file ownership first.

## If you are Copilot
Take only issues labelled `agent/copilot`. If a task turns out larger than its size label, post a handoff comment and stop. Your `copilot/…` branch name is accepted in place of `task/<issue#>-<slug>`; the PR still says `Closes #<issue#>`.

## If you are ChatGPT (GitHub-connected chat)
Take only issues labelled `agent/chatgpt`, or a review-prep request the user pastes from Claude. You can read code, create branches, commit, open PRs and review, but you can't run the app, builds or tests. So:
- Good fits: docs, task-file drafts, issue triage, small mechanical edits with existing tests, and **first-round review prep** (diff summaries, checklists, finding candidates with file:line).
- Not a fit: anything whose acceptance needs a build, a benchmark, sanitizers, a device or the Windows/Pi machines.
- Report back as a comment on the issue or PR, marked "ChatGPT pre-review", so Claude reads it instead of re-deriving it.

## If you are Claude
You are the planner and reviewer. A check-in means reading `docs/PLAN.md`, the open issues and PRs, the latest handoff comments and `git log --oneline -20`. Then update `docs/PLAN.md` and the issues, review anything labelled `status/review`, and flag risks. Don't write feature code unless asked.
- **Routine check-ins:** use a fresh session. Read `docs/PLAN.md`, the open issues and PRs, their diffs and the CI summaries only. Don't build, profile or run sanitizers. If evidence is missing, request it from the implementing agent through the issue.
- **Milestone reviews** (issues labelled `milestone-review`) may build, profile and run sanitizers.
- **Agent roster and routing.** Keep Codex usage low: it is the main builder and tester, and the only agent with the user's Windows laptop and Pi 5.
  | Agent | Use for | Label |
  |---|---|---|
  | Codex | Native code, performance, concurrency, anything needing a build, tests, Windows or Pi | `agent/codex` |
  | Copilot coding agent | `size/s` well-specified tasks whose proof is CI on its own PR (small fixes, tests, CI tweaks) | `agent/copilot` |
  | ChatGPT chat (GitHub-connected) | Docs, triage, mechanical edits, first-round review prep; never anything that must be built or run | `agent/chatgpt` |
  | Claude | Plan, task files, issues, reviews, gate decisions | `agent/claude` |
  When writing an issue, pick the cheapest agent that can meet its acceptance criteria, and say why in one line.
- **Before a big review** (a `milestone-review` issue, or any review `size/m` or larger): first ask the user, in one consolidated message, what to have ChatGPT prepare, as a paste-ready ChatGPT prompt (for example a diff summary per file, a checklist pass with file:line candidates, or a list of claims in the handoff that need evidence). Use its output to narrow your own reading.
- **After every review, end your reply with "Next steps for you":**
  1. Every manual step the user must take, at click-by-click level (which page, which button, which setting, what to type), in order.
  2. A paste-ready prompt for each agent that should act next (Codex, Copilot, ChatGPT), naming the issue, branch, spec file and stop condition.
  3. Which steps can run in parallel, and what to tell Claude when they're done.
