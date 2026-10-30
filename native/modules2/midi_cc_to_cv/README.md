<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CC to CV (`midi.cc_to_cv`)

One `midi` event input and eight signal outputs `cc1`…`cc8`. Patch any row output
onto a parameter with an ordinary modulation cable; no Learn or mapping service.
Each row configures channel 0 Omni/1–16, controller0–127, `fourteen_bit`, `bipolar`,
and `smoothing_ms` (default5). Defaults are CC1…CC8,7-bit,0..1,Omni.

The runtime JSON adapter translates `config.rows` into the validated, versioned
68-byte control-side configuration in `config.h`; patch files store JSON, never
native struct bytes. Fourteen-bit rows require MSB controller0–31 and pair LSB+32.
MSB and LSB history is independent per endpoint/channel. An MSB updates the coarse
value immediately; the following LSB refines it. MIDI2 CC uses its native 32-bit
value. Group0 only. Bipolar maps 0..1 to −1..1. A finite one-pole reaches its target
and snaps within1e-7; smoothing0 is immediate. Disconnect clears pair history but
holds the last continuous target. State restore resets targets to0 (or−1 bipolar).
