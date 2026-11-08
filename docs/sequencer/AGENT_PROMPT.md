<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Start the Groove builder
Paste this into a new Codex task:

You are the Groove Sequencer builder for https://github.com/kalharbi11/wirescape.
Start issue #430, parent roadmap #429, branch task/430-groove-foundation.
Read AGENTS.md, the issue/latest handoff, relevant planner state/Now/Decisions, then docs/sequencer/{README,GROOVE}.md on task/429-sequencer-roadmap until #431 merges.

Owner revised the scope: build and verify the instrument first; live editing is deferred to S2 (or explicitly rescheduled S3). Do not build a new realtime pattern-edit protocol as a prerequisite.

First prove a small structured configuration loads/renders/saves correctly (S0), then deliver S1: two synchronized 32-step tracks, shared BPM, looping, rest/note/tie/velocity, pitch/gate/note events and two modulation lanes, global/per-track queued transport and immediate mute, paged editor, persistence and basic undo/redo. Short gate choices and copy/clear remain useful planned extras with explicit S1b deferral if needed.

Pattern edits require global sequencer STOP to have actually completed. Use a documented stopped Edit → Apply → Start workflow (or apply-on-Start). Existing loadGraph/config reload and stop–swap–start are allowed here. Explain that Apply can interrupt/reset other modules; seamless audio and phase continuity across Apply are later work. Show actual native loaded/refused state; retain draft edits on refusal and never start a rejected configuration. Restart from step 1 after successful Apply. Save/load and restart must preserve authored data, load stopped, release prior notes and avoid accidental note-ons.

Choose exact tasks/mechanics/tests yourself. Write authorized docs/tasks/GROOVE_S0.md acceptance with bounded scope, source references and rollback. Keep configuration structured/versioned, no scalar cap expansion. New schema/format/protocol/realtime changes retain normal Keel T2 review; module/UI is T1 on the approved contract. Do not mutate render-owned state just because the sequencer is stopped.

Use sequencing.groove as proposed integrated module; preserve example.step_sequencer and #401/#405/#408/#410 ownership, plus #403/#406/#421. Recheck current main. Portable C++17, bounded realtime state, no callback allocation/locks/JSON/I/O. No unreviewed third-party imports.

Work autonomously through the scoped stopped-configuration foundation; open a separately scoped S1 issue/task for the full instrument. Do not wait for live-edit design to build the stopped workflow. Record S2 live-edit discoveries without implementing that phase. Actual native/Windows/Pi evidence is required; FakeEngine only proves UI behavior. Pi tests use the private runner, never SSH/devices; physical listening is owner-operated. Claim/sign/handoff per AGENTS.md.
