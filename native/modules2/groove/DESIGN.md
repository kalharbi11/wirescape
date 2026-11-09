<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove native S1 design

Original portable C++17 code; no third-party source imported. Westlicht
Performer is a behavioral reference only, not an imported dependency.

Keel's reviewed contract is in #432 comment 6731562789. Persistent
`module.config.version=1` gains only additive fields, and old S0
patch JSON survives. The control-side adapter validates types/ranges,
pads short tracks with rests and transfers into a 4×64 transient POD.
The POD's `sizeof` is asserted, never serialized. Unknown JSON
must be retained by the Studio authoring layer on refusal.

Two players share one double-precision frame phase and a 32-step
sixteenth-note boundary. Per-player sounding/pending notes are
fixed-capacity. A pending NOTE_OFF blocks new NOTE_ON until a bounded
retry succeeds. Events are emitted in release-before-note order and
keyed by stable event-port identities. CV gate tracks admitted notes,
not merely intended steps. Mods update only on admitted NOTE_ON.
No allocations, logging, JSON, locks or I/O occur in render/reset.

The initial high value of any command parameter is ignored. Subsequent
low→high transitions apply at their observed sample. Reset suppresses
same-sample starts and advances; queued stop is processed before a
boundary retrigger. STOP and mute release gates even if an event queue
temporarily refuses NOTE_OFF; retries continue.

Native telemetry must be verified as an authoritative observed STOP
and playhead source before S1(b) UI integration. Do not invent wire
fields or claim release-ready until Keel decides any missing seam.
The live step-edit protocol and uninterrupted Apply remain S2.
