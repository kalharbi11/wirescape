# AGENTS.md: rules for every agent (workflow v3, 2026-11-06; tiers 2026-11-10)

Any agent (Claude, ChatGPT, Codex or another) can pick up any open item. Nothing is reserved for a particular agent. You **claim** an item, you **sign** what you did, and you leave a **handoff** so the next agent, whoever it is, can continue. Detail: `docs/WORKFLOW.md`. Plan: `docs/PLAN.md` ("Now" at the top).

## Read map: only this, in this order
1. This file.
2. Your issue and its latest handoff comment, then the `docs/tasks/` file it names (in full; it ends with an `acceptance` block).
3. `docs/PLAN.md`: "Now", "Decisions", and your phase only.
4. The files the task lists under "Read first". Building a module: `docs/SDK_AUTHOR_GUIDE.md` and `native/sdk/examples/` (Claude Code: the `new-module` skill). UI work: `apps/studio/AGENTS.md` (after Phase 1A; before that, `kalharbi11/wirescape-ui`).

Never read `legacy/`, `docs/history/`, `build/`, `build_reports/`, third-party sources or generated files unless the task asks. Search narrowly.

## Project in brief
Wirescape is an open-source modular patching environment for music. It has a native C++17 realtime engine (run as a separate process, `wirescape-engine`) and a standalone desktop app: an Electron window around the web UI (TypeScript). The Raspberry Pi 5 (Linux ARM64) is the first platform and the performance target. Windows follows. Module DSP stays portable to plugins and Daisy-class hardware. Contract: `docs/specs/ABI2_PLAN.md`. Built today: `docs/ARCHITECTURE.md`. Evidence: `docs/STATUS.md`. The Qt Quick UI under `native/ui/` is frozen and is removed in Phase 4.

## Who's who (names agents use to identify themselves)
Four tiers, highest first: **leads** (Compass, Keel) → **seniors** (Codex Lead, Coda) → **builders** → **helpers**. Ask one tier up; skip a tier only for a stop that tier can't rule on.

| Name | Who | Role |
|---|---|---|
| **Compass** | Claude | Project lead: vision, product, UX and visual direction, roadmap and phases, checkpoints, final say on plan decisions. Owns `docs/PLAN.md`. |
| **Keel** | Claude | Technical lead: architecture, engine/realtime/protocol/format decisions, code-quality bar, CI health. Rules on technical stops, does Keel-only T2 reviews (below), checks the seniors' work at its sessions. |
| **Codex Lead** | Codex | Build lead: coordinates the Codex/ChatGPT builders, keeps the queue full, takes over stale work, approves T0/T1, reviews Claude-authored T2. First stop for builder questions; escalates technical stops to Keel, scope to Compass. |
| **Coda** | Claude | Senior reviewer and fixer, the Claude-side peer of Codex Lead: does the cross-family T2 reviews of Codex/GPT work, second reviews, approves T0/T1, takes over hard or twice-failed work. Escalates Keel-only T2 (below) and technical stops to Keel, scope to Compass. |
| **Builders** | Codex, ChatGPT or any agent | Build issues end to end. Sign `<agent> (<role in own words>)`, e.g. `Signed: Codex (UI builder), 2026-11-08`. |
| **Helpers** | Claude or Codex (mid-tier model, scheduled) | Constrained lanes: board hygiene, CI triage, main-sync, pre-review, and `lane/helper` issues (size/s, T0/T1). Sign `Helper (<Claude or Codex>, <lane>)`. No T1 approval, no T2, no protected paths, no takeovers. |

Builder roles aren't assigned or fixed. Address a lead or senior by name in an issue or PR comment (`Keel: …`, `Coda: …`); they read their names. Briefs for every tier: `docs/planner/LEADS.md`.

**Lead authority:** Compass and Keel act with the owner's authority. They may approve, label, merge directly (including merge commits the gate can't do), retarget, close or reopen PRs and issues, change protected planner/spec docs, and decide which owner-only runs (real audio, MIDI, Pi hardware) are actually worth the owner's time. They report what they did. Keel and Compass don't need each other's sign-off; for a T2 change one of them wrote, the other model family still reviews it. Codex Lead and Coda may approve T0/T1, route and delegate work (including to helpers via `lane/helper`), re-plan within a phase when the goals stay the same, and start non-dependent later-phase work. They tell Compass in the issue and escalate technical stops to Keel. **Senior delegation (Codex Lead 2026-11-08; Coda 2026-11-10)**, so the leads are needed less often: Codex Lead and Coda may each (a) write `Protected path named:` lines for `docs/tasks/**` and `docs/planner/STATE.md` only; (b) merge directly (squash) a reviewed PR whose required checks (`guard`, `linux-arm64`, `windows-x64`, `sanitizers`) are green when the gate is stuck; (c) classify inherited `static`/`cortex-m7` reds (not required) and merge past them; (d) apply a Claude conditional approval ("approve once X passes") once X is posted on the exact head; (e) decide whether a module/UI PR needs Pi evidence under the Pi rules below, and move a non-correctness measurement to a follow-up issue (never loosening a threshold). New modules and DSP in `native/modules2/` and `native/sdk/examples/` are **T1** unless they change SDK/ABI headers or engine2. T2 needs a cross-family review: Coda reviews Codex/GPT-authored T2, Codex Lead reviews Claude-authored T2; batch requests as one `Coda: T2 batch` comment listing PR and head. **Keel-only T2:** ABI/SDK header changes, edits to `docs/specs/**` (protocol, patch format), and merge-control/guard/CI-policy changes; Coda sends those, and any ruling it can't make from the spec, to Keel. Full prompts for every tier: `docs/planner/LEADS.md`.

## The loop: claim → work → sign → hand off
1. **Claim:** comment `Claimed: <agent + model>, <date>` on the issue and set `status/in-progress`. If an item has had a claim for 24 h with no push or handoff, anyone may take it over by saying so in a comment. Codex Lead or Coda may do so after 6 h for critical-path work named in `docs/planner/STATE.md`.
2. **Work:** use one branch `task/<issue#>-<slug>` and one PR with `Closes #<issue#>` (draft until green). Push small commits; WIP commits may use `[skip ci]`, the final push never does. Work only on your issue; ideas go under "Follow-ups".
3. **Sign:** every handoff or review comment ends with `Signed: <agent + model>, <date>`. This is how anyone can tell who did what.
4. **Hand off:** before you stop (done, near a limit, blocked), post the handoff below and set the status label. The next agent starts from your handoff, not from scratch.

## Done, review and merge
- **Done** is decided by the acceptance block: run its `must_pass` lines (`python tools/dev.py accept <issue#>`) and post the report. `pi:` lines run on the Pi runner.
- **Review:** every PR except T0 gets one review comment from a **different session than the author**. For T2 (engine threading/realtime, patch format, ABI, SDK, merge control/CI), the reviewer is also a **different model family** than the author (for example, Claude reviews GPT/Codex work and the other way round). Reviews are `COMMENT` only, with severity and file:line. A High plus `status/blocked` stops the PR.
- **Merge:** set `status/ready-to-merge` on a green head, then post the approval line on its own line: `Approved for merge <sha7>` followed by your `Signed:` line. The merge gate enables auto-merge. Compass and Keel merge directly when the gate can't (e.g. a history-preserving merge commit) or is stuck; Codex Lead and Coda may too, under delegation (b). Nobody formally approves, dismisses reviews or changes branch protection. Gate v4 is live: don't post the old `<Name>: approved for auto-merge` lines. Approvals on protected-path PRs are signed by Compass or Keel (Codex Lead or Coda for `docs/tasks/**` and `docs/planner/STATE.md` only). Helpers approve T0 only.
- **Tiers** (in each acceptance block): **T0** docs, tests, formatting and CI fixes (merge on green). **T1** features, modules (including their DSP), UI, tools (one review by a different session). **T2** as above (one cross-family review).
- **Checkpoints** (A, B, C in the plan) are reviews of a whole phase. They don't pause other work.

## Stops: post a handoff, label `status/blocked`, take the next item
ABI header edit · design choice not in the spec or plan decisions that changes IDs, formats, ABI or realtime behaviour · test removal or loosening a tolerance or threshold · new dependency not named in the task or plan D12 · diff more than 50% past the task's size budget · two failed attempts at the same failure. When unsure, ask in the issue. Keel rules on technical stops, Compass on plan/product stops (or the owner).

## Safety rails
- **Protected paths** need either a linked issue (`Closes #N`) with a line starting `Protected path named:` followed by the path or glob in backticks (only Compass, Keel or the owner writes those lines; Codex Lead and Coda too for `docs/tasks/**` and `docs/planner/STATE.md`), or the `allow-protected` label (owner, Compass or Keel). Always label-only: `AGENTS.md`, `CLAUDE.md`, `tools/ci/**`, `.github/workflows/guard.yml`, `.github/CODEOWNERS`, ABI headers, licences, `docs/history/**`, `legacy/**`. Protected: `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/**`, `LICENSE*`, `LICENSES/**`, `.github/**`, `native/include/wirescape/*.h`.
- Never make CI pass by weakening it. Never force-push, rewrite published history or commit directly to `main`. Direct merges: Compass, Keel, and Codex Lead or Coda under delegation (b) only.
- Never commit generated or binary files, fonts, `user_data/`, `local_backups/`, `.tools/`, `node_modules/`, `dist/`, credentials or machine-specific absolute paths.

## Engineering rules (non-negotiable)
- **Realtime:** no allocation, locks, I/O, exceptions, logging or JSON in process, event or reset. Bounded work. Create/prepare/destroy run off the audio thread (quiescent or via the live-replan path).
- **Devices off:** tests, CI, agents and the Pi runner never open audio/MIDI devices or LAN sockets (localhost is fine). Real-device paths only behind `--confirm-*`, run by the owner (their launcher/autostart counts).
- **App ↔ engine:** the app talks to `wirescape-engine` only over its stdin/stdout (no network sockets). The renderer reaches it only through the preload bridge (`contextIsolation` on, `nodeIntegration` off). Message sizes are bounded. LAN features (OSC) stay behind an explicit, off-by-default toggle.
- **Contract:** follow `docs/specs/ABI2_PLAN.md` and, from Phase 1, the engine protocol in `docs/specs/` (written in P1.0). ABI soft freeze: additive SDK changes only, otherwise a migration note.
- **Compatibility:** module, port and parameter IDs never change; patch migration is lossless; unknown data is preserved by both the UI and the engine.
- **Tests:** every change ships tests. UI changes keep Playwright green with `FakeEngine`; engine-sidecar changes add protocol tests with a null sink and fake MIDI.
- **Licences:** SPDX header on every new file. MIT for `native/sdk/`, `native/include/wirescape/module2.h`, `native/voice/`; GPL-3.0 elsewhere (including `apps/` and `native/sidecar/`). Third-party sources, pinned revisions and licences go in that component's `DESIGN.md`.

## Labels
Status `status/todo|in-progress|review|ready-to-merge|handoff-ready|blocked` · Tier `tier/T0|T1|T2` · Size `size/s|m|l` · Phase `phase/0..5` · `checkpoint` · overrides `allow-protected`, `allow-test-removal` (owner, Compass or Keel only). Routing: `lane/helper` (set by builders and up) marks an issue a helper may build. The old `agent/*` labels are informational only; no one is required to set them.

## Handoff comment template
```
**Status:** done | handoff-ready | blocked   **Branch/PR:** …   **Last commit:** …
**Done:** bullets
**Next:** ordered bullets (written so any agent can continue)
**Tests:** commands + results (acceptance report link)
**Gotchas/decisions:** bullets
**Follow-ups (not done):** bullets
Signed: <agent + model>, <date>
```

## Commands (all keep audio off)
`python tools/dev.py doctor | build [--headless] | test | check --package | launch`. UI (after 1A): `cd apps/studio && npm ci && npm test`. App (after 1F): `cd apps/desktop && npm ci && npm test`.
**Cloud containers:** if `cmake` or `ninja` is missing, run the repo's setup script first (`tools/ci/codex_cloud_setup.sh`, or `tools/cloud/setup.sh` once it exists) and use the PATH it prints. Fix setup problems in that script, never by asking the owner to change environment settings. Agents without a build (for example ChatGPT): CI on your branch is your build; read its summary and fix.

## Pi evidence
- No agent reaches the Pi or the laptop, by SSH or any other way. Pi work goes through the `pi5` runner on the private repo `kalharbi11/wirescape-pi` (`docs/PI_RUNNER.md`); this repo's workflows never target it.
- Add `ci/pi` to a draft PR whose acceptance has `pi:` lines; use `ci/pi-soak` for soaks and `ci/pi-retry` for one rerun after an infrastructure fix. Link the private run; queued runs are not evidence. Never run fork PRs on the Pi.
- Pi evidence only for engine, DSP, sidecar, audio-path or build changes. Real sound, MIDI devices and the display are checked by the owner.

## Docs upkeep
`docs/PLAN.md` belongs to Compass (others may tick checkboxes). Update `docs/ARCHITECTURE.md` in the PR that changes the architecture, and `docs/STATUS.md` when a phase completes. Tasks go in `docs/tasks/`, reviews in `docs/reviews/`.

### Patch design and capability-guide upkeep (owner mandate, 2026-11-08)
- Patch-design agents MUST read `docs/PATCH_DESIGNER_GUIDE.md`, `apps/studio/patches/README.md` and the current generated module catalogue before designing. They may design, validate and commit patch JSON for the owner, then self-review and merge patch-JSON/description-only changes after exact-head required checks. No extra Codex Lead or separate-session patch-design approval is required; application/DSP/CI changes retain normal review. This is the owner's scoped exception to the usual T1/lead-merge rules, not permission to bypass checks or branch protection.
- Every integration that adds, changes or removes a patch-facing capability MUST update the guide in the SAME PR: modules, ports, parameter ranges/units, voice limits, note/MIDI/CV routes, platform support, assets, save/load or publishing behavior. The integrating author owns the update; the reviewer checks it before merge. Record actual availability, limitations and a usable wiring/example where relevant; keep unmerged/proposed capabilities explicitly separate. Do not mark evidence or hardware listening passed without a result.
- If the integration has no effect on patch authoring, state `Patch guide impact: none — <reason>` in the PR self-review. STATE may link the guide, but is not its maintenance policy or capability source.

## If you are a lead or senior (Compass, Keel, Codex Lead or Coda)
Your session prompt says which one you are; full brief and the usage rules in `docs/planner/LEADS.md`. Leads and seniors delegate mechanical work down a tier (helpers via `lane/helper`, ChatGPT chats, extra Codex chats, subagents) and never poll CI. Start from `docs/planner/STATE.md`, then "Now" in `docs/PLAN.md`. Don't assign agents; the owner and their managers do that. End each session with "Next steps for you" (0–3 owner steps, or "Nothing needed from you") and update `docs/planner/STATE.md`.
