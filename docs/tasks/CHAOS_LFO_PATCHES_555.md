<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS new LFO-shape public patches (#555)

Two new public patch-v2 JSON entries use the already-merged native
`essentials.chaos_lite` `lfo_shape` parameter from #533:
`chaos-04-radio-static-fx.json` uses sample & hold (2) to step cutoff during
clocked autoplay accents; `chaos-05-moonlit-drift-pad.json` uses smooth
random (3) for slow resonant body-tuning drift under a keyboard ADSR.
Both preserve separate left/right output routes and avoid direct
keyboard gate -> CHAOS gate. No new modules, DSP or protocol.

Read `docs/PATCH_DESIGNER_GUIDE.md`, `apps/studio/patches/README.md`
and the current generated native module catalogue before edits.
Test offline, with no devices, LAN, hidden thresholds or claims of listening.
The new sound designs are candidates until owner audition. Patch guide
impact: updated current availability and examples in the same PR.

```acceptance
tier: T1
size: <= 900 changed lines, <= 6 files
must_pass:
  - python tools/patch_library.py check apps/studio/patches --shared
  - node --test apps/studio/tests-node/patch-library.test.mjs
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [DSP/ABI/SDK, patch format or parameter ID change, new dependency, test threshold/CI policy change, devices]
```
