<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 native design

Original portable C++17; no imported third-party code (Westlicht Performer
is only a behavioral reference). Keel contract: #432 comment 6731562789.

Save additive version-1 JSON, never the validated 4×64 control-side POD.
Unknown fields refuse; Studio retains authored draft on refusal.
Two players share one audio-frame 32-step clock and hold separate
sounding/pending releases. Retry NOTE_OFF before admitting another ON;
gates mirror admitted notes, mods latch only on ON. First high param
samples are baselines; RESET > STOP > MUTE > START > step.
No process/reset allocation, locks, JSON, logging or I/O.
Native completed-STOP/playhead telemetry is **not** exposed by the
existing generic seam: Keel decision requested before S1(b).
