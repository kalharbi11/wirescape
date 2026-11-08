<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Sequencer development
Shared home for Groove and future conventional/generative sequencers. This is a development lane alongside the application plan, not a replacement for its phases. Pi failures and core critical-path work retain priority.

- [Groove roadmap and compatibility gates](GROOVE.md)
- [Build-agent handoff](AGENT_PROMPT.md)

## Shared expectations
Studio owns editable musical data; native DSP owns playback. Keep authored patterns separate from transient playhead, held notes and queued performance actions. Persist musical settings; load stopped by default, with no accidental note-on. Preserve unknown patch data and validate executable configuration.

Use fixed-capacity, bounded realtime state. No callback allocation, locks, JSON, I/O or logging. UI animation never supplies timing. Live content edits must not rebuild the graph. Accepted, pending, refused and actually applied edits must be distinguishable; generation/revision guards, undo/redo and restart recovery belong to the shared contract.

Stable IDs, pitch units (1.0/octave, 0=C4), gate behavior and NOTE_ON/OFF identity must agree with the SDK. Quantized pitch CV and unquantized note events must not compete on the same destination. Test usable downstream routes, not just output generation.

Future sequencers share documented edit, persistence, telemetry and validation conventions where useful. They need not share Groove's transport, pattern schema or scheduler. Extract common code only after a second concrete user demonstrates the need. For generative modules, explicitly document seed/reproducibility, bounded work, regeneration boundaries, and whether saving stores rules, realized output, or both. Do not add a speculative universal sequencer framework now.

Keep designs/phase records here; executable acceptance task files remain in docs/tasks/ under repository workflow. Each future sequencer gets its own document and issues. Component DESIGN.md records third-party provenance: Westlicht Performer is a behavioral reference, not an approved code/dependency import. Audit license compatibility before reuse.

Signed: Codex Lead (Codex), 2026-11-08.
