<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Sequencer development
Shared home for Groove and future conventional/generative sequencers. This is a development lane alongside the application plan, not a replacement for its phases. Pi failures and core critical-path work retain priority.

- [Groove roadmap and compatibility gates](GROOVE.md)
- [Build-agent handoff](AGENT_PROMPT.md)
- [Groove S1 UI designer brief](UI_DESIGN_BRIEF.md): design-only study alongside native S1 work, with HTML/CSS deliverables and editing-state examples.

## Shared expectations
Studio owns editable musical data; native DSP owns playback. Keep authored patterns separate from transient playhead, held notes and queued performance actions. Persist musical settings; load stopped by default, with no accidental note-on. Preserve unknown patch data and validate executable configuration.

Use fixed-capacity, bounded realtime state. No callback allocation, locks, JSON, I/O or logging. UI animation never supplies timing. Stopped editing may use the existing graph reload/apply path in the first Groove release; that can interrupt other modules. When live content editing is introduced, it must not rebuild the graph. Accepted, pending, refused and actually applied edits must be distinguishable; generation/revision guards, undo/redo and restart recovery belong to the shared contract.

Stable IDs, pitch units (1.0/octave, 0=C4), gate behavior and NOTE_ON/OFF identity must agree with the SDK. Quantized pitch CV and unquantized note events must not compete on the same destination. Test usable downstream routes, not just output generation.

Future sequencers share documented edit, persistence, telemetry and validation conventions where useful. They need not share Groove's transport, pattern schema or scheduler. Extract common code only after a second concrete user demonstrates the need. For generative modules, explicitly document seed/reproducibility, bounded work, regeneration boundaries, and whether saving stores rules, realized output, or both. Do not add a speculative universal sequencer framework now.

Keep designs/phase records here; executable acceptance task files remain in docs/tasks/ under repository workflow. Each future sequencer gets its own document and issues. Component DESIGN.md records third-party provenance: Westlicht Performer is a behavioral reference, not an approved code/dependency import. Audit license compatibility before reuse.


## 16-step regression gate before the complex release

The existing401/405native and408/410editor agents retain ownership. GrooveS0
stopped-config proof may proceed independently; do not call GrooveS1release-ready
while the16step baseline or combined app regressions are failing/unverified.
Require default8/legacy trace; lengths1/8/16; steps8→9/16→1; rest/note/tie;
reset+clock compatibility; step16stop/restart/reset balancedoffs; actualslot49
velocity; statev1load/v2save; high-step pitch/gate/velocity Undo/Redo and reload;
mouse/touch layout; real native/WASM routes and zero callback allocations/faults.
Keep example.step_sequencer and sequencing.groove separate. No speculative shared
scheduler or protocol expansion; quantized pitchCV and raw note events must not
compete at a destination. Existing tolerances/owner hardware gate remain intact.

Signed: Codex Lead (Codex), 2026-11-08.
