<!-- SPDX-License-Identifier: GPL-3.0-only -->
# MIDI to CV (`midi.to_cv`)

Input `midi` is an event stream. Outputs: poly `pitch`, `gate`, `velocity`,
`aftertouch`, `pressure`, `bend`, `mod`, and assigned-voice `notes` events.
C4=0 pitch, 1/oct; velocity/pressure/mod 0..1, bend −1..1. Channels are voices.

Stable parameter slots are in `module.json`: voice_count 1–16 (default4), modes
rotate/reuse/reset/mono, oldest stealing (the shared allocator's supported policy),
mono last/low/high priority, legato, sustain hold/ignore, bend range 0–48 st (default2),
apply_bend, and channel filter (0 Omni,1–16). RPN0 is ignored; the bend range remains
manual and the service exposes `ignored_rpn`. MPE is out of scope.

Repeated MIDI1 Note Ons are fresh activations with generated IDs; anonymous offs
release FIFO, including a stolen activation's late off. Steals emit CHOKE before
replacement ON at the same offset. Sustain is separate per endpoint/channel.
Channel pressure, poly pressure and CC1 remain distinct outputs. Panic and source
choke release notes even with sustain held. CC120/123 releases only that MIDI channel.
Changing allocation/filter settings releases old notes before configuring the core.

The planner's fixed `on_new_note` policy emits balanced activation changes while
sustaining mono-legato gates. No retrigger parameter is exposed. Legacy parameter
slot5 is reserved; sustain/bend/filter IDs keep slots6–9. An optional `off` policy
is post-J9 and is not implemented.

Only UMP group0 is converted; other groups are preserved by MIDI In and counted as
ignored by this converter. State restores parameters through the host and resets
held notes; it never revives a stale sounding activation. No devices are opened.
