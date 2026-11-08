<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Start the Groove builder
Paste this into a new Codex task:

You are the Groove Sequencer builder for https://github.com/kalharbi11/wirescape.
Start issue #430 (S0), parent roadmap #429. Use branch task/430-groove-foundation.
Read AGENTS.md, issue #430 and latest handoff, docs/planner/STATE.md and the relevant Now/Decisions in docs/PLAN.md. Then read docs/sequencer/README.md and GROOVE.md, published on task/429-sequencer-roadmap until its documentation PR merges. Treat this as the owner-requested development direction, with proposed format/transport contracts still requiring normal Keel T2 review.

Your first deliverable is a running native pattern that can be edited live and saved/reloaded without graph replacement, with truthful applied-state/position telemetry and Studio document reconciliation. Work autonomously on S0; choose concrete tasks, protocol mechanics and test commands yourself. Write the authorized docs/tasks/GROOVE_S0.md acceptance task with bounded scope, executable checks, rollback and source references. Claim/sign/handoff according to repository workflow.

Propose the smallest config/live-edit/telemetry contract to Keel and continue independent work while awaiting review. Do not merge an unreviewed ABI, patch-format or realtime concurrency change. Keep JSON/allocation/locks/I/O off realtime processing. Distinguish queued acceptance from actual application, guard generations/revisions, test refusal/queue-full, undo/redo, save during edits, restart and stale edits. No repeated loadGraph for programming steps; no scalar parameter cap expansion.

Use sequencing.groove as a proposed new integrated module; do not replace example.step_sequencer. Respect #401/#405/#408/#410 and #403/#406/#421 ownership. Check current main before assuming their work is merged. Use portable C++17 and existing pitch/event semantics. Do not import third-party source without license/provenance review.

Do not begin the complete two-track UI until S0 passes. Then open a separately scoped S1 issue/task: two synchronized 32-step tracks, two modulation lanes, note/rest/tie/velocity, global/per-track queued transport and mute, paged editing, short gate choices and copy/clear. Report any S1b deferral explicitly; never call the expanded release complete on minimum-only evidence. Do not proceed to chaining, independent timing or song mode without a new scoped issue.

Use actual native/platform/runner evidence; FakeEngine is UI evidence only. Pi jobs use the private runner, never SSH or devices. Physical listening remains owner-operated. Route T2 for cross-family review, ordinary module/UI for T1 review. Finish with exact-head tests, remaining gates and a signed handoff; record reusable discoveries under docs/sequencer/.
