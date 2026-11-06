<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Decision log

What the user chose in each control-jam round, and why the spec looks the way it does. The current spec is [DESIGN.md](DESIGN.md).

## Round 1 (2026-11-05, 52 items)

| topic | decision |
|---|---|
| knob types | hero (L), standard (M), trim (S), fader, sliders, tape ruler, detent, XY: **keep** |
| hero knob | wire liked at first, but the pointer was too faint → hero ended up **black / graphite** (round 2) |
| small knobs | thin outer arcs vanish when small → thicker value arc |
| layers | **no** ghost needle, comet trail, histogram, value band or inner scope |
| pressed state | the track must not darken; the value arc thickens instead |
| several sources | show **only the combined result** (as into a VCA), one rust range; no second blue arc |
| depth handle | dropped in round 2 (it duplicates the range) |
| readouts | range ruler and inside-cap readouts dropped; readouts were too busy (musical meaning, ms period dropped) |
| info | wanted: a small, light info box behind an **i**, with leader lines per source |
| number boxes | no grey box; rounding to what matters; ratios like Ableton's compressor |
| segmented, toggle | wanted more variants (round 2) |
| displays | no grey box behind the VCO waveform or the ADSR; ADSR too thick/cartoonish |
| filter | the hero-led graphic is liked; graph mode for multimode filters, with a coders' note on extra control |
| slim strip | keep, as part of a library of controls styled the same everywhere |
| LFO | keep; its sync button should match the other controls |
| mixer | thicker meter bars; fader line dark grey so it is not confused with the meter; fix the right meter (now ballistic) |
| cables | colour difference too weak; no meaning in colour; the line should turn dotted (Grasshopper); user-picked colours |
| remove animation | a quick fade |
| chaos / signature | periodic-vs-chaotic knob demo dropped; the attractor drawing is kept aside (P4) |
| notation section | tri-spoke shape (P1), jog wheel (P2), patch map (P3) etc. archived; history mesh, pill pairs and serif type dropped |
| polar plot | archived, to be redrawn minimal (P9) |

## Round 2 (2026-11-05, 19 items)

| item | pick | applied as |
|---|---|---|
| Q1 hero pointer | **B** graphite cap, 2 px pointer | graphite `#4a4a48` for every solid cap (L and M) |
| Q2 small knobs | **C** wire, 2.4 px | S tier = wire, 2.4 px value arc |
| Q3 hierarchy | **C** graphite · graphite · (small from Q2) | L/M graphite, S wire |
| Q4 numbers | **C** plain, dotted underline | the default number style |
| Q5 segmented | **E** round fill | the default segmented style |
| Q6 toggle | — (no pick) | LED kept, OPEN-1 |
| Q7 info | **A + C** | parameter i: sources converge into a **+ sum node** (the sum in the middle, as asked); module i: connections + modulated params |
| Q8 readout | **A** `→ live value` | the default |
| Q9 waveform | **C** fading dot grid | plus an inset so the stroke is never cropped at the right edge |
| Q10 ADSR | **A** baseline axis | |
| Q11 filter view switch | keep | the multimode VCF has a KNOBS/GRAPH switch in its header |
| Q12 cables | dotted while active; more saturated grey; "replace purple with like…" (cut off) | slate grey `#5d6a73`, ochre `#c99a2e` instead of purple (OPEN-3) |
| K2 states | the depth handle is unnecessary | removed |
| K4 slim strip, K6 mixer | keep | |
| K5 LFO | wave view, no grey background | wave only, dot grid |
| E1 theme | white; the page background white too | `--paper: #ffffff`, dot texture |
| E2 shape | **B** (tall, r 14) and **D** (wide) as options | per module: `tall` or `wide`, 14 px radius |
| E3 ports | **A** side rails, spread | |
| E4 header | **A**; E archived | P14 |

## Phase 3.5 merge (2026-11-05): conflicts resolved

The branch `claude/new-session-p817y1` (phases 1–3, old look) was merged onto `main` and reskinned. Where the two disagreed, `DESIGN.md` and this file won.

| # | topic | branch | now | why / source |
|---|---|---|---|---|
| 1 | tokens, type | warm paper (`#eeede9` ground, `#faf9f6` cards), ABC Areal | `ui/tokens.css`: white paper, 16 px dot texture, IBM Plex Sans/Mono, bundled with `@fontsource` (latin 400/500/600) so the Pi kiosk works offline; boot waits for fonts because card heights are measured; old token names are aliases in `src/theme/tokens.css` | DESIGN §2 |
| 2 | cable colour and weight | colour by port hint; weight by kind/poly (2 px, poly 3, modulation 1.6 dashed, events 2.4 fine-dashed, selected 4.5) | user-picked palette (ink, rust, blue, olive, ochre, slate) stored per cable in `ui.cableColors[connKey]`, default ink, picked by swatches at the midpoint of a selected cable; idle solid 1.6, carrying signal dotted (0.1/5, 2.4, static), selected 2.6; hint colours only on jack rings (OPEN-2); sidebar legend "SIGNAL TYPES" renamed "PORT RINGS" | DESIGN §9, round 1 |
| 3 | cable shape | horizontal S-bezier | sagging quadratic curve; while a module is dragged or a cable pulled, the control point follows a damped spring (k 90, damping 0.02^dt), stepped from the one ticker loop only while unsettled | DESIGN §8 |
| 4 | knob modulation | needle followed the live value; one arc per modulation stepping 5 px outward, each with a draggable amount handle | cap and pointer show the set value; one rust arc inside the track shows `n ± Σ|amount|`; rust live dot; `→ live value` line (L/M, ≤ 14 Hz); source chips on L; depth handle gone; amount edited on the selected cable (`amount +30%`, drag 0.004/px, shift fine, arrows) | DESIGN §4.1, §5; round 2 K2 |
| 5 | knob sizes | 70 / 51 px; section `hero` flag set size | tiers L 144 / M 76 / S 38 (S wire, L/M graphite cap) by parameter role (DESIGN §4.2 inference), optional `roles` per parameter in `panels/<id>.json`; `hero` only lifts a secondary S to M | DESIGN §4.1–4.2 |
| 6 | widget rules | 2–5 choices segmented, > 5 dropdown | 2–4 segmented (round fill), > 4 dropdown; the 5–8 detent is not built (no catalogue parameter has 5–8 choices); new plain dotted number box (stepped non-modulatable parameters, BPM) and fader (dB, or role `fader`); single-choice readout kept | DESIGN §4.2; round 2 Q4, Q5 |
| 7 | value formatting | branch rules | true minus sign, signed bipolar values, Hz/kHz and ms/s rounding, bpm 1 dp, cents `ct`, `40%` with no space | DESIGN §3 |
| 8 | card header | 82 px (category, big title, id tag, grip) | header A, 30 px: `CODE · Name ……… #id · i` (+ view switch); `code` from `panels/<id>.json`, else derived from the id | DESIGN §7; round 2 E4 |
| 9 | card chrome | radius 13, soft shadow; selection outline rust; port candidate in accent | radius 14, 1 px bottom line, 0.5 px graphite border; selection outline ink (rust is for live, modulation, events); port candidate ink; knob modulation target = dashed rust ring pulsing 1.1 s | DESIGN §5, §7 |
| 10 | ports | fixed 31 px pitch from 70 px under the header, label gutters with INPUTS/OUTPUTS heads | side rails: 12 px jack straddling the edge, label inside, spread evenly over the card height; minimum pitch 30 px (touch 42) | DESIGN §7; round 2 E3 |
| 11 | card height | derived arithmetically, never measured | measured once after the card is in the DOM, batched for a whole patch (Pi rule 8), since the controls are content-sized; port anchors derived from that height and never read again; width still derived from the controls or `width`; the overflow test guards both hit profiles | PERFORMANCE rule 8 |
| 12 | sections | always labelled ("CONTROLS", "MORE", "MODE", "01 / FILTER") | auto sections unlabelled (hero, primaries, secondaries, trims/numbers, modes/toggles); labels only where an override gives them (CHAOS-lite, Macro Osc, MIDI to CV, Step Seq); per-module widths removed from overrides; the "MORE" catch-all stays | DESIGN §7 |
| 13 | toggle | slide switch | LED latch | OPEN-1 default |
| 14 | displays | LFO wave in a box with a "CONTROL SIGNAL" head; plain envelope | LFO on a fading dot grid, inset, rust phase line and dot, `φ 116° +0.90`; envelope with baseline axis, A D S R letters, hollow draggable handles, ADSR stages as number boxes; new filter pane (hero-led strip) on VCF and Ladder; VCF has the KNOBS/GRAPH switch (drag the peak for cutoff + resonance; card height kept); VCF tall | DESIGN §6, §7; round 2 Q9–Q11; OPEN-6 default |
| 15 | meter | canvas | DOM with transforms, kit ballistics: rises instantly, falls 0.9/s, peak hold 1 s, clip bar 1.2 s | DESIGN §4.2 |
| 16 | motion | none specified | DESIGN §8 added: add 240 ms bounce drop-in, remove 120 ms fade, connected 200 ms pop and port pulse, refused cable shake 280 ms, event port pulse 380 ms; all WAAPI one-shots, off when Motion is off | DESIGN §8 |
| 17 | info boxes | none | parameter **i** (hover a modulated knob: sources → + node → sum and result) and module **i** (connections and modulated parameters) | DESIGN §5; round 2 Q7 |
| 18 | contract 2 (`EngineClient`) | telemetry without per-output values | Telemetry gains an optional `outputs?: Record<'module:port', number>` (−1..1 of each connected signal output; audio outputs report level); drives dotted cables and info sparklines; an engine that omits it still conforms; no method changed | DESIGN §10 |
| 19 | contract 3 (patch v2) | no cable colours | `ui.cableColors` added inside `ui`; IDs unchanged; unknown fields still round-trip (test passes) | DESIGN §10 |
| 20 | kept from the branch (not reskin items, open at checkpoint A) | replace-on-drop plus shift-drop adds; readout shows the set value; typed value on readout click; dark theme wired | all kept. Replace/add: PLAN Q-B default is "add (sum)", unresolved. Readout shows the set value, now with the live line under it. Typed entry kept and extended to number boxes (OPEN-8). Dark theme wired, not designed (Q-C). Knob shift-drag fine factor changed from 5× to ~4.6× (≈ 830 px) | DESIGN §4.1 |
| 21 | tests | 23 session tests | two adjusted for geometry and new classes only: the Delete-cable test clicks a point on the curve (a sagging cable's bounding-box centre is off the path); the catalogue test counts `.control-fader` and `.control-number` as controls. New `tests/kit-controls.spec.ts` (12 tests) covers the kit controls | |

Rejected outright (don't revive without asking): ghost needle, comet trail, histogram, value band, inner scope, range ruler, inside-cap readout, leader-line popups as value readouts, history mesh, pill pairs with contour labels, serif display type, periodic-vs-chaotic knob demo, colour-coded cables, the grey display box, the per-source modulation arcs.

## Phase 3.5 owner review (2026-11-05)

| note | applied as |
|---|---|
| Cables vanish where they go behind a module | cables leave and enter every node horizontally before bending (DESIGN §9), so the end at the node is always visible |
| Filter has no cutoff/resonance nodes although they can be modulated | parameter nodes on the left rail: essentials always (VCF cutoff + resonance), others appear once mapped (DESIGN §7) |
| How to map (Ableton-like) | right-click an output → Map to a parameter… → targets pulse → click; or right-click a knob → Map from a source… |
| Too many nodes (CC to CV, MIDI to CV, Macro Osc) | rails with > 5 ports collapse to 4 + connected, with `− n/N +`; CC to CV labels take a typed CC number (needs an engine parameter, flagged) |
| LFO: no vertical rust line | phase dot only, on the dot grid |
| Revise the reskin's design | headers no longer repeat the code (`VCF Filter`); horizontal output meters with a dB scale; Clock leads with a large tempo number, duty + ticks/beat below; LFO controls regrouped (rate; depth · beats · offset; sync); CC to CV card height follows its collapsed rail |
| Cable shape was inconsistent (hook in the middle, too straight at some ends) | one cubic per cable, near level at both nodes, droop capped by the reach; the owner approved the short loop into a gate and the curl into an input from below |

## Phase 4 (2026-11-05): editing

| topic | decision |
|---|---|
| undo model | snapshots of the patch (minus the view) per gesture, not per-feature inverse commands: every edit is undoable with no extra code; the cost is a `JSON.stringify` of the patch per step (well under 1 ms at 20 modules) and a card rebuild per undo |
| step boundaries | pointer gestures commit on pointerup; everything else after 350 ms of quiet |
| view | not part of history (undo never moves the camera) |
| clipboard | in-app plus `localStorage` (`wirescape.clipboard.v1`), not the system clipboard: works across reloads and patches without a permission prompt |
| typed values | one parser for all controls; a bare number is in the parameter's own unit, except a percentage (read as shown) |
| Qt starters (#187) | not ported: the Qt repo was not reachable from this session |

## Phase 5 (2026-11-05): system

| topic | decision |
|---|---|
| card padding (owner) | header 30 → 36 px; body 12/14 → 22/24 px top/bottom; sides unchanged |
| OSC | off by default, localhost only by default; port range 1024–65535, refusals roll back with a toast; recent-message monitor in Settings |
| engine contract | only optional additions (`osc?`, `setSampleRate?`, `setBufferFrames?`, `setOsc?`) so a real engine without them still conforms |
| automatic Pi profile | boot sniff (cores, memory, ARM) plus a run-time watchdog (< 40 fps for 5 s, once a session), user can turn it off |
| update check | a stub that never touches the network |
| settings switches | LED latch, as the panels (OPEN-1 default) |


## Phase 6 continuation (2026-11-05)

Owner authorization: assume phase 3.5 approved, preserve its design language, finish phases 5 and 6. No further design checkpoint blocks implementation. The existing stacked phase branches remain unmerged pending owner review.

| topic | decision |
|---|---|
| signature panels | data overrides plus small reusable `steps`, `models`, `channels` panes; no module branch in the generic builder |
| sequencer | shared exact-value controls, explicit Rest/Note/Tie, joining bars, actual-length telemetry playhead; values outside length retained |
| Macro models | numbered values from catalogue; no invented model names or fake waveform preview; responsive keyboard navigation |
| Mixer/Output | existing faders and meter style; telemetry A/B after gain for Mixer, L/R for Output; Output width derived, no invented output controls |
| starter polish | move the drone LFO below the Ladder and the Macro LFO below its expanded touch panel; guard all browser and Qt starter layouts against overlapping cards at both densities |
| Qt starters | port six fixtures from PR #192 at `96cad1cd7eeef673db6b5cbdcf09d4dc9008ab9d`; keep existing browser demos so their documented session remains reproducible |
| system audit | optional audio capabilities hidden; no-device safe; device rates refreshed; OSC unsubscribed on close; reduced-motion default matches OS |
| automation | GitHub Actions installs locked dependencies, typechecks/builds and runs Chromium regression suite; failure traces retained |
| hardware | OPEN-5 stays open until actual Pi measurements; CHAOS-lite, dark theme and phase 7 remain deferred |

## Revision before phase 7

Fix reproduced history gesture boundaries, stuck keyboard note releases after octave changes, modal focus/background shortcuts, sparse effective-value retention and fader live-marker reset. Preserve the approved design and synchronous prototype engine contract. The real engine adapter must supply native graph metadata/output buses and define asynchronous rejection handling; these are integration work, not visual redesign. Keep actual Pi display benchmarking separate from hosted headless evidence.

## Roadmap renumbering — 2026-11-05

The owner authorized **Phase 7** as a separate Raspberry Pi *UI-only* pretest in Wirescape-UI, using the dedicated self-hosted `pi-ui` runner, headed Chromium and `FakeEngine`. **Phase 8** is the real native engine integration primarily in `kalharbi11/wirescape` ([PHASE8_HANDOFF.md](PHASE8_HANDOFF.md)).

This does not reopen the approved design system. No Pi run was started, no native code was changed, and performance OPEN-5 remains unmeasured on physical hardware. Q-B (ordinary-drop replace vs Shift-add, while the plan favors sum/add), touch access, physical readability and signature panels are Phase 7 owner acceptance items. Preserve previous rejected designs unless a specific Pi UI defect warrants revisiting them.
