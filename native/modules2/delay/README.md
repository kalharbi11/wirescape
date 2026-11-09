<!-- SPDX-License-Identifier: GPL-3.0-only -->
# fx.delay: stereo delay

Module ID `fx.delay` (library `ws2_delay`, category Effects). A clean stereo delay: echoes that follow a patched
clock, feedback that holds instead of growing, a tone control inside the loop, and ping-pong. No tape character.

## Ports (IDs are final once merged)
| Kind | ID | Name | Notes |
|---|---|---|---|
| in | `left` | In L | audio, `WS2_SUM` (poly cables are summed), stereo pair with `right` |
| in | `right` | In R | audio, `WS2_SUM`; unconnected reads `left` |
| in | `clock` | Clock | trigger/gate, `WS2_FIRST`; when connected the delay time follows the clock |
| out | `left`, `right` | Out L/R | one voice each, stereo pair |

## Parameters
| ID | Range | Default | Notes |
|---|---|---|---|
| `time` | 0.001 to 2.0 s, log | 0.375 | ignored while a patched clock has a measured period |
| `division` | stepped 0 to 7 | 5 | x1/4, x1/3, x1/2, x2/3, x3/4, x1, x3/2, x2 of the clock period |
| `feedback` | 0 to 1 | 0.4 | 1.0 holds repeats at about the input level (loop limiter), never growth |
| `tone` | -1 to +1 | 0 | in the loop: -1 low-pass 800 Hz, 0 flat (bit-exact bypass), +1 high-pass 1.5 kHz |
| `mode` | stepped 0 or 1 | 0 | 0 Stereo, 1 Ping-pong (input averaged to mono, bounced L, R, L...) |
| `mix` | 0 to 1 | 0.35 | equal power; 0 is the dry input bit for bit, 1 has no dry |

## Behaviour
- Time changes glide (one-pole, about 50 ms, read speed limited to 0.5x..1.5x), so a jump is a short pitch bend, never a click.
  Reads use 4-point Hermite interpolation.
- Clock: period measured between rising edges. The first interval applies at the second edge; a later change must be seen
  twice (within 2%). No edge for 4 s keeps the last period. Delay is clamped to 1 ms .. 2 s. Unpatching the clock returns to `time`.
- Loop: tone filter, DC blocker (10 Hz), then a soft limiter (linear up to 1.0, ceiling 1.5) on the signal written to the line.
  Output stays within +-2 for a +-1 input at any setting.
- Memory: 2 lines of ceil(2 s x rate) + 8 floats, `WS2_LARGE`, requested in `prepare`. No allocation in `process`/`reset`.
- State: a version word only. Lines clear on `reset` and on `load_state`.
- The output is never flagged CONSTANT (tails make that rarely useful).
