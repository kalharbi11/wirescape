<!-- SPDX-License-Identifier: GPL-3.0-only -->
# References

✓ = page or search result checked during the design rounds (2026-11). ○ = pointer worth checking.

## Animation and rendering

| resource | what for | |
|---|---|---|
| [CSS `linear()` easing (Chrome docs)](https://developer.chrome.com/docs/css-ui/css-linear-easing-function) | springs and bounces without JS; Chrome/Edge 113, Firefox 112, Safari 17.2 | ✓ |
| [Motion: migrate from GSAP](https://motion.dev/docs/migrate-from-gsap-to-motion) · [GSAP vs Motion](https://motion.dev/docs/gsap-vs-motion) | the mini `animate` (~2.3 kB) WAAPI wrapper; vendor pages | ✓ |
| [anime.js v4 docs (mirror)](https://www.mintlify.com/juliangarnier/anime) | modular, tree-shakeable; sizes conflict, so measure on bundlejs.com | ✓ |
| [Webflow: GSAP becomes free](https://webflow.com/blog/gsap-becomes-free) · [GSAP licence](https://gsap.com/community/standard-license/) | all plugins free; a no-charge licence, not open source | ✓ |
| [Rive runtime sizes](https://rive.app/docs/runtimes/runtime-sizes) | 222–648 kB; too heavy for controls | ✓ |
| Web Animations API (MDN), `IntersectionObserver`, `OffscreenCanvas` | the native tools the kit uses | ○ |
| [Josh Comeau: spring physics](https://www.joshwcomeau.com/animation/a-friendly-introduction-to-spring-physics/) · [Emil Kowalski: animations.dev](https://animations.dev/) | when and how much to animate | ○ |

## Controls and modulation in plugins

| resource | takeaway | |
|---|---|---|
| [FabFilter Pro-Q knobs](https://www.fabfilter.com/help/pro-q/using/knobs) | typed entry ("2k", "A4"), slow-drag precision, reset, hover glow | ✓ |
| [Pro-Q band controls](https://prod.fabfilter.com/help/pro-q/using/bandcontrols) | a live activity bar inside the range ring | ✓ |
| [Ultra modulation](https://ultra.audio/help/synth/modulation/modulation) | a range ring plus a separate live-value dot (our model) | ✓ |
| [Bitwig modulators (guide)](https://www.audeobox.com/learn/bitwig/modulators-system-guide/) · [official](https://www.bitwig.com/userguide/latest/the_unified_modulation_system/) | ring around the knob = modulation range | ✓ |
| [Phase Plant docs](https://kilohearts.com/docs/phase_plant) | every knob/slider is a vertical drag | ✓ |
| Forum pain points: [Vital amount indicator](https://forum.vital.audio/t/modulation-amount-rotary-indicator-confusing/9513), [Zebra bipolar half-ring](https://www.kvraudio.com/forum/viewtopic.php?p=9248813), [HISE sources cancelling](https://forum.hise.audio/post/115973), [turn animated rings off](https://www.kvraudio.com/forum/viewtopic.php?p=9174518) | why we show one summed range and offer a Motion switch | ✓ |
| [VCV Rack component library](https://git.kx.studio/VCVRack/Rack/src/commit/b2e00b2b06456998f916ebe81463dab98a2fe421/include/componentlibrary.hpp) | knob classes by size = tiers | ✓ |
| [Mutable Instruments Rings manual](https://cdn.softube.com/user-manuals/mutable-instruments-rings) | an attenuverter per CV input | ✓ |
| Ableton EQ Eight | what a filter "graph mode" can add | ○ |
| Ableton Compressor ratio steps | the ratio list in DESIGN §3 | ○ |
| Grasshopper (Rhino) | line style (dotted) marks data on wires; our "active cable" | ○ |

## Port placement precedents (round 2, E3)

Eurorack / VCV Rack: jack rows at the bottom · Max/MSP, Pure Data: inlet tabs on the top edge, outlets on the bottom · **Bitwig Grid, Blender nodes: sockets on the side edges** (chosen: side rails) · TouchDesigner: tabs centred on the sides. ○

## Visual references the user supplied (round 2)

- A yellow-on-black sequencer/routing GIF: hex instance ids, arrows travelling along connections, labelled peaks. → module ids, patch map (P3), labelled peaks (P5).
- The WORT recorder (Teenage Engineering-style): tri-spoke jog wheel, contoured labels, LCD. → P1, P2.
- A tri-spoke EQ (MID/BASS/TREBLE). → P1.
- Data-viz plates (polar sectors, venn blobs, zoned dot columns). → P9.
- A generative wire mesh. → dropped.
- "The Science of Sound" poster: rings, range bars with end caps, numbered sections. → P7.
- A technical diagram sheet (dashed paths, starbursts, hatching). → P8.

## Also worth a look ○

[Teenage Engineering OP-1 Field](https://www.thisispaper.com/mag/op-1-field-teenage-engineering) (✓ article) · [Ableton Learning Synths](https://learningsynths.ableton.com/) · Bret Victor, [Explorable Explanations](https://worrydream.com/ExplorableExplanations/) · Elektron OLED pages · monome norns · Max/MSP number boxes · test-equipment graticules (Tektronix/HP) · Braun / Dieter Rams.
