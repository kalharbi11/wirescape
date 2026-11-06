# Performance: rules, measurements, costs

Target (from the prototype brief): 60 fps on the laptop and **≥ 30 fps on the Pi** with 20 modules, 40 cables and modulation moving. On the Pi the UI shares the CPU with the audio engine.

## Rules

1. **Animate only `transform` and `opacity`** on HTML elements; that work is composited and close to free.
2. **One `requestAnimationFrame` loop** applies the latest telemetry. No per-widget timers and no CSS keyframes for signal-driven motion. (CSS/WAAPI are fine for one-shot *events*.)
3. **Skip what isn't visible.** An `IntersectionObserver` stops every off-screen widget; the kit's top bar shows the "live widgets" count.
4. **Redraw only when dirty.** Canvases redraw when their inputs change or while they are modulated. Cable paths are recomputed only while something moves.
5. **Throttle text.** Readouts update at 10–15 Hz; motion at the frame rate. Changing text costs layout.
6. **Never call `getPointAtLength` per frame.** Sample the path once into a lookup table (see `pathLUT`). Measured: **1.8 ms → 0.05 ms per frame** for 16 travelling markers.
7. **Pre-render static layers** (dot grids, bifurcation maps, tick scales) once and `drawImage` them.
8. **Cache layout reads.** Read `offsetWidth` once, never after style writes in the same frame.
9. **Avoid on the Pi:** animated `box-shadow`, `filter`, `backdrop-filter`; animating `width/top/left`; `@property` + `conic-gradient` rings (these repaint every frame on the main thread); `will-change` on more than a handful of elements.
10. **Pi profile:** 30 Hz drawing (the kit's "Pi 30Hz" switch skips every other frame), no card shadows, simpler cable activity.

## Measurements

Headless Chromium on a cloud x86 container, 1400×1000, DPR 1, 2026-11-05. Treat the Pi as **several times slower** (not measured yet, OPEN-5); budget for 5–8×.

**Final kit (`ui/wirescape-ui-kit.html`)**

| situation | main-thread ms per frame |
|---|---|
| normal scrolling (only visible widgets run) | 0.07–0.45 |
| **every widget forced visible at once** | **≈ 1.2** |

Per widget, all forced visible (ms per frame):

| widget | ms | why |
|---|---|---|
| VCF multimode (both views running) | 0.55 | two filter curves, 110 points each, plus fill |
| filter curve display (modulated) | 0.22 | full canvas redraw every frame while the cutoff moves |
| LFO card | 0.13 | wave + phase canvas every frame |
| VCF wide / hero-led | 0.05–0.06 | small curve + knobs |
| knob L/M (modulated) | ~0.005–0.01 each | 2 rotates + 1 dash change |
| mixer (4 meters) | 0.025 | transforms only |
| info box (open) | 0.02 | 3 sparklines every 3rd frame |
| VCO, ENV, XY, faders, numbers | < 0.02 | |

What that means: a 20-module patch of stock cards is roughly 0.5–1 ms on a laptop. The things to watch on the Pi are **filter curves and other per-frame canvases while modulated**: draw them at 30 Hz and only while visible.

**Round-1 sketch numbers, for archived ideas**

| idea | cost |
|---|---|
| history mesh (P-archive, dropped) | ~3,000 hairline segments per redraw: high |
| comet trail on a knob (dropped) | 16 rotates per frame per knob |
| occupancy histogram (dropped) | 1 path rebuild every 2 frames |
| patch map with arrows (P3) | 0.05 ms with a LUT, 1.8 ms without |
| labelled peaks display (P5) | 0.19 ms (2 polylines + ~6 labels) |
| bifurcation map (P13-related, C4) | ~10 ms once, then an image |
| 120 ms popup fade/scale | effectively free (compositor) |

### App (Vite build)

Headless Chromium, cloud x86 container, 1920×1080, DPR 1, 20 modules, 40 cables including 10 modulations, a note held. `ticker.applyMs` is ms per frame applying telemetry, averaged over 240 frames, two runs each, same session. Values are branch (old look) / reskin.

| profile | zoom | branch (old look) | reskin | cards drawn (branch / reskin) |
|---|---|---|---|---|
| normal | fit | 0.25 / 0.33 | 0.13 / 0.26 | 20 / 20 |
| normal | 100% | 0.09 / 0.13 | 0.11 / 0.22 | 6 / 11 |
| pi (30 Hz) | fit | 0.45 / 0.50 | 0.33 / 0.38 | 20 / 20 |
| pi (30 Hz) | 100% | 0.15 / 0.25 | 0.31 / 0.31 | 6 / 11 |

- The branch's own recorded figure was 0.65 (fit) and 0.39 (100%).
- At 100% the reskin's cards are smaller, so about twice as many are on screen. Per drawn card the cost is about the same.
- Noisy (±50% run to run). Not Pi numbers (OPEN-5).

**Reproduce:** `npm run build && npm run preview`, open http://localhost:4173, load a 20-module patch with 40 cables, hold a note. Press `` ` `` for the dev panel, which shows ms/frame applying telemetry. Or read `window.__wirescapeTest.ticker.applyMs` (smoothed) after the frames you want to average; the fit and 100% zoom come from `__wirescapeTest.fit()` and `zoomTo(1)`. Switch the Pi profile in preferences (`setPref`).

**Where the reskin spends and saves time**

- Knob, per frame: 1 transform. The text (live value) updates at ≤ 14 Hz.
- Cables: the dotted (carrying signal) state is checked at 10 Hz and written to the DOM only when it flips.
- Cable sag springs are stepped only while unsettled.
- Filter and wave canvases redraw only when dirty.
- Info boxes bind to the ticker only while open.
- Meters are DOM transforms, not canvas.

## Library weights (if you ever need one)

The kit uses **no libraries**; CSS transitions, the Web Animations API, CSS `linear()` springs and one rAF loop covered everything. Sizes are min+gzip; see [REFERENCES.md](REFERENCES.md) for sources.

| option | size | note |
|---|---|---|
| CSS / WAAPI / `linear()` | 0 kB | Chromium 113+ for `linear()` |
| Motion (motion.dev) mini `animate` | 2.3–3.2 kB | WAAPI wrapper, springs; vendor pages disagree on the size |
| anime.js v4 | ~3 kB WAAPI-only to ~10–25 kB full | sources conflict; measure the imported modules |
| GSAP | ~23 kB core | all plugins free since 2025 under a no-charge (not open-source) licence |
| Rive runtime | 222–648 kB (brotli) | too heavy for controls |
| Lottie | not measured | likely overkill |

## How to measure

The kit's top bar shows `loop ms/frame · fps · live widgets · WAAPI`. For per-widget numbers, wrap each `tickers[i].fn` with `performance.now()` (the snippet used for the table above is in [AGENTS.md](../AGENTS.md#measuring)).

### Phase 6 cloud regression sample (2026-11-05)

Reproducible `tests/review-artifacts.spec.ts`: 10 LFOs + 10 VCFs, 40 cables
(30 modulation connections), a note held, Chromium 131 on GitHub's hosted x86
runner, 1920×1080, 120 animation-frame samples after warm-up.

| profile | zoom | telemetry application ms/update | animation-loop FPS |
|---|---|---|---|
| normal | fit | 0.693 | 60 |
| normal | 100% | 0.333 | 60 |
| Pi profile | fit | 1.171 | 60 |
| Pi profile | 100% | 0.583 | 60 |

The Pi profile applies telemetry at 30 Hz; the reported FPS is the requestAnimationFrame
loop, not a claim of 60 Hz widget updates. `ticker.applyMs` is a smoothed application
cost. This fixture contains more continuously redrawn filters than the earlier app
sample, so the tables are not a before/after comparison. None of these are Pi
hardware measurements; OPEN-5 remains open. Screenshots and a WebM recording are
uploaded with CI's `browser-review-and-traces` artifact, never committed.

## Phase 7 UI-only Raspberry Pi hardware baseline (pending)

A separate UI-only Pi pretest is planned in [PI_UI_TEST_PLAN.md](PI_UI_TEST_PLAN.md). **No valid headed physical Pi FPS result has been recorded yet.** An earlier assigned runner job was cancelled before any setup steps; hosted/cloud Chromium measures are not hardware evidence.

Before closing OPEN-5: extend the existing 120-frame timing sample to sustained post-warmup Pi intervals; add p95/p99 and long-frame pacing metrics, a signal-bearing VCO/Mixer/Output fake UI workload in addition to 20-module/40-cable modulation stress, separate watchdog checking, actual GPU/backend/display resolution/DPR/refresh and read-only thermal/load context. Target remains ≥30 actual Pi UI FPS with acceptable frame pacing. Record the exact tested SHA, runtime/conditions, results and run link. **Native engine contention is a separate Phase 8 benchmark.**
