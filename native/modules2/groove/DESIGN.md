<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 native design

Original C++17, no imported third-party code; Westlicht Performer is only
a behavioral reference. Keel contract: #432 comment 6731562789.
Persist additive version-1 JSON, not validated 4×64 POD; refuse unknowns.
Two tracks share audio-frame 32-step clock with per-track sounding/pending
notes. Failed NOTE_OFF retries before new ON; gates and mods follow admitted
notes. Initial high is baseline. RESET > STOP > MUTE > START > tick.
Render/reset avoid allocation, locks, JSON, logging and I/O.
No authoritative completed-STOP/playhead in the existing native telemetry
seam; Keel ruling requested before S1(b) UI.
