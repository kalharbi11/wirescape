<!-- SPDX-License-Identifier: GPL-3.0-only -->
# sound.blend: design notes

## Provenance
- **Prototype:** the owner's *Conjunctions* web app (`src/dsp.js`, `src/procdefs.js`, attached in the #302 builder session, 2026-11-06), processes `mix` (Mix · crossfade) and `splice`. Conjunctions renders whole files offline.
- **Code:** original; no third-party code. CDP's `submix` / `edit` are named in the prototype only as inspiration.
- Licence: GPL-3.0-only.

## What changed from the prototype, and why
| Prototype | Here | Why |
|---|---|---|
| Two process cards | One module, `mode` a stepped, modulatable parameter; a mode change glides | Same sources, same law; one panel |
| Mix motion over the render (static / sweep / back / swing at a rate) | Dropped: **Balance** is modulatable; patch an LFO or envelope | Render-length motions don't exist live (Compass, #302) |
| Splice edges planned for the whole file at once, then rendered | Streaming: each segment's length is drawn when it starts, so CV on the segment lengths acts from the next segment | Live; no look-ahead, no latency |
| Fade only at splice edges | One raised-cosine glide for every jump: splice switch, trigger, mode change | Clickless in every case; the trigger is new |
| `Rng(4242)` per render | `ws::Random` seeded per instance, saved in state | Reproducible across block sizes and reloads |
| Provenance tint for the scope | Dropped | Display is Phase 3A telemetry |
| `levelise` / `finish` | None | The equal-power law is unity at the ends and −3 dB per side in the middle |

**New: `switch` trigger.** It forces the next splice segment at its sample, which lets a clock or sequencer play the splice rhythmically. In Crossfade it flips the side Balance measures from (a CV of 0.2 becomes 0.8), an instant A/B swap that still glides.

## Parity with the prototype
Same law (`cos/sin` of the position · π/2) and the same raised-cosine edge as `P.mix` and `P.splice`. The tests check the parts that can be compared without the prototype's random edge plan:
- the equal-power gains;
- exact segment lengths with jitter 0;
- the raised-cosine shape over exactly `fade`;
- jitter bounds and mean.

The prototype's jitter sequence comes from its own RNG, so splices aren't sample-identical; the statistics match.

**Listening note:** I can't listen here. The owner should check Splice at 30–80 ms segments with Fade under and over 3 ms, and Crossfade with a slow LFO on Balance.
