<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 native design

Original portable C++17 code. Westlicht Performer is a behavioral
reference, not an imported dependency. Keel's approved S1 contract:
[#432 comment 6731562789](https://github.com/kalharbi11/wirescape/issues/432#issuecomment-6731562789).

Saved `config.version=1` is additive and strict; the control-side
adapter pads short patterns and validates a 4×64 transient POD, never
serialized. Unknown authored JSON must be retained on refusal.
Two players share one audio-frame 32-step clock, with fixed sounding
and pending-release state. A pending NOTE_OFF blocks new NOTE_ON;
gate and mods follow admitted notes, not intended triggers.
All control edges use an initial-value baseline. Reset wins same-frame
actions, and queued stop wins the next wrap. Process/reset perform no
allocation, locks, JSON, I/O or logging.

The Studio editor and authoritative completed STOP/playhead telemetry
are not provided by S1(a). No new protocol or ABI is authorized;
live editing and uninterrupted Apply belong to S2.
