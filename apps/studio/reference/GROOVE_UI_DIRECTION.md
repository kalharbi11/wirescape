<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove sequencer: UI direction (owner-approved study)

Status: design study, approved by the owner on 2026-11-09 as the direction for the
Groove editor. Mockup only: no engine, protocol, patch-format or module change.
Earlier, separate study: [GROOVE_S1_DESIGN_NOTES.md](GROOVE_S1_DESIGN_NOTES.md) (#457).
Plan context: [docs/sequencer/GROOVE.md](../../../docs/sequencer/GROOVE.md).

## Files

| file | what |
|---|---|
| [`ui/groove-r2-final.html`](../ui/groove-r2-final.html) | **The chosen design.** Self-contained, openable offline; demo clock, optional test tones. |
| [`ui/groove-r3-chords.html`](../ui/groove-r3-chords.html) | The chosen design plus chords (#514) and the scale in the transport row. |
| [`ui/groove-r1-concepts.html`](../ui/groove-r1-concepts.html) | Round 1: three concepts (pads + lanes, note roll, stages), research and comparison. Kept for the record. |

Open either file directly in Chromium. Every transport state, playhead and load
confirmation in them is simulated; none of it is engine truth.

## How we got here

**Round 1** compared three editing surfaces on one shared card, transport and step data:

- **A. Pads + lanes.** Groovebox style: Elektron trig keys, PER|FORMER layers, Novation Circuit paint views.
- **B. Note roll.** DAW clip editor, Squarp Hapax.
- **C. Stages.** One column per step, Metropolix-style.

The Squarp Hermod+ user reports were a warning against hiding values behind one
encoder and menus. The current 16-step module was judged hard to use: pitch shows
as octave numbers, each step has three small targets, Tie needs a click cycle,
16 steps wrap as 8+8, and there is no painting, keyboard entry or melody shape.

**Owner decisions after round 1**

- Groovebox pads are the default, with a Pads/Roll switch. The roll is where chords go later.
- Step count per track: 8 / 16 / 32 / 64, default 16. Each extra 16 steps is a new bar row underneath, so the grid expands downwards.
- No live editing until live editing is solved for all modules (S2).
- Less visual noise: fewer black borders, minimal menus, more space around the pads, square pads.
- Ties are not black lines.
- Try coloured notes (dark grey, dark red) and Elektron-style LEDs.
- Subtle cues for what a trig holds (modulation, conditions).
- Add Elektron-style trig conditions to the UI even though the module lacks them.

**Owner notes on round 2, folded into the final file**

- The first click on a pad only selects it; clicking the selected pad adds or removes the trig.
- Hold a note pad and drag up/down to step through notes; with Shift, octaves. The mouse wheel over a note does the same.
- The phone (web app) view was cropped, so it now has a vertical layout.

## Final layout

Module card in the Wirescape style (DESIGN §7). Top to bottom:

1. **Header:** `GRV Groove` · `PADS | ROLL` view switch (DESIGN §7 puts view switches in the header) · **i**.
2. **Transport:**
   - Round play/stop button (rust icon while playing) · Reset · status in grey mono (`PLAYING 1.3.2`, `STOPS in 9`).
   - A draft marker with Apply / Revert when the pattern differs from what is loaded.
   - Tempo (plain dotted number) · undo / redo.
3. **Tracks:** text tabs `T1 Lead` / `T2 Bass`, each with:
   - A status dot that starts/stops the track: rust = playing, dashed = queued, ring = armed.
   - Position `13/32`, and a mute latch.
   - On the right, **Steps 8 16 32 64** for the selected track.
4. **Lanes:**
   - `Trig · Note · Velocity · Length · Mod 1 · Mod 2 · Condition` as minimal text tabs (DESIGN §4.2 segmented, underline variant).
   - Scale (root + mode, a UI helper) · Copy / Paste / Clear for the bar holding the selected step.
5. **Grid:** bars of 16 stacked vertically, numbered 1–4 at the left; the playing bar's number is rust. Four beat groups of four. Pads are square: 56 px with a mouse, 68 px for touch.
6. **Inspector** for the selected step:
   - Step/bar, Rest·Note·Tie, Note (big; click to type `D#3` or a MIDI number), Velocity.
   - Length ¼ ½ ¾ 1, Mod 1, Mod 2, Condition.

The **roll view** keeps the same transport, tracks, inspector and step count:

- The grid fits the track length; bar lines every 16 steps.
- Notes use the pad colour, and ties draw as a lighter tail.
- Conditions are a small label above the note.
- A lollipop lane below edits Velocity / Length / Mod 1 / Mod 2.

### Menus and borders

- Menus are text only. The selected option has a 1.5 px ink underline that slides (`--bounce`); hovering an option gives a grey (`--card2`) fill.
- Page-level switches use a soft grey fill instead of a black pill.
- Only trigs keep borders. Rests are pale `--card2` tiles with no border.

## Trig looks (owner to pick; default Graphite)

| | Note pad | Tie | Playing |
|---|---|---|---|
| **Graphite** | `#3a3a38` fill, white text | `#e4e4e0` tile + `#b9b9b4` band | pad turns rust; rust tick under it |
| **Oxblood** | `#7c2e15` fill, white text | warm pale tile + `#d9a58e` band | pad turns `#d9632e` |
| **LED** | white, `#5a5a58` border | pale tile + grey band | rust border; the LED under the pad lights rust (dim red LED = step has a note) |

Rust stays the live colour (playhead, sounding step, running status) in every look.
Oxblood bends the "rust = live only" rule on purpose so the owner can judge it.

## Cues on a trig (subtle by design)

| holds | cue |
|---|---|
| velocity | faint hairline at the bottom; length ∝ velocity |
| gate length < 1 | four tiny ticks at top-left, the active ones darker |
| Mod 1 / Mod 2 | thin olive / ochre bar on the left edge, height ∝ amount (echoes the knob's modulation arc; CV port-hint colour) |
| condition | dashed border + tiny label at top-right (`50%`, `1:4`, `1st`) |
| tie | soft band from the note through the tied pads, across the gaps |
| changed since load | small hollow dot at top-left |
| skipped this pass (condition false) | pad dims to 28 % with the name struck through; LED stays off |

## Gestures

| where | gesture | result |
|---|---|---|
| pad (Trig or Note lane) | click / tap | select only |
| selected pad | click again | rest ⇄ note (new notes use the last pitch used on that track) |
| note pad | press, drag up/down | pitch by scale step, about 14 px per step; **Shift** = octaves |
| note pad | mouse wheel | pitch by scale step; **Shift** = octaves |
| note pad | press, drag right onto later pads | tie across them |
| value lane | click / drag across pads | paint the value (height = value) |
| Condition lane | click a note pad | condition picker |
| roll | click empty cell / drag right | add note / extend as ties |
| roll note | drag up/down · drag right edge · double-click or right-click | pitch · length · delete |
| inspector numbers | drag sideways · click to type · ↑ ↓ | exact values |
| keys | Space, ← →, ↑ ↓ (Shift = octave), A–K piano row, Z/X octave, `-` tie, ⌫ rest, 1/2 track, Ctrl+Z | play/stop, select, pitch, enter notes, edit |

Second-click toggling replaces the round-1 "tap toggles, hold selects" rule, at the owner's request.

## Phone / narrow layout (< 760 px)

The web-app view was cropped, so narrow screens get a vertical layout:

- Port rails are hidden.
- Each bar becomes **two rows of 8**; pad size is computed from the screen width (41 px on a 390 px phone).
- Track tabs shrink to `T1 32 st`; the lane tabs scroll sideways.
- The roll scrolls inside its own box. Touch density is chosen automatically on coarse pointers.
- No page-level horizontal overflow at 390 px (checked in Chromium on an iPhone 13 profile).

41 px is under the 44 px touch target; the 3 px gaps help, but on-device checking
is still needed. Rows of 4 would meet 44 px at the cost of twice the scrolling.

## Fit with the core build: what needs a decision

| item | in the mockup | plan status | who decides |
|---|---|---|---|
| per-step data | Rest/Note/Tie, key, velocity, Mod 1/2 | S1 | — |
| gate length ¼ ½ ¾ 1, bar copy/clear | shown | S1b extras | — |
| editing while playing | edits are a **draft**; "Apply at loop end" = queued stop → load ACK → start at step 1; Play applies a dirty draft first | S1 allows apply-on-Start; editing during playback is UI-only, nothing reaches the engine until Apply | Compass (confirm), Keel (ACK path) |
| live editing | not shown (owner: wait for S2) | S2 | — |
| per-track length 8/16/32/64 | default 16; mixed lengths loop independently; the global loop is the longest track | S1 is fixed 32; lengths are S4 | Compass (phase), Keel (master-cycle rule) |
| trig conditions | chance %, A:B every N loops, first / not first | **not planned**; needs a schema field | Compass (scope), Keel (format, T2) |
| scale / root | UI helper; steps store plain MIDI keys | none | could live in patch `ui` extras |
| chords on steps | roll reserved for it | not planned | later |
| playhead | demo timer | must be engine telemetry | — |

## Checks run

- Cloud Chromium (Playwright): 1280×800, 1440×900, 1920×1080 and iPhone 13. No console errors and no root overflow.
- Scripted checks:
  - First click selects and the second adds a note.
  - Drag-up pitch (C5 → G5), Shift-drag octave and wheel pitch.
  - Drag-right tie, condition picker, apply-at-loop-end.
  - Steps 8/16/32/64 in both views.
- Not run: physical Pi display, real touch hardware, engine integration. Screenshots were kept out of the repo.

## Chords (UI for #514, G4)

File: [`ui/groove-r3-chords.html`](../ui/groove-r3-chords.html). It is `groove-r2-final.html` plus chords and a more visible scale.
Data: #514 gives a note step up to 5 keys (the reserved `keys` field). The mockup keeps `key` as the
**root** and adds `keys` only for 2–5 notes, so a single-note step is saved exactly as today.

**Inspector: `Notes  ○ Single  ○ Chord [Am ▾]  ◁ ▷`**
- **Single**, not "Note": the Trig control already says Rest / *Note* / Tie, and a chord is notes too.
- Choosing **Chord** on a single note builds the **scale chord** on that root and opens the picker.
  Inversion ◁ ▷ moves the lowest note up or the highest down an octave.
- **Picker** (popover). Names are spelled on the step's root; types that leave the scale are shown in grey.

  | Group | Options |
  |---|---|
  | In <scale> | the scale triad and scale 7th (e.g. Am, Am7 in A minor) |
  | Triads | maj, m, °, +, sus2, sus4, (5) |
  | Sevenths | maj7, m7, 7, m7♭5, 6, m6 |
  | Extended | add9, m9 |
  | One note | back to Single |

- The Note field becomes **Root** when the step holds a chord.

**Pads (small, so the cue is a symbol, not the notes)**
- The pad shows the **chord name** (`Am`, `Cmaj7`, `G`) in semibold.
- If the keys match no known chord, the pad shows root + count (`A4+2`).
- On pads the slash bass is dropped (`Cm7/E♭` shows as `Cm7`); the inspector and roll show the full name.
- A tiny **ladder** on the right edge marks each key's height within two octaves: a close chord reads tall, an open voicing reads spread.
- The other cues (velocity, length, mods, condition, tie) are unchanged.
- Phone pads use 10.5 px names; `Cmaj7` fits a 41 px pad.

**Moving a chord**
- Drag up/down, wheel or ↑↓ moves the whole chord.
- A **scale chord moves diatonically**: Am → B° → C in A minor, so it stays in key.
- A typed chord type (e.g. maj7) moves in parallel. **Shift** moves by octaves.

**Entering chords without an on-screen keyboard**
- Select a step and **hold several keys together**. The computer row A–K works in the mockup; the app keyboard and
  MIDI in would feed the same function.
- The held notes become the step's chord (up to 5), and the selection advances when all keys are released.
- One key alone enters a single note, as before.
- Real MIDI devices stay owner-run (devices-off rule).

**Roll (manual voicing)**
- Clicking an empty cell in a column that already has a note **adds that key to the step** (up to 5).
- Each key can be dragged up/down on its own; double-click or right-click deletes one key; the step's last key deletes the note.
- The chord name (and condition) sits above the top key. The keys share the step's start, gate length and ties: per-key
  start/length isn't in #514's model, so it isn't drawn.

**Scale (UI helper, more visible)**
- `Scale [A] [minor]` now sits in the transport row, next to Tempo.
- It drives pitch drag and wheel steps, the picker's "In scale" chords and grey marks, diatonic chord moves, and the roll's
  **Scale notes only** toggle (on by default). With it on, the roll shows only scale rows; off-scale keys still show as dashed tags.
- Still UI-only: steps store plain MIDI keys. Saving the scale in the patch `ui` extras is the proposal.

**For the G4b builder / reviewers**
- Use `keys` only for 2–5 notes. Velocity, gate, mods, condition and ties are per step and apply to every key.
- Chord naming is a pure function of the keys and root: no stored chord type is needed. The mockup's `q` (which
  picker entry made it) is a UI hint for diatonic moves; store it in `ui` extras or drop it.
- Pads-view multi-note entry is UI-only work, separate from G4b's Roll entry; it needs no extra engine field.

## Per-track speed (owner request, 2026-11-10; UI only)

- `Speed 1× ½ ¼ ⅛` sits next to `Steps` for the selected track. A track tab shows `½×` / `¼×` / `⅛×` when slowed.
- A slowed track holds each step for 2/4/8 sixteenths, and its gate lengths scale with it. Example: 16 steps at ¼ = one chord
  per beat-bar over 4 bars, all in one bar row.
- The loop end used by Stop / Apply at loop end is the longest track's `length × divisor`, still at most 64 sixteenths ×8.
- **Not in the build:** per-track speed is plan S4 ("per-track speed" is listed as out of v1). It needs a
  per-track `div` field and a master-cycle rule from Keel; values here are proposals (1, ½, ¼, ⅛; no 2×).

## Research sources

- Westlicht PER|FORMER manual: https://westlicht.github.io/performer/manual. The session network blocked it, so details came from search excerpts.
- Elektron p-locks and trig conditions (Elektronauts): https://www.elektronauts.com/t/p-locks-vs-trig-conditions/42492
- Digitakt review: https://de.ra.co/reviews/21197
- Hermod+ / Metropolix user discussion: https://reddit.birdcat.cafe/r/modular/comments/1j0g8q3/hermod_w_metropolix_overkill

Signed: Claude (Groove UI designer), 2026-11-10 (chords section added the same day)
