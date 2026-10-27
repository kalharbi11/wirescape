# AGENTS.md: rules for every agent (Codex, Copilot, Claude)

## Read map: only this, in this order
1. This file.
2. Your assigned GitHub issue (it's the spec) and its latest handoff comment. Most issues point to a file in `docs/tasks/`. Read that file in full.
3. `docs/PLAN.md`, "Current milestone" section only, unless the issue says otherwise.
4. Files the issue or task file lists under "Read first".

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
- Never merge. The user merges after review.
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
- Agent: `agent/codex`, `agent/copilot`, `agent/claude`
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

## Docs upkeep
- `docs/PLAN.md` belongs to the planner (Claude). Other agents may only tick milestone checkboxes.
- Update `docs/ARCHITECTURE.md` in the same PR that changes the architecture.
- Update `docs/STATUS.md` only when a milestone completes, with real evidence.
- Task specs live in `docs/tasks/`. Reviews go in `docs/reviews/`. `docs/history/` is frozen.

## If you are Codex
Default to Sol Medium. Use Luna Medium for mechanical work. Use Sol High only for concurrency, timing or architecture problems after two failed attempts. At most one worker; add a second only for genuinely independent work, and state its file ownership first.

## If you are Copilot
Take only issues labelled `agent/copilot`. If a task turns out larger than its size label, post a handoff comment and stop.

## If you are Claude
You are the planner and reviewer. A check-in means reading `docs/PLAN.md`, the open issues and PRs, the latest handoff comments and `git log --oneline -20`. Then update `docs/PLAN.md` and the issues, review anything labelled `status/review`, and flag risks. Don't write feature code unless asked.

<!-- Temporary CI guard negative control; revert after evidence. -->
