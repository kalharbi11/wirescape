<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Blend (`sound.blend`)

Joins two sources, **A** and **B**, in time. One position (0 = A, 1 = B) drives an equal-power mix, `A·cos(p·π/2) + B·sin(p·π/2)`, so the middle is −3 dB per side and doesn't dip.

| Mode | What it does |
|---|---|
| 0 **Crossfade** (default) | The position follows **Balance**. Patch an LFO, envelope or slew into Balance for sweeps and swings |
| 1 **Splice** | Alternating stretches of A (**A segment**) and B (**B segment**), both running at their own pace, each length varied by up to ±**Jitter**. Short segments make a rhythmic gate; long ones sound like channel hopping |

Every jump (a splice switch, a **Switch** trigger, a mode change) glides the position along a raised cosine over **Fade**, so nothing clicks. Under about 3 ms the joins sound like hard tape cuts.

## Ports
| ID | Direction | Notes |
|---|---|---|
| `a` | in | Audio. Polyphonic cables are summed (SUM). Unpatched = silence |
| `b` | in | Audio, summed. Unpatched = silence |
| `switch` | in | Trigger (first channel), read with a Schmitt trigger. A rising edge starts the next splice segment now; in Crossfade it flips which side Balance measures from |
| `audio` | out | One voice |

## Parameters
| Slot | ID | Range (default) | Notes |
|---|---|---|---|
| 0 | `mode` | 0 Crossfade / 1 Splice | Stepped, modulatable; a change glides over Fade |
| 1 | `balance` | 0–1 (0.5) | Crossfade position, per sample |
| 2 | `seg_a` | 0.01–2 s (0.22), log | Splice: length of A stretches, read when a segment starts |
| 3 | `seg_b` | 0.01–2 s (0.22), log | Splice: length of B stretches |
| 4 | `fade` | 0.001–0.2 s (0.01), log | Glide length for every jump. Segments are at least two fades long |
| 5 | `jitter` | 0–1 (0.3) | Random variation of each segment length, ±jitter, from `ws::Random` (seeded per instance) |

No latency. The splice starts on B (the position starts in the middle) when the module resets in Splice mode.

## State
Everything: position, glide, mode, current source, samples left in the segment, the trigger's Schmitt level and the RNG. A loaded instance continues exactly.
