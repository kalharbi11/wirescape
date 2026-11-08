<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape UI — design spec

Status: **current** (after control jam rounds 1 and 2 and the phase 3.5 merge, 2026-11-05).
Live reference: [`ui/wirescape-ui-kit.html`](../ui/wirescape-ui-kit.html). Open it in Chromium; every control works and the top bar shows the real per-frame cost.
Anything not in this file or the kit is either archived ([ARCHIVE.md](ARCHIVE.md)) or rejected ([DECISIONS.md](DECISIONS.md)).

## 1. Principles

1. **One drawing per control type.** Modules choose controls; they never restyle them. A fader looks the same in every module.
2. **Size follows use, and detail follows size.** The parameter a module is *played* with is big and carries a scale. Amounts and trims are small and carry only what stays legible.
3. **Rust means live.** The single accent (`--rust`) is reserved for live values, modulation and events. Static structure is ink, graphite and hairlines.
4. **No boxes behind displays.** Displays sit directly on the card. Waveforms get a fading dot grid; the filter curve keeps its frequency grid; the envelope sits on a baseline.
5. **Detail on demand.** Source breakdowns and connections live behind an **i**, not on the panel.
6. **Real values drive every animation** (from the prototype brief). Nothing fakes a signal with a CSS loop.
7. **Pi first.** See [PERFORMANCE.md](PERFORMANCE.md).

## 2. Tokens

Source: [`ui/tokens.css`](../ui/tokens.css). The kit sets these on `:root` at startup.

| token | value | use |
|---|---|---|
| `--paper` | `#ffffff` | page |
| `--canvas-ground` | `#f5f5f3` | patch viewport only (P1G Pi feedback) |
| `--card` | `#ffffff` | module cards, popovers |
| `--card2` | `#f4f4f2` | rare fills (tape mask, XY pad) |
| `--ink` | `#161616` | text, value arcs, meters, curves |
| `--graph` | `#2e2e2d` | card border, secondary ink |
| `--mute` | `#8c8c88` | labels, fader fill, ticks |
| `--hair` | `#c6c6c2` | tracks, grid lines |
| `--hair2` | `#e2e2df` | faint grid, dividers |
| `--rust` | `#c2501f` | live values, modulation range, events |
| `--blue` | `#2e5f80` | second source in info boxes, pitch hint |
| `--olive` | `#6d7a35` | cv hint |
| graphite cap | `#4a4a48` | knob caps (L and M) |
| page texture | dots, `rgba(0,0,0,.06)`, 16 px | page background |

**Cable palette** (user-picked colour, no meaning): ink `#161616`, rust `#c2501f`, blue `#2e5f80`, olive `#6d7a35`, ochre `#c99a2e`, slate `#5d6a73`.

**Jack ring colour follows the port hint** (brief): audio ink, pitch blue, cv olive, gate/trigger rust, events plum `#7b4a8c`. See OPEN-2.

**Type**

| role | font | size / weight |
|---|---|---|
| titles, module names | IBM Plex Sans | 500, 20 px (section) / 11 px (card name) |
| body, notes | IBM Plex Sans | 400, 13 px |
| values | IBM Plex Mono | 400, 11 px (M), 12.5 px (L), 9.5 px (S), tabular numbers |
| labels | IBM Plex Mono | 500, 8.5 px, caps, 0.09 em tracking, `--mute` |
| live value | IBM Plex Mono | 9 px, `--rust`, prefixed `→ ` |
| module code | IBM Plex Mono | 600, 10 px, 0.14 em tracking |

## 3. Value formatting

Round to what matters; never show digits that carry no meaning.

| unit | rule | examples |
|---|---|---|
| Hz | < 1 Hz: 2 dp · < 100: 1 dp · < 1 k: 0 dp · ≥ 1 k: kHz, 1 dp (≥ 10 k: 0 dp) | 0.35 Hz, 12.5 Hz, 440 Hz, 1.2 kHz, 12 kHz |
| ms | < 10: 1 dp · < 1000: 0 dp · ≥ 1 s: s, 1 dp | 4.5 ms, 375 ms, 1.2 s |
| % | 0 dp; bipolar shows a sign | 40%, +30%, −40% |
| dB | 1 dp, signed; ≤ −59.5 shows −∞ | −8.0 dB, +2.0 dB |
| st / ct | 1 dp semitones / 0 dp cents, signed | +7.0 st, +12 ct |
| ratio | stepped list, compressor-style | 1:1 1.1:1 1.25:1 1.5:1 2:1 3:1 4:1 6:1 8:1 10:1 20:1 ∞:1 |
| bpm | 1 dp | 118.5 bpm |
| pitch readout | note + cents, then Hz in small type | C4 +2¢ · 262 Hz |

Range bounds are written tidily ("20 Hz–20 kHz"). The full implementation is `fmt()` and `fmtS()` in the kit.

## 4. Control library

### 4.1 Knob, three tiers

Sweep is 270° (−135° to +135°). Travel follows the parameter taper (log for frequency/time).

| tier | size | track r | cap r | value arc | pressed arc | finish | carries |
|---|---|---|---|---|---|---|---|
| **L** hero | 144 px | 46 | 30 | 2.4 px | 3.6 px | graphite cap, white 2 px pointer | 50 minor + major ticks at the parameter's marks, scale labels, source chips, value, live value |
| **M** standard | 76 px | 26 | 17 | 2 px | 3.2 px | graphite cap, white 1.6 px pointer | end/centre (or mark) ticks, name, value, live value |
| **S** trim | 38 px | 14 | 9 | **2.4 px** | 2.8 px | wire (no cap mass), 0.9 px pointer to track | name, value (9.5 px), live dot |

- Use **L** for the one or two parameters a module is played with (cutoff, LFO rate, tune). Use **M** by default. Use **S** for amounts, depths, trims and offsets. Bipolar S/M knobs fill from 12 o'clock.
- **Hit area** ≥ 44 px for S on touch screens, even though it is drawn at 38 px.
- **Readout under the knob:** name (label style), value (mono), then live value `→ 2.6 kHz` in rust when modulated. On L only, source chips follow (neutral grey text in a hairline box).

**Interaction:** drag vertically (~180 px for the full range; shift ≈ 830 px). Wheel and arrow keys work once the knob is focused (1% steps, 0.2% with shift). Double-click resets to the default. Stepped parameters snap. Typing a value on click is in the brief; it is not in the kit yet.

### 4.2 Other controls

| control | when | spec |
|---|---|---|
| Fader + meter | levels compared down a row | dB scale on the left, 1 px hair track, **grey** 1.5 px fill, 16×8 thumb with a grey centre line, meter **7 px** black bar beside it |
| Meter | any level | rises instantly, falls 0.9 of the range per second, peak line holds 1 s then falls, clip bar above 0.97 for 1.2 s |
| Slider (h) | pan, mix, offsets, PW | needle thumb (2 px + cap); bipolar fills from the centre; rust tick = live |
| Segmented, **round fill** | 2–4 named modes | pill outline; the black indicator slides with an overshoot (`--bounce`, 340 ms) |
| Detent rotary | 5–8 ordered choices | glyphs (waves) or words around the dial; click or drag (16 px per step); pointer snaps with overshoot (360 ms) |
| Toggle | on/off | LED latch for now (**OPEN-1**) |
| Number, **plain dotted** | exact, rarely swept (BPM, ratio, voices) | label + value with a dotted underline, no box; drag sideways (0.004/px) |
| Tape ruler | time/frequency where neighbouring values matter | the scale moves under a fixed rust hairline; flick for inertia |
| XY pad | two parameters with a joint meaning | dotted grid; ring = set; rust dot + fading trail = live |
| Edit on the display | envelopes, filter curve | handles 3.2 px hollow; 1 px curve |

Parameter-to-control rule set (it can be inferred from the module data):

| the parameter is… | control |
|---|---|
| the thing you play the module with | knob L |
| continuous, swept sometimes | knob M |
| an amount of something else, or bipolar | knob S |
| a level compared across a row | fader |
| exact, rarely swept | number |
| 2–4 named states | segmented |
| 5–8 ordered states | detent |
| on/off | toggle |
| two parameters with a joint meaning | XY pad |
| a shape or a set of times | edit on the display |

**Proposed data extension** for `wirescape_modules.json`: an optional `role` per parameter (`hero | primary | secondary | trim | mode | toggle | fader`) and an optional `layout` per module (`tall | wide`). When `role` is missing it is inferred: choices → mode; stepped 0/1 → toggle; dB → fader; min < 0 → trim; first modulatable Hz/ms → hero; the next two → primary; the rest → secondary. Round 1 item S0 demonstrates the inference.

## 5. States and modulation

| state | look |
|---|---|
| idle | as drawn |
| hover | no change to the track; the **i** appears if the knob is modulated |
| pressed / dragging | value arc thickens (see 4.1); the track does not change |
| focus (keyboard) | as pressed |
| modulation target (while dragging a cable) | dashed rust ring outside the track, pulsing opacity 1.1 s |
| modulated | **one** rust arc inside the track shows the combined range `n ± Σ|amount|`, plus a rust live dot at `n + Σ amount·source` |

- Several sources are shown **only as their sum**, as a VCA would hear them; there is no per-source arc. The depth is edited on the cable or in the source; there is no depth handle on the knob.
- **Parameter info (i):** hover a modulated knob and a 13 px **i** appears at its top right. Hovering it opens the box; clicking pins it. Each source row (name in its colour, a 40-sample sparkline, its contribution `+6%`) sends a line into a **+** node, and the sum shows to the right: `+7% → 2.3 kHz`. 120 ms fade/rise.
- **Module info (i in the header):** shows the module's **connections** (`IN ← VCO 65 · out`, `OUT → MIX D6 · 1`) and its **modulated parameters** (`CUTOFF  LFO 1 → 1.3 kHz`).
- The global **Mod** switch hides all modulation drawing; the **Motion** switch (full / calm / off) slows or freezes it.

## 6. Displays

- No background box and no border.
- Waveforms (VCO, LFO): a fading dot grid (1 px dots every 8 px, alpha falling to 0 at the edges, cached per size/density), a 1.1 px ink line with round joins, and **3–4 px horizontal inset**. The LFO uses a phase-driven scrolling two-cycle output trace, newest at right, with `φ 116° +0.90` when phase is reported; without telemetry it remains static (P1G Pi correction).
- Filter curve: frequency grid (100 / 1 k / 10 k darker), dashed 0 dB line, dashed set-cutoff line, rust live-cutoff line, a light rust fill under the live curve; in graph mode, a hollow peak handle and `fc · Q` in rust.
- Envelope: baseline axis with segment ticks, 1 px curve, 3.2 px hollow handles, dotted sustain guide, A D S R letters (the active one in rust), rust voice dot. Number boxes below.

## 7. Modules

- **Card:** white, 0.5 px `--graph` border, **14 px radius**, a 1 px bottom shadow line `rgba(0,0,0,.1)`.
- **Proportions:** `tall` (default) or `wide` (a display beside the controls, for modules that need room). Each module picks one; they need not match.
- **Header (A):** `CODE  Name ……… id` in a 6×12 px padded row with a hairline under it. The **i** sits at the right end. View switches (knobs ⇄ graph) also sit in the header.
- **Ports: side rails.** Inputs on the left edge, outputs on the right, spread evenly along the full height, the 12 px jack straddling the card edge, label inside. Jack ring 1.5 px in the hint colour, graphite hole.
- **Parameter nodes** (owner, 3.5 review). A modulatable parameter can have its own node on the left rail, under the signal inputs: a 10 px jack with a rust ring and a rust label (rust = modulation). A modulation cable lands on the node, never on the knob.
  - A few **essential** parameters always have a node: `modPorts` in `panels/<id>.json`, else the hero and primary modulatable parameters, at most 2 (VCF: cutoff, resonance).
  - Any other parameter gets a node **the moment something modulates it**, and loses it with its last cable, so modules with many parameters (CHAOS-lite has 22) stay clean.
  - Dragging from a node with a cable pulls that cable off; from an empty node it starts a cable you drop on an output.
- **Mapping.** Right-click a signal output → **Map to a parameter…**: every modulatable knob on the other modules shows a pulsing dashed rust ring (and existing parameter nodes pulse). Click one to map it (amount +0.30); the node appears and the cable lands there. The other way round: right-click a knob → **Map from a source…**, then click an output. Esc or a click elsewhere cancels. Dropping a cable on a knob or a node still works.
- **Collapsed rails** (Grasshopper-style). A side with more than 5 ports shows 4 (or `ports.<side>.shown`), plus any that are connected, and a `− 4/8 +` control under the last node. `ports.<side>.priority` picks which show first (MIDI to CV: pitch, gate, velocity, notes). The count is saved per module in `ui.portsShown`.
- **CC numbers.** With `ports.output.assign: "cc"` (CC to CV) each output label reads `CC 1` and is typed into on click (0–127), saved in `ui.ccMap[module][port]`. *Note for the code team:* the engine has no CC-number parameter yet; this needs one per output (or a mapping message) before it does anything.
- **Header name.** When a module's name only repeats its code (`VCF VCF`), the header shows the category instead (`VCF Filter`, `LFO Motion`, `ENV ADSR` stays).
- **Layout inside:** hero first, primaries in a row, trims and selectors last. Use a display only where it explains something you can't hear on your own.
- **Filter graph mode** (multimode filters only): a header switch between `KNOBS` and `GRAPH`. *Note for the code team:* graph mode should add control you only get by seeing the curve, as EQ Eight does: drag the peak for cutoff + resonance, scroll on the peak for slope (12/24 dB), and a drive lift on the curve. Simple filters stay knob-only.

The stock set in the kit: VCF (hero-led), VCF multimode with the view switch, VCF wide, VCF slim strip (faders), VCO, LFO, ENV, MIX, CLK. CLK was not reviewed in round 2 (OPEN-7).

## 8. Motion

| kind | budget | examples (duration, easing) |
|---|---|---|
| signal | every frame, one rAF loop, transform/opacity or one canvas | live dots, meters, scopes, phase dots |
| event | one-shot, 150–400 ms, WAAPI on transform/opacity | port pulse (380 ms, scale 1→2.4, opacity .9→0), clock beat pop (220 ms), add module (240 ms, `--bounce`, from −10 px/0.96), **remove (120 ms fade)**, refused edit (280 ms shake ±5 px), connected (200 ms, scale 1.03 + port pulse) |
| interaction | follows the hand, never lags | segmented slide (340 ms bounce), detent snap (360 ms bounce), cable sag spring (k = 90, damping 0.02^dt; recomputed only while moving), tape inertia (velocity × 0.93 per frame) |

- `--bounce: linear(0,.18 5%,.6 13%,1.02 21%,1.13 27%,1.1 33%,1.01 43%,.97 52%,1.005 68%,1)`; `--ease: cubic-bezier(.2,.8,.2,1)`.
- Text readouts update at 10–15 Hz while motion runs at the frame rate.
- With `prefers-reduced-motion`, Motion starts at `off`.

## 9. Cables

- Colour carries no meaning; the user picks it from the palette (§2).
- **Idle:** solid 1.6 px. **Carrying signal:** dotted (round caps, dash 0.1/5, 2.4 px), static (OPEN-4). **Selected:** 2.6 px.
- Drag: sag spring as above. Hit area 12 px.
- **One smooth cubic, near level at each node** (owner, 3.5 review, revised). A cable leaves an output to the right and enters an input from the left, then bends in one curve: controls at `a + (L, s)` and `b − (L, −s)`, reach L = clamp(0.4·|dx| + 0.18·|dy| (+40 when backward), 28, 180) px, droop s from the sag spring, capped at 0.85·L so each end stays within ~40° of level. The end at a node stays visible even when the cable routes behind a card; no midpoint kink, no long rigid straight lead.

## 10. Contracts

The app is built against three contracts. Nothing below them may change without the real engine agreeing.

| contract | where | notes |
|---|---|---|
| Module data | `public/wirescape_modules.json`, typed in `src/modules/types.ts`; optional `public/panels/<id>.json` | loaded at boot; nothing is hand-coded per module |
| Engine interface | `src/engine/EngineClient.ts` | `FakeEngine` is the only implementation and the only place values are invented; `RealEngine` will replace it |
| Patch file | `src/patch/types.ts` (patch v2) | `PatchDoc` is the in-memory document; unknown fields round-trip |

- **Port identity is (direction, id), not id.** The catalogue reuses a port `id` across directions on one module (`essentials.ladder`, `essentials.output` and `essentials.vca` have an input and an output called `audio`; `essentials.scale_offset` and `essentials.slew` use `control` on both). Every lookup keys on the pair. Patch v2 endpoints stay `{module, port}`; the direction follows from the side of the connection.
- **Modulation is summed in knob space:** `effective = normToValue(param, valueToNorm(param, base) + Σ amount × source)`, clamped to 0–1 travel. An `amount` of 0.4 means four tenths of the knob's travel in any unit and respects the taper. Summing in the parameter's own units would make one amount a 4 ms nudge on an envelope and an 8 kHz leap on a cutoff. **Open question for the engine team (OPEN-9):** confirm the real engine does the same, or saved amounts will not transfer.
- **Unknown fields:** `PatchDoc` keeps the object each module and connection was parsed from and spreads it back on save; unrecognised top-level and `ui` keys are held in `extras`. Covered by a test.
- **Telemetry `outputs` (optional):** `outputs?: Record<'module:port', number>` is the current value (−1..1) of each connected signal output; audio outputs report their level. It drives dotted cables (§9) and the info-box sparklines. An engine that omits it still conforms. No `EngineClient` method changed.
- **Patch `ui.cableColors`:** `Record<connKey, paletteName>` inside `ui` (palette names: ink, rust, blue, olive, ochre, slate; default ink). IDs are unchanged.

## 11. Card sizing

Each module type keeps its own size (no grid), but the size comes from data.

- **Width** is derived: input gutter + output gutter + the widest row of controls (or the widest pane; in a `wide` card, pane + controls), plus 12 px padding per side. The header must also fit. The override key `width` replaces it. Gutters are sized from the longest port label: 22 + 5.6 px per character, at most 132 px, 14 px with no ports.
- **Height is measured once**, after the card is in the DOM, because the controls are content-sized. Measuring is batched for a whole patch (Pi rule 8). Port anchors are then derived from that height and never read again. A minimum height keeps the port pitch.
- **Ports** sit on side rails and are spread evenly over the card height. `HEADER_H` = 36 px (raised from 30 in the owner review so header controls clear the border); the body has 22 px padding above and 24 px below the controls. Minimum port pitch 30 px (mouse), 42 px (touch). Minimum height = `HEADER_H` + (port rows + 1) × pitch.
- Control footprints used for the width: knob L 150, M 84, S 58 px; fader 46; select 150; item gap 10.
- Fonts (IBM Plex, `@fontsource`, latin 400/500/600) are bundled so the Pi works offline. Boot waits for them because heights are measured.
- A test checks every module in both hit profiles for overflow.

**`panels/<id>.json` override keys** (all optional; listed in `public/panels/index.json`):

| key | effect |
|---|---|
| `code` | header code (`VCF`); derived from the id when absent |
| `width` | card width in px |
| `layout` | `tall` (default) or `wide` |
| `sections` | explicit rows of parameters: `label`, `columns`, `params`, legacy `hero` (lifts a secondary S to M) |
| `short` | shorter labels for cramped controls |
| `roles` | role per parameter (below) |
| `widgets` | force a widget kind (older overrides; `roles` is preferred) |
| `panes` | displays: `wave`, `envelope`, `meter`, `scope`, `steps`, `filter` |
| `paneOptions` | options per pane (the filter pane reads `graph` to offer KNOBS/GRAPH) |
| `hidden` | parameters left off the card (still saved and modulatable) |

**Roles.** Roles are `hero | primary | secondary | trim | mode | toggle | fader | number | readout | select`. When `roles` has no entry, the role is inferred in this order, per parameter:

1. has choices but is not a real choice (one choice) → `readout`
2. stepped 0/1 off/on → `toggle`
3. choices: up to 4 → `mode` (segmented), more → `select` (dropdown)
4. unit dB → `fader`
5. unit BPM, or step ≥ 1 and not modulatable → `number`
6. minimum < 0 → `trim`
7. first modulatable Hz or time parameter → `hero`
8. otherwise `secondary`; the first two secondaries become `primary`

Knob tiers: hero L, primary and secondary M, trim S. Auto sections have no labels and run in this order: hero, primaries, secondaries and faders, trims/numbers/readouts, modes/toggles/selects. Labels exist only where an override gives them. **Any parameter an override forgets is still placed, in a trailing "MORE" section.** A panel cannot silently drop a control.

## 12. Gestures

| gesture | behaviour |
|---|---|
| Drag a module header | moves it; moves the whole selection if it is selected |
| Drag empty canvas | pan |
| Shift-drag empty canvas, or middle-drag | box-select (adds to the selection) |
| Shift-click a module | add to / remove from the selection |
| Wheel, or trackpad pinch | zoom centred on the pointer, 25 %–185 % |
| Two-finger pinch | zoom (touch) |
| Double-click empty canvas | quick-add search at that point |
| Drag output → input, or input → output | patch a cable (either direction) |
| Drag from a *connected* input | grabs that cable's end to move it or pull it off |
| Alt-drag from a connected input | starts a new cable instead |
| Drop a cable on empty canvas | quick-add filtered to modules with a matching port; picking one completes the cable |
| Drop a signal output on a knob, or on a parameter node | modulation, starting at amount +0.30 (a re-dropped cable keeps its amount) |
| Right-click a signal output → Map to a parameter… | knobs on other modules light up; click one to map it; Esc cancels |
| Right-click a knob → Map from a source… | outputs on other modules light up; click one |
| Drag from a parameter node | with a cable: pull it off; without: start one, drop it on an output |
| `−` / `+` under a collapsed rail | show one fewer / one more port |
| Click a `CC n` label (CC to CV) | type the CC number |
| Shift-drop on a signal input | adds alongside instead of replacing |
| Click a cable | select it; a tool box appears at its midpoint; Delete removes it |
| Selected cable: colour swatches | pick ink, rust, blue, olive, ochre or slate; stored in `ui.cableColors` |
| Selected modulation cable: `amount +30%` | drag sideways 0.004 per px (shift 0.001); arrows ±0.01 (shift ±0.002); range −1…+1 |
| Hover a cable | title shows "from → to", and the amount for a modulation |
| Knob: drag | 180 px of travel covers the full range; shift ≈ 4.6× finer (≈ 830 px) |
| Knob: wheel | ±0.02 travel (±0.004 with shift) |
| Knob: arrows | ±0.01 travel (±0.002 with shift); PageUp/Down ±0.1; Home/End to the ends |
| Knob: double-click | reset to the default |
| Knob readout, number box, fader value: click | type a value. It starts from what's shown (`1.2 kHz`) and accepts `2k`, `1.2 kHz`, `A4` (Hz), `350 ms`, `1.5 s`, `-6`, `40%` or a choice label. A bare number is in the parameter's own unit, except a percentage, which reads as shown (`40` means 40%). Nonsense is ignored |
| Knob: no modulation handle | the depth is edited on the selected cable |
| Header KNOBS / GRAPH (VCF) | graph: drag the peak for cutoff + resonance; the card keeps its height |
| Envelope handles | hollow, draggable on the display; ADSR stages also as number boxes below |
| Number box: drag sideways | 0.004 of travel per px |
| Number box: click | type a value; arrows ±0.01 (shift ±0.002); double-click resets |
| Fader: drag | moves the thumb; wheel and arrows nudge (shift finer); Home/End to the ends; double-click resets; click the value to type |
| Segmented, toggle, select | click |

**Editing (phase 4).**

| action | keys / control | behaviour |
|---|---|---|
| Undo | Ctrl/Cmd + Z, top-bar ↶ | back one step, all the way to an empty canvas; the view stays where it is |
| Redo | Ctrl/Cmd + Shift + Z, Ctrl + Y, top-bar ↷ | forward one step; any new edit clears the redo stack |
| Copy / cut | Ctrl/Cmd + C / X | the selected modules, their parameters, labels, extra fields, and the cables *between* them (with colours); kept in `localStorage`, so paste works after a reload and in another patch |
| Paste | Ctrl/Cmd + V | new instance ids; cascades +40 px per paste; if the originals are off screen, lands in the middle of the view; unknown module types are skipped with a toast; the pasted group is selected |
| Duplicate | Ctrl/Cmd + D | copy + paste in one step |

**What one undo step is** (`src/patch/History.ts`): history is snapshot-based (the patch minus the view, up to 200 steps). Changes made while a pointer is down are committed on pointerup, so a whole knob drag, module drag, cable drop, stereo pair or mapping click is one step. Other changes (keys, typed values, paste, delete) are committed after 350 ms of quiet, so holding an arrow key is one step too. Loading or starting a patch, and restoring the autosave, starts a fresh history. Undo rebuilds the cards from the snapshot (one `reset`) and reloads the engine graph through `loadGraph`.

**Replace or add:** a signal input takes one cable; a second drop replaces the first; shift-drop adds. The PLAN default for Q-B is "add (sum)"; this is still to be confirmed.

**Stereo pairs:** when both ends belong to a two-port stereo group, a bar offers to connect L and R together. Pairing is positional (OPEN-10). Declining connects the one cable.

**Feedback cables:** a cable whose target can already reach its source gets a small chevron at its midpoint.

## 13. Test hooks

Every interactive element carries a `data-testid`; a test asserts none is missing. `window.__wirescapeTest` (`App.testApi`) exposes `state()`, `load()`, `save()`, `autosaveNow()`, `clearAutosave()`, `patchName()`, `zoomTo()`, `fit()`, `addModule()`, `connect()`, `select()`, `deleteSelection()`, `setPref()`, `noteOn()`, `noteOff()`, `effective()`, `fps()`, `starters`, and the live `doc`, `engine`, `ticker`, `canvas` and `registry`.

| testid | element |
|---|---|
| `module-<id>`, `module-header-<id>`, `module-title-<id>`, `poly-<id>` | card, header, title, poly badge |
| `port-<id>-<direction>-<port>` | jack |
| `param-port-<id>-<param>` | parameter node |
| `ports-more-<id>-<direction>`, `ports-less-<id>-<direction>` | collapsed-rail + / − |
| `context-menu`, `context-map` | right-click menu, its Map item |
| `knob-<id>-<param>`, `readout-<id>-<param>`, `readout-input-<id>-<param>` | knob, its readout, the typing input |
| `number-<id>-<param>` | number box |
| `fader-<id>-<param>`, `fader-thumb-<id>-<param>` | fader, thumb |
| `seg-<id>-<param>-<value>`, `segmented-<id>-<param>` | segmented button, group |
| `toggle-<id>-<param>`, `select-<id>-<param>` | toggle, dropdown |
| `view-<id>-knobs`, `view-<id>-graph` | header view switch |
| `filter-<id>`, `wave-<id>`, `envelope-<id>`, `scope-<id>`, `meter-<id>`, `steps-<id>` | displays |
| `info-<id>-<param>`, `module-info-<id>` (plus `-box`) | parameter and module info buttons and boxes |
| `cable-<key>`, `cable-tools`, `cable-color-<name>`, `cable-amount` | cable, selected-cable tools |
| `library-item-<module-id>`, `quick-add-<module-id>`, `debug-<switch>` | library, quick-add, dev switches |

Tests (`npm test`): 23 in `tests/session.spec.ts` (21 test blocks; the patching test runs at 50/100/185 % zoom) 12 in `tests/kit-controls.spec.ts` for the kit controls, and 5 in `tests/review-35.spec.ts` for the owner's 3.5 review (parameter nodes, mapping, collapsed rails and CC numbers, cable leads).

## 14. Frame figures

Headless Chromium, cloud x86 container, 1920×1080, DPR 1; 20 modules, 40 cables including 10 modulations, a note held. Value is ms per frame applying telemetry (`ticker.applyMs`), averaged over 240 frames, two runs each, same session.

| profile | zoom | branch (old look) | reskin | cards drawn (branch / reskin) |
|---|---|---|---|---|
| normal | fit | 0.25 / 0.33 | 0.13 / 0.26 | 20 / 20 |
| normal | 100% | 0.09 / 0.13 | 0.11 / 0.22 | 6 / 11 |
| pi (30 Hz) | fit | 0.45 / 0.50 | 0.33 / 0.38 | 20 / 20 |
| pi (30 Hz) | 100% | 0.15 / 0.25 | 0.31 / 0.31 | 6 / 11 |

- The branch's own recorded figure was 0.65 (fit) and 0.39 (100%).
- At 100% the reskin's cards are smaller, so about twice as many are on screen. Per drawn card the cost is about the same.
- Numbers are noisy (±50% run to run). These are not Pi numbers (OPEN-5). Method and cost sources: [PERFORMANCE.md](PERFORMANCE.md#app-vite-build).

## 15. App layout of the source

```
src/
  modules/    the catalogue: types, registry, taper maths, value formatting
  patch/      PatchDoc and the patch v2 types. No DOM.
  engine/     EngineClient (the contract) and FakeEngine (the only invention)
  canvas/     Viewport, CableLayer, ModuleCard, PatchCanvas
  panels/     layouts.ts (roles, sizing), PanelBuilder, panes.ts, widgets/
                widgets: Knob, NumberBox, Fader, Segmented, Toggle, Selector,
                InfoBox, FilterDisplay, WavePreview, EnvelopeCurve, Meter,
                Scope, StepsPane
  runtime/    Ticker, the single animation loop
  system/     status bar, toasts
  styles/     app, chrome, controls, displays, info CSS
  theme/      tokens.css and preferences
  app/        App, Library, QuickAdd, Keyboard, DevPanel, Modal, starters
public/
  wirescape_modules.json    the real catalogue, unmodified
  panels/<module-id>.json   optional per-module layout overrides
tests/        session.spec.ts, kit-controls.spec.ts, helpers.ts
reference/    the brief and the first visual sketch
```

## 17. System (phase 5)

Everything here runs against the fake engine; the real engine fills the same fields.

**Status bar** (bottom): audio state chip (`STARTING`, `RUNNING`, `NO DEVICE`, `DEVICE LOST`, `RECONNECTING`); device · sample rate · buffer · latency; CPU % (rust above 80); dropouts (rust above 0); BPM and run/stop when a Clock exists; OSC chip `OSC 9000 · LOCAL` while OSC is on (rust `NETWORK` when it accepts from the network); one chip per MIDI device with an activity dot; PANIC; SETTINGS.

**Toasts for changes under the user:** a MIDI device connected or disconnected; the audio device lost ("reconnecting when it returns"), no device, and "Audio is back".

**Settings**, grouped:

| group | items |
|---|---|
| Audio | output device, sample rate (the device's rates), buffer size (latency follows) |
| MIDI | each device on/off (all on by default) |
| OSC | server on/off (off by default), port (1024–65535; anything else is refused and rolled back), allow network (off = localhost only), recent messages (last 8, live) |
| Interface | motion, performance profile, **switch to Pi when slow** (on by default), hit areas, UI scale, theme, port labels |
| About | version, build (git sha · date), cores, profile; **Check for updates** (a stub: answers "Up to date" after 0.6 s, nothing leaves the machine) |

On/off settings use the LED latch (OPEN-1 default), the same as panel toggles.

**Pi profile, automatic.** At boot, a device with ≤ 4 cores, ≤ 4 GB or an ARM user agent starts in the Pi profile. At run time, if frames stay below 40 fps for 5 s in a row (tab visible), the profile switches to Pi once per session with a toast. The setting "Switch to Pi when slow" turns this off. The debug panel's "Simulate slow frames" exercises it.

**Contract 2 additions (optional, nothing breaks):** `SystemStatus.osc?: OscStatus`; `EngineClient.setSampleRate?`, `setBufferFrames?`, `setOsc?`. An engine that omits them conforms; the UI hides what's missing.

**Debug panel** (`` ` ``): FPS and frame time; fault switches: unplug audio, plug in a MIDI device, CPU spike, dropouts, refuse edits, OSC traffic, simulate slow frames.

## 16. Open items

| id | question | current default |
|---|---|---|
| OPEN-1 | toggle style (round 2 Q6 got no pick): LED, switch, invert, square or words | LED latch |
| OPEN-2 | jack rings keep hint colours (brief) while cables have none — keep? | keep hint colours on jacks |
| OPEN-3 | cable palette: round 2 note was cut off at "replace purple with like…" | purple replaced by ochre `#c99a2e`; grey made slate `#5d6a73` |
| OPEN-4 | active cable: static dots vs slowly crawling dots | static dots |
| OPEN-5 | Pi measurements of the kit (FPS, ms per frame) | not measured yet; see PERFORMANCE.md |
| OPEN-6 | which stock modules are `wide` | VCF offers both; others tall |
| OPEN-7 | the Clock card was not reviewed in round 2 | as in round 1 |
| ~~OPEN-8~~ | typed value entry on click (brief) | **done in phase 4**: knobs, number boxes and faders share one parser (`parseTyped`) |
| OPEN-9 | modulation maths: does the real engine sum in knob space (§10)? saved amounts depend on it | knob space |
| OPEN-10 | stereo pairing is positional (Nth stereo output to Nth stereo input); the catalogue marks `stereo_pair: "stereo"` without naming L/R. A real group id would be better | declaration order |
| OPEN-11 | poly cable weight: moot. Cable weight no longer depends on kind or poly (§9). Whether `poly` on a port is static or the runtime voice count is still unanswered | not used by the UI |
| OPEN-12 | instance labels are stored in `ui.labels`, keyed by instance id; confirm the real format or name the field | `ui.labels` |
| OPEN-13 | patch name is stored in `ui.name` by the autosave wrapper; should patch v2 carry a name? | `ui.name` |
| OPEN-14 | backward cables (target left of source) draw as a wide curve across the canvas; busy in a dense patch. A routing pass that bows them vertically may help | drawn as is |
| ~~OPEN-15~~ | `example.step_sequencer` gate parameters are `0=Rest, 1=Note, 2=Tie` with `step: 1`; the placeholder grid cycles three states. The signature panel should show tie as a joining bar | **done in phase 6:** editable pitch/velocity lanes and gate tie bars |
| OPEN-16 | the CPU figure is faked from the module count; what does the real engine report, DSP load or process CPU? | faked |


## 18. Signature panels (phase 6)

The phase-3.5 design is approved for this continuation. All signature panels use the shared controls and catalogue parameters; CHAOS-lite remains generic. `PanelBuilder` has no module-specific branches.

- **Step Sequencer:** eight lanes of dotted NumberBoxes for pitch (octaves) and velocity (displayed as a percentage), and neutral Rest / Note / Tie gate buttons. A tie draws a joining bar. Run uses the LED latch; Length uses an exact number. Shortened sequences dim unused steps without deleting values. The rust playhead uses telemetry phase times the actual sequence length; stopping clears it. Width 800 px fits the lanes at mouse and touch density.
- **Macro Oscillator:** a numbered model bank follows the catalogue's 0–23 range (no model names are supplied, so none are invented). Harmonics is L; timbre/morph are M; pitch and modulation amounts are S, followed by the LPG group. Arrow keys navigate by the displayed column count, Home/End jump to bounds. Touch uses four columns with 44 px targets. This pane subscribes to no animation loop.
- **Mixer:** shared A/B gain faders and shared meter ballistics, driven by FakeEngine post-gain channel levels. `Telemetry.meters` stays the same numeric pair: Mixer labels it A/B, Output L/R. Hidden generic controls retain explicit modulation nodes. Effective gains feed both channel meters and the summed output.
- **Output:** stereo meter and scope remain; width is derived from the display and rail footprints rather than the former 300 px override. The catalogue supplies no Output gain control.

Six original Qt starters from `kalharbi11/wirescape` PR #192, source `96cad1cd7eeef673db6b5cbdcf09d4dc9008ab9d`, are bundled offline alongside the original six browser demos. Source IDs, parameter values, connections, config/settings/output fields and licence text survive. Browser positions are spaced for the larger controls. Real MIDI, polyphony and sound prerequisites are described in the library. This ports fixtures from an unmerged source PR; it does not imply Qt acceptance or merge.

Phase-5 follow-up: audio settings tolerate zero devices, hide unsupported optional setters, refresh sample-rate choices when devices change, and call `setBufferFrames` through the engine contract. Closing Settings releases the OSC listener. An unsaved motion preference honors the OS reduced-motion setting.

Pi checkpoint B / OPEN-5 remains **unmeasured on hardware**. Cloud browser and source validation are not Pi measurements. The real-engine work is now Phase 8. Phase 7 is the Pi UI-only pretest.

Phase-6 cloud frame samples (20 modules / 40 cables) and the recording procedure are in PERFORMANCE.md. The drone and Macro starter LFOs are moved below the Ladder and expanded touch Macro panels so controls remain clear. Every bundled starter is checked for card overlap at mouse and touch density.

## 19. Revision after phases 3.5–6

Phase 2B-a (#281): the top-bar **KEYS** button reveals a chromatic input strip
above the status bar, hidden by default. Its 18 keys match the computer-keyboard
range and OCT setting; −/+ and Z/X share octave selection. Pointer press and
focused-key Enter/Space send the existing engine note commands. Rust marks a
locally held key, not received MIDI or synthesized telemetry. Overlapping local
sources share pitch ownership. Release outside, cancellation, blur, hidden-page
and Panic release notes; hiding the strip releases its sources only. Inputs and
modal dialogs continue to block background computer notes. All updates are
event-driven, with no timer or animation-loop subscription.

Phase 2B-d (#289): Settings → MIDI has a remembered **Computer keyboard plays**
switch, on by default (including older preferences without this field). Off
releases only musical-key shortcut notes and disables their Z/X octave shortcuts.
Explicit on-screen keys and enabled MIDI inputs remain separate. Both local key
sources continue through the existing engine note commands; no device is opened
by this preference.

Keyboard note releases remember their original pitch across octave changes. Modal dialogs contain Tab navigation, restore focus on close, and block background note/edit shortcuts. Keyboard edits pending in history commit before a new pointer gesture; click changes commit at the end of that gesture, while repeated keyboard edits still group over 350 ms. Window blur releases a held history gesture.

The ticker accumulates sparse `effective` parameter deltas until replaced or deleted, including updates received while off screen or between draws. A density rebuild preserves this state; graph load/undo starts a fresh telemetry epoch. Other telemetry fields remain packet snapshots. Faders only draw a rust live marker when the effective value differs from the base and clear it when values agree.

See [PHASE8_HANDOFF.md](PHASE8_HANDOFF.md) for native schema and asynchronous refusal gaps. Phase 8 must adapt these explicitly rather than assuming the prototype file is already a runnable native graph.

## 20. Phase 7 — physical Pi UI-only pretest (planned)

Phase 7 checks the current interface with `FakeEngine` on real Raspberry Pi hardware, using the separate `pi-ui` runner and **headed system Chromium**. Test browser rendering, real monitor size/scaling, keyboard/mouse/touch-*profile* geometry, cables/controls, editing/recovery, large signature panels and simulated system faults. Include sustained modulation-heavy and signal-bearing FakeEngine workloads. Execution and acceptance gates: [PI_UI_TEST_PLAN.md](PI_UI_TEST_PLAN.md).

**OPEN-5 remains unresolved:** the physical run must capture exact tested revision, renderer, resolution/DPR, normal/Pi × fit/100% results, ≥30 actual rAF FPS with acceptable pacing and resource/thermal context in [PERFORMANCE.md](PERFORMANCE.md). Cloud FPS or 30Hz ticker frequency do not count as Pi hardware evidence.

Only UI/browser issues are fixed here. Native patch codec and output routing, real-host acknowledgements, genuine telemetry, actual sound/MIDI/OSC, DSP correctness, audio latency, and engine contention are **Phase 8**, documented in [PHASE8_HANDOFF.md](PHASE8_HANDOFF.md).

### Conditional parameter controls (#306)

A catalogue parameter may carry `visibleWhen: { param: "mode", values: [7] }`.
The card shows it when the named parameter's saved base value matches a listed
value (or its descriptor default when absent). DSP modulation does not change
visibility. Hidden controls retain their values and modulation cables; their
rail nodes remain available. Empty rows and section labels disappear, and the
card remeasures and reroutes cables only when visibility changes. Unknown or
malformed conditions leave the control visible. Static panel `hidden` still wins.

### Crash recovery (#295)

An active-session marker distinguishes unexpected termination from orderly pagehide.
A valid autosave from an unclean session or unexpected engine exit is offered in
the existing modal style. Autosave pauses until Restore or Keep current patch
(including Escape/close); pending choices survive reload without overwriting the
snapshot. Restore preserves the name and unknown patch fields. Clean sessions
retain automatic last-session restoration. Recovery does not write a user file
or request an engine restart; Electron main retains its crash-loop budget.

### Patch browser (#294)

PATCHES lists six bundled starters and three demos as read-only, plus bounded
user patch basenames in the desktop app. Bundled choices follow the advertised
catalogue in web-demo builds. Selection uses loadGraph; only dirty documents
prompt before switching. A bundled selection detaches the native file target,
and Save creates a user copy through Save As in the patch folder. User data
and unknown fields use the normal lossless document path. Optional openPatch
library list/read/reset requests retain the five-member preload surface and
existing return shape; main owns containment, UTF-8 and 2 MiB bounds. Browser
builds return an empty user list and keep normal file-picker Open unchanged.
