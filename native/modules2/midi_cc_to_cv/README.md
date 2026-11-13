<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CC to CV (`midi.cc_to_cv`)

One `midi` event input and eight signal outputs `cc1`…`cc8`. Patch any row output
onto a parameter with an ordinary modulation cable; no Learn or mapping service.
Each row configures channel 0 Omni/1–16, controller0–127, `fourteen_bit`, `bipolar`,
and `smoothing_ms` (default 5), plus optional finite numeric `min`/`max`.
Defaults are CC1…CC8, 7-bit, 0..1, Omni. If an endpoint is absent,
its bipolar-derived default applies independently (minimum -1 when bipolar,
otherwise 0; maximum 1). Inverted and equal endpoints are valid; nonnumeric
or nonfinite endpoints are refused before the running graph is replaced.

The runtime JSON adapter translates `config.rows` into the validated, versioned
v2 control-side configuration in `config.h`, retaining decoding of older
68-byte v1 configurations with bipolar defaults; patch files store JSON,
never native struct bytes. Fourteen-bit rows require MSB controller0–31 and pair LSB+32.
MSB and LSB history is independent per endpoint/channel. An MSB updates the coarse
value immediately; the following LSB refines it. MIDI2 CC uses its native 32-bit
value. Group0 only. The effective configured range maps normalized 0..1 to min..max before
smoothing; `bipolar` is preserved in JSON even with explicit endpoints.
A finite one-pole reaches its target
and snaps within1e-7; smoothing0 is immediate. Disconnect clears pair history but
holds the last continuous target. State restore resets targets to each effective `min`. Learn observation
and D8 row persistence belong exclusively to Studio UI (#287), backed by
the transient sidecar MIDI Learn seam (#381); DSP does not write patch JSON.
