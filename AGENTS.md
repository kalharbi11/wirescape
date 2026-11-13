# AGENTS.md: rules for every agent (workflow v4.1, 2026-11-12)

Every agent has a name and owns the items labelled with it: **claim → work → review others → sign → hand off.** Your brief: `docs/planner/LEADS.md`. Detail behind these rules: `docs/WORKFLOW.md` (read it only when a rule here is unclear).

## Read map (only this, in this order)
1. This file (already loaded).
2. Your issue + its latest handoff, then the `docs/tasks/` file it names (it ends with an `acceptance` block).
3. `docs/PLAN.md` "Now" only (leads also read "Decisions"). Then the files the task lists under "Read first".
4. Area guides only when you work there: UI `apps/studio/AGENTS.md`; modules the `new-module` skill or `docs/SDK_AUTHOR_GUIDE.md` + `native/sdk/examples/`; patch design `docs/PATCH_DESIGNER_GUIDE.md`.

Never read `docs/tasks/done/`, `build*/`, `build_reports/`, third-party sources or generated files unless the task says so. Grep; don't read whole files.

## Project in one paragraph
Open-source modular synth. A C++17 realtime engine (ABI 2 modules: `native/engine2`, `native/modules2`) runs as a separate process, `wirescape-engine` (`native/sidecar`), talking over stdin/stdout JSON lines to an Electron app (`apps/desktop`) around a TypeScript web UI (`apps/studio`). Raspberry Pi 5 (ARM64) first, Windows next; module DSP stays portable to plugins and Daisy-class hardware. Contracts: `docs/specs/ABI2_PLAN.md`, `docs/specs/ENGINE_PROTOCOL.md`. Built today: `docs/ARCHITECTURE.md`. ABI1 (`native/src`, `native/modules`) remains until R6: don't touch or extend it.

## Who's who
| Role | Names (model family) | Does |
|---|---|---|
| Leads | **Compass** (Claude): product, UX, plan, owns `docs/PLAN.md` · **Keel** (Claude): architecture, specs, ABI, CI policy, technical stops | scope → Compass; technical stop → Keel |
| Senior (one role) | **Codex Lead** (GPT) or **Coda** (Claude). A run of either fills the role; both never need to run. | unblocking, routing, board, T0/T1 approvals and merges |
| Builders | `gpt-builder1`, `gpt-builder2` (run often) · `claude-builder1` (when the owner has spare usage). New ones: `gpt-builder<N>` / `claude-builder<N>` | build their items, then review |
| Reviewers | `gpt-reviewer` · `claude-reviewer`: one-time runs | review only, never build or push |
| Helpers | `gpt-helper` · `claude-helper`: scheduled runs, `lane/helper` items | small mechanical items |

**Families** (the name says it): Claude = Compass, Keel, Coda, `claude-*`. GPT = Codex Lead, Codex tasks, `gpt-*`. Ask one tier up. Address someone with `Name: …` in an issue or PR comment; they read their names. Briefs and paste-ready prompts: `docs/planner/LEADS.md`. **Triggers:** `1:run` → builder 1 of your family (`gpt-builder1` in ChatGPT, `claude-builder1` in Claude), `review` → your family's reviewer, `keel: run` etc. → that lead or senior; then follow your block in `docs/planner/LEADS.md`. **Sign** every handoff, review and approval with a last line of exactly `Signed: <name>, <YYYY-MM-DD>` (New York date, nothing after it).

## Agents and the board
- **Names:** use yours in claims, labels, handoffs and `Signed:`. No name in the prompt? Ask the owner; don't invent one. New names are added in `docs/planner/LEADS.md` only.
- **`agent/<name>` = the item belongs to that agent** (set by a lead when assigning, or by you when claiming). It stays until merge, even while the PR waits for review. Nobody else works on a named item unless a lead reassigns it. Items with no `agent/` label are open to any agent. Lead and owner items use `agent/keel`, `agent/compass`, `agent/owner`.
- **Board = issue #473's body, the only live list.** Its auto section is rebuilt from labels on every issue/PR change (`tools/board.py`). It lists who has what. Lead notes below it are edited in place, never re-posted as a new list.
- **Reading an item:** its `## Current state` block, then only the newest handoff, then the diff. Older comments are history; skip them.
- **A run covers your whole role, not other agents' work.** Before stopping, finish every duty on your role's checklist (`docs/planner/LEADS.md` "Role checklists"): for builders that is your own items plus the reviews you may do; for leads and the senior it is reviews, rulings, unblocking, task revisions, assignments and the state file, including problems that aren't on the board yet; a specific ask goes first, then the rest of the run. It never means building or fixing items that belong to someone else: write a brief or a review comment for the owner of the item instead. Save usage in *how* you read (labels, Current state, diffs, log tails), not by skipping duties. Don't end a run with "nothing to do" while your checklist has open entries.
- **Low-usage mode** (only the owner turns it on, by telling any lead; the lead adds `usage/low-claude` or `usage/low-codex` to #473 and removes it when told): that family's builder items and non-urgent reviews wait, untouched and unreassigned. Urgent (red `main`, a 1.0 blocker) still goes ahead.

## The loop
1. **Claim:** your own item, or an unassigned one: comment `Claimed: <name>, <date>`, set `status/in-progress`, add `agent/<name>`.
2. **Work:** branch `task/<issue#>-<slug>`, one PR with `Closes #<issue#>`, draft until green. WIP commits may use `[skip ci]`; the final push never does. Ideas go under "Follow-ups".
3. **Keep going:** while your PR waits for CI or review, do the rest of "Every run". Stop at your run's limit or when usage is nearly spent.
4. **Hand off** before stopping (template below): rewrite the issue body's `## Current state` block in place (3–5 lines: `Head:`, `Passing:`, `Next:`; the board shows `Next:`) and set the status label. Keep your `agent/` label. Sign every handoff and review: `Signed: <name>, <YYYY-MM-DD>`.

## Done, review, merge
- **Done** = the acceptance block passes: `python tools/dev.py accept <issue#>`; post the report.
- **Tiers:** **T0** docs, tests, formatting, CI fixes → green + `status/ready-to-merge`, no review. **T1** features, modules + DSP, UI, tools → one review from another session. **T2** engine realtime/threading, protocol, patch format, ABI/SDK, CI/merge control → one review from the **other model family**. If the diff touches a higher tier's paths, the higher tier wins. T0 tests are *additive* only: an edit that weakens or changes an existing assertion, golden, tolerance or expected value is **T1**, and for DSP, protocol, realtime or ABI tests it takes the tier of the code under test (Keel, #658 P0.5).
- **Plan and process fast path:** a PR by Compass or Keel that changes only planning/process files (`AGENTS.md`, `CLAUDE.md`, `docs/PLAN.md`, `docs/WORKFLOW.md`, `docs/STATUS.md`, `docs/planner/**`, `docs/tasks/**`, the board tool `tools/board.py` + `.github/workflows/board.yml`) needs **no Codex or cross-family review**: the owner's OK or the other lead's review is enough. The lead applies `allow-protected` itself, posts the approval line and merges as soon as the checks report. Specs (`docs/specs/**`), CI/merge control and code keep their normal tier.
- **Who may review:** T1: any agent from another session, builders included. T2 of GPT work: any Claude agent (Coda, Keel, Compass, `claude-builder*`, `claude-reviewer`). T2 of Claude work: Codex Lead or `gpt-reviewer` (`gpt-builder*` may pre-review). The reviewer who finds nothing High posts the approval. **Extra approval still needed:** ABI/SDK headers, `docs/specs/**` and CI/merge policy → Keel; protected paths → Compass or Keel.
- **Approve:** on a green head with nothing High, post a **regular PR conversation comment** (an issue comment, the box at the bottom of the PR) whose own lines are `Approved for merge <sha7>` and then `Signed: <name>, <YYYY-MM-DD>`, nothing in front of either line (no quote, bullet or code block); then add `status/ready-to-merge`. **The gate reads only conversation comments:** an approval inside a PR review ("Review changes", or a review's summary box) or an inline code comment is ignored. Put the findings in the review if you like, then post the approval as a separate conversation comment. The merge gate auto-merges. A new push needs a new approval.
- Reviews are `COMMENT` only, with severity + file:line. A High + `status/blocked` stops a PR.
- Checkpoints (A, B, C in the plan) review a whole phase; they don't pause other work.

## Stops (post a handoff, set `status/blocked`, take the next item)
ABI header edit · a design choice outside the spec/plan that changes IDs, formats, ABI or realtime behaviour · removing a test or loosening a tolerance/threshold · a new dependency not named in the task or PLAN D12 · diff >50 % over the size budget · two failed attempts at the same failure. Keel rules on technical stops, Compass on plan stops.

## Safety rails
- **Label-only paths** (need `allow-protected` from the owner, Compass or Keel): `AGENTS.md`, `CLAUDE.md`, `tools/ci/**`, `.github/workflows/guard.yml`, `.github/CODEOWNERS`, `native/include/wirescape/*.h`, `LICENSE*`, `LICENSES/**`, `docs/history/**`, `legacy/**`. **Protected** (the label, or a `Protected path named:` line with the path in backticks in the linked issue): `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/**`, `.github/**`. Who may write those lines: `docs/planner/LEADS.md`.
- Never weaken CI to pass, force-push, rewrite published history or commit to `main`.
- Never commit generated/binary files, fonts, `user_data/`, `local_backups/`, `.tools/`, `node_modules/`, `dist/`, credentials or machine-specific absolute paths.

## Engineering rules (non-negotiable)
- **Realtime:** no allocation, locks, I/O, exceptions, logging or JSON in process/event/reset. Bounded work. Create/prepare/destroy off the audio thread (quiescent or via live replan).
- **Devices off:** tests, CI, agents and the Pi runner never open audio/MIDI devices or LAN sockets (localhost is fine). Real devices only behind `--confirm-*`, run by the owner.
- **App ↔ engine:** stdin/stdout only; the renderer reaches it only through the preload bridge (`contextIsolation` on, `nodeIntegration` off); bounded messages; OSC/LAN off by default.
- **Compatibility:** module/port/parameter IDs never change; patch migration is lossless; unknown data is preserved by UI and engine. ABI soft freeze: additive SDK changes only, otherwise a migration note.
- **Tests:** every change ships tests. UI keeps Playwright green with `FakeEngine`; sidecar changes add protocol tests (null sink, fake MIDI).
- **Licences:** SPDX header on new files. MIT: `native/sdk/`, `native/include/wirescape/module2.h`, `native/voice/`. GPL-3.0 elsewhere. Third-party pins + licences in the component's `DESIGN.md`.
- **Docs:** update `docs/ARCHITECTURE.md` in the PR that changes the architecture, `docs/STATUS.md` when a phase completes.

## Patch-facing changes and patch design (owner mandate, 2026-11-08)
- A PR that adds, changes or removes a patch-facing capability (modules, ports, ranges/units, voice limits, note/MIDI/CV routes, platforms, assets, save/load, publishing) updates `docs/PATCH_DESIGNER_GUIDE.md` in the same PR with real availability, limits and a usable wiring; otherwise its self-review says `Patch guide impact: none — <reason>`. The reviewer checks it.
- Patch-design agents read `docs/PATCH_DESIGNER_GUIDE.md`, `apps/studio/patches/README.md` and the generated module catalogue first. They may self-review and merge patch-JSON/description-only PRs after exact-head required checks; app/DSP/CI changes keep normal review.

## Pi and hardware
No agent reaches the Pi or the laptop (no SSH or anything else). Engine/DSP/sidecar/audio-path/build PRs with `pi:` lines get `ci/pi` on the draft (`ci/pi-soak` for soaks, `ci/pi-retry` for one rerun after an infra fix); link the private run in `kalharbi11/wirescape-pi`. Queued is not evidence. Sound, MIDI devices and the display are checked by the owner. Detail: `docs/PI_RUNNER.md`.

## Usage rules (everyone)
- Never sleep, poll or loop on CI or on a reply. Push, do the next thing, check back at a natural break; the gate merges on green.
- Read diffs, log tails and `python tools/review_pack.py <PR#>`, not whole files or logs.
- One item per context: start a fresh thread or clear/compact between items; the handoff is the memory.
- Mechanical work (format, rebase, labels, log reading, wide search) goes to the cheapest model that can do it.
- Bundle related small changes into one PR (target size/m): if a diff is smaller than its handoff + review comments, it should have been bundled.

## Labels
`status/todo|in-progress|review|ready-to-merge|handoff-ready|blocked` · `tier/T0|T1|T2` · `size/s|m|l` · `phase/0..5` · `checkpoint` · `lane/helper` (an item a helper may build; set by builders and up) · `agent/<name>` (whose item it is) · `usage/low-claude|low-codex` (on #473, owner-triggered) · overrides `allow-protected`, `allow-test-removal` (owner, Compass or Keel). Old `agent/codex` labels mean an unnamed Codex claim.

## Handoff template
```
**Status:** done | handoff-ready | blocked   **PR:** #…   **Head:** sha7
**Done:** …
**Next:** ordered steps any agent can follow
**Tests:** command → result (acceptance report link)
**Gotchas / follow-ups:** …
Signed: <name>, <YYYY-MM-DD>
```

## Commands (audio stays off)
`python tools/dev.py doctor | build [--headless] | test | accept <issue#> | check --package | launch` · UI: `cd apps/studio && npm ci && npm test` · App: `cd apps/desktop && npm ci && npm test`. Cloud container missing cmake/ninja: run `tools/ci/codex_cloud_setup.sh` and use the PATH it prints (fix setup problems in that script). No local build (e.g. ChatGPT)? CI on your branch is your build.
