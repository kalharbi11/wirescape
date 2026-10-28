# Wirescape plan

Owner: the planner (Claude). Other agents may only tick checkboxes. Last updated: 2026-10-28 (Jam checkpoint adopted from the fresh-eyes review).

## Now (≤30 lines; single source of truth; checked against the GitHub API 2026-10-28 ~17:00 UTC)
- **Focus:** Jam checkpoint (J) + workflow v2 (`docs/WORKFLOW.md`). Agents run in the cloud; the Pi is a runner only.
- **Merged today:** #75 J plan/guide, #40 M2-3, #41 M2-4, #95 workflow v2, #96 CodeQL, #102 guard reads issue-named paths, #97 cloud setup, #103 review packs, **#76 J7 CHAOS-lite**.
- **Open PRs:**
  - #99 accept tool (T0): green, ready → merge.
  - #104 static analysis, #106 PR template (T0): Ruff must become diff-only (61 old findings), then merge. #105 pre-commit is stacked on #104.
  - #98 Pi bridge docs: refresh after wirescape-pi#1, then merge.
  - #78 J1 (T2): code done; needs Windows CI + `pi5/soak` (label `ci/pi-soak`), then Claude review.
  - #79 J2 → #80 J3 → #81 J4 → #82 J5: stacked on #78; never merge before #78.
  - #77 J6 (T1): upstream patch + Windows fix pending.
- **Pi bridge:** runner live; blocked on token permissions (HTTP 403) and wirescape-pi#1 (PATH, soak label). Summary issue #101.
- **Next 3:** (1) user: token + Codex network setting + merge #99 and wirescape-pi#1; (2) Codex: J1 to green, then J6, J2, J3; (3) ChatGPT: #88, #93, #56, #49, #66 and T1 pre-reviews.
- **Routing:** Copilot gets nothing. Deferred W items: #91, #92 until J1 merges; #94 after J5.
- **Deferred until after J9:** #64 and the Windows 1.05× gate, M3 full UI, Learn/Activity/Mappings, MIDI Out/Clock, UDP Send, OSC Out, MPE, Quantizer + Basic Sequencer, X-4, long soaks, the formal ABI freeze.

## Vision
Wirescape is a raw, expandable modular environment for music. Its aims:
- Modules are easy for people and AI agents to build, from one clear contract and SDK.
- Complex instruments (reverbs, synth voices, a rebuilt CHAOS) work as well as simple utilities.
- Module DSP moves unchanged into CLAP/VST3 plugins and Daisy-class hardware through thin adapters.
- MIDI, OSC, UDP and later DMX, serial/Arduino and scripting plug in as services without rebuilding the core.

It's fully open source. The app and Essentials are GPL-3.0. The module SDK is MIT, so module authors can choose their own licence. The name is in `TRADEMARKS.md`.

## Principles
1. Contract first: decisions that touch every module are settled before many modules exist.
2. Realtime safety: bounded, allocation-free processing, explicit lifecycle and deterministic offline renders.
3. Creative freedom: no type-gated cables, audio-rate modulation, feedback loops and polyphony everywhere.
4. Efficiency through architecture: constant-signal fast paths, SIMD across voices, float32 and measured optimizations. The Pi 5 is the CPU target.
5. Evidence over claims: CI on Windows x64 and Linux ARM64, with honest docs (`docs/STATUS.md`).
6. Usage-conscious agents: narrow reads, bounded issues, handoff comments and review at milestones.

## Settled decisions (details: `docs/specs/ABI2_PLAN.md`, sections 2 and 9)
| Area | Decision |
|---|---|
| Cables | float32, audio rate, 1–16 channels, CONSTANT flag |
| Polyphony | Channels on the cable (VCV model). The source sets the count. Broadcast / SUM / FIRST policies. One shared voice allocator |
| Stereo | Two ports with a `stereo_pair` hint. Channels always mean voices |
| Units | Audio ±1. Pitch 1.0/oct with 0 = C4. CV ±1 or 0..1. Gates 0/1 through a Schmitt trigger. Hints never block a connection |
| CV → parameters | Host-side and non-destructive, with an amount on the cable, in normalized space, per voice when poly |
| Events | Per-block sorted lists with sample offsets. Event outputs. UMP-style MIDI. Bounded messages. Overflow degrades and never drops note-offs |
| Feedback | Loops allowed. Loop parts run in sub-blocks (default 32, down to 1) |
| Memory | Host allocator with FAST/LARGE hints, and declared requirements |
| Also | State blobs, latency, config-dependent ports, transport, ingress/egress rings, applied-generation ack |
| SDK | Plain C++ class + header-only wrapper over a C ABI. C++17 for DSP. Own SIMD wrapper with a scalar fallback |
| Licence | SDK MIT; app and Essentials GPL-3.0; Surge-derived modules GPL |
| Network | Localhost by default; explicit LAN toggle per module |
| CI | Linux ARM64 on every PR update and every push to `main`. Windows on PRs that are ready for review, or with the `ci/windows` label. The `guard` judges PRs with the base branch's rules. Sanitizers (ASan+UBSan, TSan) on ABI 2 suites. The Pi 5 is a self-hosted runner on the private repo `kalharbi11/wirescape-pi` only (PR checks on own branches, nightly tests/soaks/benchmarks/fuzzing/static analysis); this repo's workflows never target it |

## Milestones
- **M0 Migration** to `github.com/kalharbi11/wirescape`
  - [x] M0-1 Local cleanup and clean tree (`docs/tasks/M0-1_LOCAL_CLEANUP.md`, Codex, local)
  - [x] M0-2 First push, labels, milestones and issues (`docs/tasks/M0-2_GITHUB_PUSH.md`, Copilot, local)
  - [x] M0-3 CI and repo hygiene (`docs/tasks/M0-3_CI_SETUP.md`, issue #1, PR #4). Reviewed 2026-10-27: safe to merge; then set branch protection (checks `linux-arm64`, `guard`, `windows-x64`; no approvals)
  - [x] M0-4 Guard hardening and CI trigger cleanup (`docs/tasks/M0-4_GUARD_HARDENING.md`, issue #5, size/s). Findings from the PR #4 review: the guard must run the base branch's rules, use merge-base diffs and protect `tools/ci/`. **Merge before M1-3 touches `ci.yml`**
- **M1 ABI 2 foundation**: contract, SDK, engine2, voice allocator, 5 modules ported. **Stops for review.**
  - [x] M1-1 Foundation slice: built before the migration. Review package: `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`. Functional tests A–M pass offline. **Known gate:** mono throughput is about 1.89× ABI 1 (target ≤ 1.05×). ARM64 is unverified until CI.
  - [x] M1-R Claude review of the slice (issue #2; review: `docs/reviews/M1-R_PLANNER_REVIEW.md`). **No-go for M2 until M1-3.** The design stays. The gap is framework overhead (per-sample SDK accessors, two copies per cable, redundant finite scans), measured at 1.57× by instruction count. ARM64: all 24 suites pass in CI; ASan+UBSan and TSan clean locally.
  - [x] M1-2 Audio capture and analysis harness (`docs/tasks/M1-2_AUDIO_CAPTURE_HARNESS.md`, PR #13 merged)
  - [ ] M1-2b The user's one real-device capture run on Windows (issue #43). Not a gate; do it early
  - [x] M1-3 Slice fixes and the performance gate (`docs/tasks/M1-3_SLICE_FIXES.md`, issue #6, PR #22, Codex, size/l). Gate for M2: mono ≤ 1.05× on both CI runners and by instruction count, poly ≤ 1.00×, sanitizer job green, adversarial concurrency and feedback tests. **Reviewed 2026-10-27 (`docs/reviews/M1-3_REVIEW.md`): conditional go.** Everything passes except Windows mono (1.097×), which moves to M1-3b and gates M2-6
  - [x] M1-3b Windows (MSVC) mono gap and an Ir gate in CI (issue #32, PR #37). Accepted with the remaining gap moved to #64, which is now tracked, not a gate (2026-10-28)
- **M2 Port and patch v2** (ABI2 plan B2 to B4): engine2 becomes the app engine; all Essentials on ABI 2; per-module CV; patch v2 with lossless v1 migration. Optional: self-hosted Pi 5 runner for benchmarks.
  Task file: `docs/tasks/M2_PORT_AND_APP_ENGINE.md`. All Codex; M2-0 may start now, M2-1 to M2-5 after PR #22 merges, M2-6 after M1-3b and M2-1/2/3/5 merge. The M2 queue is superseded by the J queue; Pi work goes through the Pi runner (workflow v2).
  - [x] M2-0 Benchmark tooling and Pi 5 realtime evidence (issue #23, PR #31)
  - [ ] M2-0b Pi soak re-run on post-M1-3 `main` (issue #42)
  - [x] M2-1 Per-bus latency and staged reconfigure (issue #24, PR #38)
  - [x] M2-2 Port mixer, noise, output, scale_offset, slew (issue #25, PR #39)
  - [x] M2-3 Port clock, function_envelope, ladder (issue #26, PR #40)
  - [ ] M2-4 Behaviour regressions for the five ports, per-module CV (issue #27, PR #41: green, waiting for merge)
  - [x] M2-5 Patch v2 and lossless v1 migration (issue #28, PR #48)
  - [ ] M2-6 engine2 becomes the app engine (issue #29, size/l). Now **J1**
  - [ ] M2-P Portability compile probe for a Daisy-class target (issue #44, PR #55, report only), then the fix (#56)
  - [ ] M2-L The user's live session (issue #45). Replaced for now by **J9**, which covers it
  - [ ] M2-R Claude milestone review. Folded into J-R1/J-R2
- **J Jam checkpoint** (adopted 2026-10-28; `docs/tasks/J_JAM_CHECKPOINT.md`). A pause point where everything that exists works, on the laptop and the Pi, so the user can play and write their own modules. Decisions: CHAOS-lite as one native module now (decomposition stays M7); ABI soft freeze from J-R2 (additive changes only, migration note otherwise); headless Pi runner required, Pi UI a stretch; Windows 1.05× mono is tracked, not a gate.
  Per-item task files with acceptance blocks: `docs/tasks/J2_POLY_APP.md` … `J8_AUTHOR_GUIDE.md`. Deferred until after J9: #64 / Windows 1.05× (tracked only), M3 full UI, Learn/Activity/Mappings, MIDI Out/Clock, UDP Send, OSC Out, MPE, Quantizer + Basic Sequencer, X-4, long soaks, formal ABI freeze.
  - [ ] J0 Merge #40 (done), then #41 (user)
  - [ ] J1 = M2-6 engine2 is the app engine (#29, PR #78, Codex, T2)
  - [ ] J2 Polyphony in the app: `host.keyboard`, voice count, poly badge (#69, PR #79, Codex)
  - [ ] J3 Pi audio: headless `wirescape play`, Linux audio, a Pi setup guide; Pi UI stretch as a Pi-runner job (#70, PR #80, Codex; first real audio run by the user)
  - [ ] J4 MIDI In, MIDI→CV, CC→CV, one backend, hot-plug safe (#71, PR #81, Codex, T2)
  - [ ] J5 OSC In over UDP (localhost default, LAN toggle) and Monitor log (#72, PR #82, Codex, T2)
  - [ ] J6 Macro Oscillator spike = X-3, poly 8, 48 kHz (#73, PR #77, Codex)
  - [ ] J7 CHAOS-lite, two instances on the Pi (#74, PR #76, Codex)
  - [x] J8 Module author guide v2, two templates, `new-module` skill (Claude)
  - [ ] J-R Claude gate review (T2 PRs + the milestone)
  - [ ] J9 The user's checkpoint session, laptop + Pi (#45)
- **W Workflow v2 automation** (adopted 2026-10-28; `docs/WORKFLOW.md`, task files `docs/tasks/W*.md`). Runs alongside J; order: W0 Codex cloud setup → W1 Pi runner → W-a accept → W-b review pack → W-c static analysis, W-d CodeQL, W-i pre-commit, W-j PR template → W-e benchmark gate, W-f golden hashes → W-k docs freshness. W-h fuzzing after J5.
- **M3 UI for the contract** (B5): poly cables, modulation drag-to-knob with an amount on the cable, base and modulation rings, back-edge markers, stereo pairs. → **Preview 0.7** (a patching preview, not yet an instrument) when M3 is green and an M2-L-style live session passes on it.
- **Beta 0.8: first playable instrument** (proposed 2026-10-27; the user may redefine it). Preview 0.7 plus **MIDI In and MIDI→CV** (pulled forward from M4) and **Quantizer and Basic Sequencer** (pulled forward from M6). It ships when a 60-minute live session with an external MIDI keyboard passes with no stuck notes, dropouts or crashes. Then file the US trademark, Class 9 (see `TRADEMARKS.md`). The rest of M4 and M6 follows the beta.
- **M4 Communication I** (`docs/specs/COMMUNICATION_MODULES_PLAN.md`, C1 to C3): endpoint registry, Activity and Learn, Mappings, Monitor, MIDI In / MIDI→CV / CC Map / MIDI Out / MIDI Clock.
- **M5 Communication II** (C4 to C6): UDP Receive/Send, OSC In/Out, hardening.
- **M6 Musical core**: transport, Quantizer, Basic Sequencer, Chord Sequencer + Strum, VCA LPG mode, exponential Lag, Scope and Spectrum through bounded analysis taps. Module list: `docs/ROADMAP.md`.
- **M7 Instruments**:
  - subpatches and macros
  - Channel Expander (unison/cluster)
  - a CHAOS rebuild from modules, using sst-* library blocks (GPL) as a subpatch with a panel; the study-case table is in `ABI2_PLAN.md` section 3
  - one Plaits or Braids port from pinned sources
- **M8 Portability proofs** (B6): CLAP adapter (VST3 via clap-wrapper), Daisy static build, export bundle spec. Future: Python and Faust script modules, Serial, DMX (Art-Net/sACN).
- **Track X: porting existing open-source instruments** (exploratory, never blocks M2; `docs/tasks/X_THIRD_PARTY_PORTS.md`). Studies now, by ChatGPT: X-1 Mutable Instruments Plaits (#51), X-2 CHAOS decomposed into Surge/sst-based modules (#52). Spikes (X-3 Plaits, X-4 the first Surge/sst primitive) come after the studies are reviewed and only when they don't compete with M2 for Codex time. Nothing merges before M2-R. **The ABI 2 header and SDK are not frozen until X-3 and X-4 (one Mutable instrument, one Surge/sst component) have shown that outside DSP fits the contract without engine changes, on desktop and the Pi.** The M7 instrument items build on what Track X learns. A whole-Surge-engine module (X-6) is an optional desktop feature for after Beta 0.8.

## Current milestone: J (Jam checkpoint)
See "Now" at the top and `docs/tasks/J_JAM_CHECKPOINT.md`. After J9, the plan resumes at M2-R leftovers and M3; Beta 0.8 then needs only the Quantizer and Basic Sequencer on top of J (MIDI In/MIDI→CV ship in J4), and M4/M5 shrink to what J left out.

## How work flows
- **The board is GitHub Issues** (labels and milestones in `AGENTS.md`). Each issue points to a `docs/tasks/` file.
- **Roles, tiers, stops and usage rules:** `AGENTS.md` (short) and `docs/WORKFLOW.md` (detail). Codex cloud builds; ChatGPT does docs, self-contained modules and first-round reviews; Claude plans and does T2/gate reviews; the Pi runner produces all Pi numbers; Copilot gets nothing for now.
- **Switching agents** at usage limits: the next agent reads the issue's latest handoff and continues on the same branch.
- **Approvals:** branch protection requires no approvals; no agent approves, merges or enables auto-merge; the user's merge click is the approval. Every merge is reversible with "Revert".
- **Override labels** (`allow-protected`, `allow-test-removal`): only the user, or the planner when the issue names the protected path.
- Planner queue changes go in issue comments, not PRs.
- **Planner check-in prompt** (a fresh session each time):
  > Read AGENTS.md and the "Now" section of docs/PLAN.md, then the open issues and PRs, latest handoffs and CI summaries (REST API). Update "Now", the issues and task files (each with an acceptance block). Review only T2 PRs, from their review packs. Don't write code.

## Document map
- **Planner handoff** (the reasoning behind the decisions, and the M1 review checklist): `docs/PLANNER_HANDOFF.md`.
- **Specs:** `docs/specs/` (ABI2_PLAN, ABI2_SPEC with the patch v2 drafts, COMMUNICATION_MODULES_PLAN, AUDIO_CAPTURE_HARNESS, EMBEDDED_PORTABILITY, PROVIDER_BRIDGE_CONTRACT).
- **Module author guide:** `docs/SDK_AUTHOR_GUIDE.md`, with the minimal template in `native/sdk/template/` and worked examples in `native/sdk/examples/`. Claude Code skill: `.claude/skills/new-module/`.
- **Tasks:** `docs/tasks/`.
- **Reviews:** `docs/reviews/`.
- **What exists:** `docs/ARCHITECTURE.md`. **Evidence:** `docs/STATUS.md`. **Modules:** `docs/ROADMAP.md`.
- **UI reference:** `docs/design/`.
- **Frozen history**, never read unless asked: `docs/history/`.
- **The old Python/browser app**, reference only: `legacy/`.
