<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC In (`osc.in`)

Eight `row1`…`row8` signals plus `messages` events; no inputs or parameter knobs.
On builds with the OSC sidecar host (slice B of #72), the control thread
validates authored `config` fields and encodes a private POD for `prepare()`;
the module itself does not open sockets. Native OSC input starts only after
a successful graph commit. This example requires slice B to be merged:

```json
{"port":9000,"lan":false,"rows":[{"pattern":"/pico1/knob/*","argument":0,"kind":"cv","input_min":0,"input_max":1,"output_min":0,"output_max":1,"smoothing_ms":5,"timeout_ms":1000,"stale":"hold"}]}
```

Kinds: `cv`, `gate` or `trigger`. CV clips/scales finite numbers to the output range
and smooths them; gate uses0/1 and threshold0.5; trigger fires at least1ms even for a
message with no arguments/Impulse. At most64 triggers per row/block are accepted.
Stale `zero` sets a timed-out target to0; `hold` retains it. Timeout0 disables expiry.
Default rows match `/pico1/knob/1`…`8`. Unknown kinds, bad ranges and invalid patterns
fail preparation. Raw messages are forwarded as valid OSC wire packets.

Per-row `enabled` (#612) is an optional boolean. Absent means `true`, so older
patches keep listening; writers emit it only as `false`. A disabled row ignores
every message: a CV row holds `output_min` and a gate/trigger row stays low. Its
pattern and ranges are kept for re-enabling. A message that matches only
disabled rows is dropped: the sidecar's OSC worker never delivers it, and the
module would not forward it on `messages` either. So it produces no event and no
Monitor entry. Messages that match an enabled row, or no row at all, still pass
on `messages`, so a Monitor can still be used to discover addresses. If all eight
rows are disabled ("Clear"), the sidecar opens no UDP socket for the module,
`messages` forwards nothing, and the route state is `disabled`.

Wildcard numeric final path segments become poly channels, densely sorted by index,
up to the prepared channel limit(16). Nonnumeric suffixes on a wildcard index row are
ignored; other address globs may still select a mono row. Newly observed indices are
discovered/sorted per callback, so discovery can renumber channels at block boundaries.
Send initial values for all controls before playing when stable channel identities
matter. With a stable index set, smoothing and timeout processing are sample-based.
Coalescing is deliberately per callback: changing block boundaries can retain different
intermediate CV/gate messages, so that case does not promise identical renders.
Numeric rows coalesce latest-wins per channel/block at that last message's offset;
triggers retain individual arrival offsets. Other glob forms `?`, `[]`, `{}` work.

Only the native sidecar-owned host opens UDP sockets. Without owner
`--confirm-lan`, even a saved `lan:true` route binds `127.0.0.1`.
A port-busy route reports its failure through optional `status.osc.routes`
without refusing the patch; it receives no input. Up to 32 input routes
share listening ports. Socket routing is supported only in builds containing
the reviewed slice B; portable slice A alone has no UDP listener. Monitor
log presentation is separate slice C. No OSC networking exists in the web demo.
