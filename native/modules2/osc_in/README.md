<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC In (`osc.in`)

Eight `row1`…`row8` signals plus `messages` events; no inputs or parameter knobs.
The host's `config` JSON selects UDP port(default9000), `lan`(false), and up to8 rows:

```json
{"port":9000,"lan":false,"rows":[{"pattern":"/pico1/knob/*","argument":0,"kind":"cv","input_min":0,"input_max":1,"output_min":0,"output_max":1,"smoothing_ms":5,"timeout_ms":1000,"stale":"hold"}]}
```

Kinds: `cv`, `gate` or `trigger`. CV clips/scales finite numbers to the output range
and smooths them; gate uses0/1 and threshold0.5; trigger fires at least1ms even for a
message with no arguments/Impulse. At most64 triggers per row/block are accepted.
Stale `zero` sets a timed-out target to0; `hold` retains it. Timeout0 disables expiry.
Default rows match `/pico1/knob/1`…`8`. Unknown kinds, bad ranges and invalid patterns
fail preparation. Raw messages are forwarded as valid OSC wire packets.

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

Sockets live exclusively in the host service. Default binding is127.0.0.1; LAN needs
both module `lan:true` and explicit global confirmation. This module never opens a
socket. See `docs/CONTROLLERS.md` for controller and sender setup.
