<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape UI guidance

Repository workflow: [root AGENTS.md](../../AGENTS.md). These rules cover the UI design system only.

## Read in this order

1. [`docs/DESIGN.md`](docs/DESIGN.md): the spec (tokens, formatting, controls, states, modulation, displays, modules, motion, cables, open items).
2. [`ui/wirescape-ui-kit.html`](ui/wirescape-ui-kit.html): the reference implementation. Open it and use it; when the spec and the kit disagree, ask.
3. [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md): rules plus measured costs. Read it before adding anything that moves.
4. [`docs/DECISIONS.md`](docs/DECISIONS.md): what the user chose and rejected. Don't revive rejected ideas.
5. [`docs/ARCHIVE.md`](docs/ARCHIVE.md): parked ideas (P1–P15). Don't build them unless asked.
6. [`docs/REFERENCES.md`](docs/REFERENCES.md): libraries, plugin precedents, visual references.

## Files

| path | what |
|---|---|
| `ui/wirescape-ui-kit.html` | self-contained reference page (published at https://claude.ai/artifact/CV9V15CkywzgEpt2Q1hhb9) (no libraries, ~110 kB): tokens, control library, states + info boxes, displays, 9 stock module cards, motion, cables, archive index |
| `ui/tokens.css` | design tokens as CSS variables |
| `src/` | the app: catalogue, patch, engine client + `FakeEngine`, canvas, panels and widgets, ticker (layout in DESIGN §15) |
| `tests/` | Playwright: `session.spec.ts` (23), `kit-controls.spec.ts` (12) |
| `public/` | `wirescape_modules.json` (generated from native manifests; never hand-edit: run `python tools/gen_ui_catalogue.py` and commit the result. CTest `ui_catalogue_sync` fails on drift) and `panels/<id>.json` overrides |
| `reference/` | the prototype brief and the first visual sketch |
| `index.html` | app entry (Vite) |
| `docs/*.md` | spec, performance, decisions, archive, references |
| `archive/sketches/control-jam-r1.html` | round 1 exploration (~60 ideas, incl. archived ones), published at https://claude.ai/artifact/G6WHy9jfYvcedE6b8E2P6d |
| `archive/sketches/control-jam-r2.html` | round 2 options page, published at https://claude.ai/artifact/QgS2XUea5qkom9bWimizmM |

The first visual sketch and the 21-module catalogue are now in `reference/` and `public/`.

## Running the app

```
npm ci
npm run dev      # Vite dev server
npm test         # builds, serves on :4173, runs Playwright
```

Playwright uses the Chromium at `/opt/pw-browsers/chromium`, or the one named by `WIRESCAPE_CHROMIUM`. Press `` ` `` in the app for the dev panel (fps, ms/frame). `window.__wirescapeTest` is the test API (DESIGN §13).

## How the kit's code is organised

One `<script>`, top to bottom:

1. **utilities**: `h()` / `s()` (HTML/SVG element builders), `arc()`, `pol()`, `canv()`, `fmt()` / `fmtS()` (value formatting, DESIGN §3), themes/tokens (`C`, `THEMES`).
2. **`Param`**: range, taper (lin/log), step, choices, `mods` (modulation list), `live()` (modulated value), subscriptions.
3. **globals + loop**: `G` (mod/motion/finish map/Pi switch), `S` (current value of each mod source), `SRC` (source names/colours), `tickers` + an `IntersectionObserver`, `sims`, and the single `requestAnimationFrame` `loop()`, which also feeds the perf readout.
4. **`knob()`**: tiers `TIERS`, finishes `drawFinish()`, combined modulation arc, live dot, interaction.
5. **controls**: `numBox`, `tape`, `fader`, `meter`, `slider`, `seg`, `detent`, `toggle`, `xy`.
6. **`jack()`, `card()`**: ports with event pulses; module cards with side rails.
7. **page scaffolding**: `section()`, `item()`.
8. **shared module params + sims**: `P.*` (filter, oscillator, LFO, envelope, mixer, clock), the LFO/clock/envelope simulations standing in for engine telemetry.
9. **displays**: `vcfResp` / `vcfDisplay`, `waveCanvas` / `dotGrid`, `adsrView`, `pathLUT`.
10. **info**: `attach2`, `sumInfo` (parameter i), `modInfo` (module i).
11. **sections 00–06**, each `item(sec,'ID',title,note,opts,build)`.

The sims (`sims`, `S`, `ENV`, `onStep`) stand in for the brief's `EngineClient.onTelemetry`. In the real app they become telemetry fields; the drawing code doesn't change.

## Mapping: kit → where it now lives

| kit | app |
|---|---|
| `ui/tokens.css`, `C` / `THEMES` | `src/theme/tokens.css` (old token names kept as aliases), `src/theme/theme.ts` |
| `knob` | `src/panels/widgets/Knob.ts` |
| `numBox` | `NumberBox.ts` |
| `fader` | `Fader.ts` |
| `seg` | `Segmented.ts` |
| `toggle` | `Toggle.ts` |
| `attach2`, `sumInfo`, `modInfo` | `InfoBox.ts` |
| `vcfDisplay` | `FilterDisplay.ts` |
| `waveCanvas` | `WavePreview.ts` |
| `adsrView` | `EnvelopeCurve.ts` |
| `meter` | `Meter.ts` |
| role → control rules (DESIGN §4.2) | `src/panels/layouts.ts` |
| `card`, `jack` | `src/canvas/ModuleCard.ts` |
| cable styles (DESIGN §9), sag spring | `src/canvas/CableLayer.ts` |
| `sims`, `S` | `src/engine/FakeEngine.ts` |
| the single `loop()` | `src/runtime/Ticker.ts` |

Not built from the kit: `slider`, `detent`, `tape`, `xy` (no catalogue parameter needs them yet).

## Working rules

- Follow the brief's Pi rules and [PERFORMANCE.md](docs/PERFORMANCE.md). Check the top-bar `loop ms/frame` after any change.
- Rust is only for live values, modulation and events.
- Use the controls; don't restyle them per module.
- Record new design decisions in `docs/DECISIONS.md`, and update `docs/DESIGN.md` when the spec changes.


### Measuring

Per-widget cost, all widgets forced visible (paste into `page.evaluate`):

```js
new Promise(res=>{const st=new Map();for(const t of tickers){const f=t.fn;t.fn=(...a)=>{const t0=performance.now();f(...a);
 const id=(t.el.closest('.item')||{}).id||'?';st.set(id,(st.get(id)||0)+performance.now()-t0);};t.vis=true;}
 let n=0;const c=()=>{if(++n<120)requestAnimationFrame(c);else res([...st].sort((a,b)=>b[1]-a[1]).map(([k,v])=>k+' '+(v/120).toFixed(3)+' ms'));};requestAnimationFrame(c);})
```

