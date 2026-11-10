<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove S1 native design

Original C++17, no imported third-party code; Westlicht Performer is only
a behavioral reference. Keel contract: #432 comment 6731562789.
Persist additive version-1 JSON, not validated 4×64 POD; refuse unknowns.
Two tracks share audio-frame 32-step clock with per-track sounding/pending
notes. Failed NOTE_OFF retries before new ON; gates and mods follow admitted
notes. Initial high is baseline. RESET > STOP > MUTE > START > tick.
Render/reset avoid allocation, locks, JSON, logging and I/O.
Observation ports append after the ten published outputs (no ABI edit):
`t1_pos`/`t2_pos` (slots 10/11, CV_BI) report the current 0-based step,
or -1 only after that track's transport stops, gate is zero and all owed
NOTE_OFFs have drained. A queued STOP, mute or refused release never proves
completion. `t1_loop_lo`/`t2_loop_lo` (12/13) and
`t1_loop_hi`/`t2_loop_hi` (16/17) carry the exact low/high 16-bit halves
of the uint32 loop counter; non-render readers reconstruct `lo | (hi << 16)`.
`t1_fired`/`t2_fired` (14/15, gate) report whether the current note trig
passed its condition, inherited through continued ties, false on rests/STOP.
Before G1 conditions, every note trig passes. Loop starts at 1 on Start/Reset
and advances at wraps. All observation buffers are ordinary channel-0 signals.
Sidecar and WASM install uncabled taps and consume one render-owned last-frame
snapshot. JSON creation stays on the non-render reader. See ENGINE_PROTOCOL §7
and #497 for the additive maps and current-generation stopped Apply gate.
