# AGENTS.md: rules for every agent (workflow v3.1, 2026-11-10)

Any agent may pick up any open item: **claim → work → sign → hand off.** Leads, seniors and helpers: your brief and authority are in `docs/planner/LEADS.md`. Detail behind these rules: `docs/WORKFLOW.md` (read it only when a rule here is unclear).

## Read map (only this, in this order)
1. This file (already loaded).
2. Your issue + its latest handoff, then the `docs/tasks/` file it names (it ends with an `acceptance` block).
3. `docs/PLAN.md` "Now" only (leads also read "Decisions"). Then the files the task lists under "Read first".
4. Area guides only when you work there: UI `apps/studio/AGENTS.md`; modules the `new-module` skill or `docs/SDK_AUTHOR_GUIDE.md` + `native/sdk/examples/`; patch design `docs/PATCH_DESIGNER_GUIDE.md`.

Never read `legacy/`, `docs/history/`, `docs/tasks/done/`, `build*/`, `build_reports/`, third-party sources or generated files unless the task says so. Grep; don't read whole files.

## Project in one paragraph
Open-source modular synth. A C++17 realtime engine (ABI 2 modules: `native/engine2`, `native/modules2`) runs as a separate process, `wirescape-engine` (`native/sidecar`), talking over stdin/stdout JSON lines to an Electron app (`apps/desktop`) around a TypeScript web UI (`apps/studio`). Raspberry Pi 5 (ARM64) first, Windows next; module DSP stays portable to plugins and Daisy-class hardware. Contracts: `docs/specs/ABI2_PLAN.md`, `docs/specs/ENGINE_PROTOCOL.md`. Built today: `docs/ARCHITECTURE.md`. Qt (`native/ui/`) and ABI1 (`native/src`, `native/modules`) are being removed: don't touch or extend them.

## Who's who
| Tier | Names | Ask them about |
|---|---|---|
| Leads (Claude) | **Compass** (product, UX, plan; owns `docs/PLAN.md`) · **Keel** (architecture, specs, ABI, CI policy, technical stops) | scope → Compass; technical stop → Keel |
| Seniors | **Codex Lead** (Codex: build queue, board #473) · **Coda** (Claude: T2 reviews of Codex/GPT work, hard fixes) | anything a builder can't decide |
| Builders | any agent (Codex, ChatGPT, …) | — |
| Helpers | scheduled mid-tier runs, `lane/helper` items | — |

Ask one tier up. Address someone with `Name: …` in an issue or PR comment; they read their names.

## The loop
1. **Claim:** comment `Claimed: <agent>, <date>` and set `status/in-progress`. A claim with no push or handoff for 24 h may be taken over (say so in a comment).
2. **Work:** branch `task/<issue#>-<slug>`, one PR with `Closes #<issue#>`, draft until green. WIP commits may use `[skip ci]`; the final push never does. Ideas go under "Follow-ups".
3. **Keep going (queue mode):** when your item is green and handed off, take the next unclaimed item in your lane in the same session instead of ending it. Stop at your run's item limit or when usage is nearly spent.
4. **Hand off** before stopping (template below) and set the status label. Sign every handoff and review: `Signed: <agent> (<role>), <date>`.

## Done, review, merge
- **Done** = the acceptance block passes: `python tools/dev.py accept <issue#>`; post the report.
- **Tiers:** **T0** docs, tests, formatting, CI fixes → green + `status/ready-to-merge`, no review. **T1** features, modules + DSP, UI, tools → one review from another session. **T2** engine realtime/threading, protocol, patch format, ABI/SDK, CI/merge control → one review from the **other model family**. If the diff touches a higher tier's paths, the higher tier wins.
- **Approve:** on a green head, a standalone line `Approved for merge <sha7>` followed by a `Signed:` line in the same comment, plus `status/ready-to-merge`. The merge gate auto-merges. A new push needs a new approval. Who may approve what: `docs/planner/LEADS.md` "Authority".
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
`status/todo|in-progress|review|ready-to-merge|handoff-ready|blocked` · `tier/T0|T1|T2` · `size/s|m|l` · `phase/0..5` · `checkpoint` · `lane/helper` (an item a helper may build; set by builders and up) · overrides `allow-protected`, `allow-test-removal` (owner, Compass or Keel). `agent/*` labels are informational.

## Handoff template
```
**Status:** done | handoff-ready | blocked   **PR:** #…   **Head:** sha7
**Done:** …
**Next:** ordered steps any agent can follow
**Tests:** command → result (acceptance report link)
**Gotchas / follow-ups:** …
Signed: <agent> (<role>), <date>
```

## Commands (audio stays off)
`python tools/dev.py doctor | build [--headless] | test | accept <issue#> | check --package | launch` · UI: `cd apps/studio && npm ci && npm test` · App: `cd apps/desktop && npm ci && npm test`. Cloud container missing cmake/ninja: run `tools/ci/codex_cloud_setup.sh` and use the PATH it prints (fix setup problems in that script). No local build (e.g. ChatGPT)? CI on your branch is your build.
