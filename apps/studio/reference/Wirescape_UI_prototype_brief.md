<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Wirescape UI prototype brief

Nov 4, 2026 · @khalid

## Goal and rules

Build the complete Wirescape interface in the browser with a fake engine behind it. Every button, gesture, animation and value display works for real; only the sound is missing. The code team then replaces the fake engine with the real one, without redesigning anything.

**Inputs to give the agent**

- This brief.
- `WIRESCAPE_Studio_Final.html`: the visual language (paper, graphite, rust accent), cable curves, knob style, LFO motion.
- `wirescape_modules.json`: the real module list. 21 modules with their real port and parameter IDs, ranges, units and mode labels.

**Rules for the agent**

1. **Modules come from data, never hand-coded.** Every module card is drawn from `wirescape_modules.json`. A new module added to that file should appear with a working panel and no code changes. Custom layouts are optional overrides on top.
2. **One fake engine, behind one interface.** All the UI's talk to "the engine" goes through one object (`engine`) with a small set of methods and events (see "Structure"). Nothing else in the UI may fake sound or values.
3. **Real values drive every animation.** A knob that moves with an LFO, a meter, a phase dot: each one draws a number the fake engine sends. No CSS animation pretends to be a signal.
4. **The patch file is the real format.** Save and load use the Wirescape patch v2 shape (see "Structure"), so patches made in the prototype open in the real app.
5. **Built for the Pi first.** Follow "Pi performance rules"; the laptop gets it for free.
6. **Test hooks on everything.** Each interactive element has a `data-testid`, and a scripted test can drive the whole app (see "Done checklist").

## How to work with the agent

Use Claude Code (web) on its own small repo or folder, so it can run the app, take screenshots and run tests itself. Build in six phases, and review each one before the next starts.

**Setup:** a Vite + TypeScript project. It's plain HTML/CSS underneath; TypeScript just catches mistakes. The folder later becomes `apps/studio` in the new Wirescape repo.

| Phase | What gets built | You review |
| --- | --- | --- |
| 1. Rebuild | The sketch rebuilt as components with the same look. The module library and cards come from `wirescape_modules.json`. The fake engine skeleton | Does it look and feel like the sketch? |
| 2. Patching | Every cable gesture, including cables onto knobs for modulation ("Cables" below) | Patch a small synth by hand |
| 3. Live values | The fake engine runs: knobs move with modulation, value rings, meters, scope, LFO phase, note activity | Does the motion feel right and readable? |
| 4. Editing | Undo/redo, copy/paste, multi-select, save/load, starter patches, autosave | Break it on purpose |
| 5. System | Audio, MIDI and OSC status and settings, CPU, panic, preferences, the Pi performance mode | Run it on the Pi in Chromium |
| 6. Signature panels | Custom panels for CHAOS-lite, Step Sequencer, Macro Oscillator, Mixer/Output. A final polish pass | Full session on the Pi |

**Each phase ends with** screenshots, a short screen recording of the main gestures, and the test run passing, all done by the agent. You open it on the laptop and on the Pi, then reply with changes.

**Keep a `DESIGN.md` in the project.** The agent records every design decision it makes (sizes, colours, gesture rules, animation timings), so the code team doesn't have to rediscover them.

**When the agent wants to invent something big** (a new panel type, a new gesture), it proposes it in a sentence or a quick mock first. It doesn't just build it.

## Structure the agent must follow

Three contracts keep the prototype swappable: the module data, the engine interface and the patch file. Everything else is the agent's choice, written down in `DESIGN.md`.

**Folders**

- `src/patch/`: the patch document, undo/redo history and save/load. No DOM code.
- `src/canvas/`: pan/zoom, module cards, cables, selection.
- `src/panels/`: the generic panel builder, control widgets (knob, toggle, selector, readout, meter, scope) and optional custom panels.
- `src/engine/`: `FakeEngine`, which implements `EngineClient`. The only place values are invented.
- `src/system/`: audio, MIDI and OSC status, settings and toasts.
- `src/theme/`: design tokens (the sketch's CSS variables), with a normal and a Pi profile.

**Module data (`wirescape_modules.json`)**

- Each module has an `id` (for example `essentials.vcf`), a `name`, a `category`, `ports` and `parameters`.
- A port has an `id`, a `direction` (input or output), a `kind` and a `hint`. `kind` is `signal` (an audio-rate cable) or `event` (notes or MIDI messages). `hint` is `audio`, `pitch`, `cv_uni`, `cv_bi`, `gate`, `trigger`, `ev_notes` or `ev_midi`. Hints set colour and icon only, **they never block a connection**. Only signal-to-event connections are refused.
- A parameter has an `id`, `name`, `minimum`/`maximum`/`default`, `unit`, `taper` (linear or log: knob travel follows it), `step` (non-zero means stepped), `decimals`, a `modulatable` flag and, sometimes, `choices` (mode labels: show these as segmented buttons).
- Optional `panels/<module-id>.json` overrides add sections, hero knobs, short labels and widget types. A missing override falls back to the generic layout.

**Engine interface** (the code team implements the same thing for real)

```typescript
interface EngineClient {
  // the patch
  loadGraph(patch: PatchV2): Promise<void>;
  addModule(id: number, type: string, params?: Record<string, number>): void;
  removeModule(id: number): void;
  connect(c: Connection): void;
  disconnect(c: Connection): void;
  setParam(moduleId: number, paramId: string, value: number): void;
  setModAmount(c: Connection, amount: number): void;
  // playing
  noteOn(note: number, velocity: number): void;
  noteOff(note: number): void;
  panic(): void;
  // live values pushed ~30 times a second
  onTelemetry(cb: (t: Telemetry) => void): void;   // effective param values, meters, LFO phase, scope buffers
  onStatus(cb: (s: SystemStatus) => void): void;   // audio device, sample rate, buffer, CPU, dropouts, MIDI devices + activity
  // devices
  listAudioDevices(): Promise<Device[]>;  selectAudioDevice(id: string): void;
  listMidiDevices(): Promise<Device[]>;   setMidiEnabled(id: string, on: boolean): void;
}
```

The UI never waits on the engine to update its own screen. It draws the change at once, sends it, and shows an error if the engine refuses.

**Patch file (patch v2):** modules with numeric instance IDs and string parameter IDs, connections with `kind` `signal`, `modulation` (into a parameter, with an `amount` from -1 to 1) or `event`. Positions and UI state go in a `ui` block. Unknown fields must survive load → save.

```json
{
  "format": "wirescape.native.patch", "version": 2,
  "modules": [
    { "id": 1, "module": "essentials.lfo", "parameters": { "rate": 2 } },
    { "id": 2, "module": "essentials.vcf", "parameters": { "cutoff": 800 } }
  ],
  "connections": [
    { "kind": "modulation", "from": { "module": 1, "port": "out" }, "to": { "module": 2, "port": "cutoff" }, "amount": 0.4 }
  ],
  "ui": { "positions": { "1": [40, 100], "2": [620, 80] }, "view": { "x": 0, "y": 0, "zoom": 1 } }
}
```

The port and parameter names in this example are illustrative. The real ones are in `wirescape_modules.json`.

## Feature list

This is everything the app will have. Items marked *(later)* can be designed now but built last. Tick them off as the prototype gains them.

**Canvas**

- [ ] Pan by dragging empty space; zoom with wheel, trackpad pinch and +/− buttons, centred on the pointer; "fit all"
- [ ] Zoom range about 25–185%; cables and hit areas stay accurate at every zoom
- [ ] Box-select by dragging on empty space with a modifier; shift-click adds to the selection
- [ ] Empty-canvas state: a quiet hint to add a first module or open a starter patch
- [ ] Shortcut overlay (`?` key)
- [ ] Touch: one-finger pan, two-finger zoom, long-press for the context menu *(later; depends on a Pi touchscreen)*

**Modules**

- [ ] Library sidebar grouped by category, with search
- [ ] Add by click (lands in view), by drag-and-drop to a spot, or by double-clicking the canvas to open a quick-add search
- [ ] Move, delete, duplicate, copy/paste (also between patches), move a selected group
- [ ] Instance label ("Bass VCO") separate from the module type
- [ ] Generic panel from data: header, inputs left, outputs right, sections of controls
- [ ] Poly badge showing the voice count when a module is polyphonic
- [ ] Missing-module card: an unknown module type shows a grey card with its saved ports and values, and is never deleted
- [ ] Compact or collapsed card view *(later)*

**Cables**

- [ ] Drag output → input or input → output; compatible ports highlight while dragging
- [ ] Drop on empty canvas → quick-add menu filtered to modules with a matching port
- [ ] Grab a cable's end at an input to move it or pull it off
- [ ] Select a cable and press Delete; hover shows "from → to"
- [ ] Colour and style by hint (audio, pitch, CV, gate/trigger, events); thicker line for poly cables
- [ ] Stereo pairs: dragging L onto a stereo input offers to connect L and R together, as two cables drawn as a pair
- [ ] Feedback cables (a loop back upstream) get a small marker
- [ ] **Cable onto a knob = modulation.** Only modulatable knobs light up as targets. The amount (-1 to 1) is set by dragging a handle on the knob's ring or on the cable. Several modulations on one knob show as separate arcs
- [ ] Open question for the code team: when a signal input already has a cable, does a new one replace it or add to it? Default to replace; shift-drop adds

**Controls**

- [ ] Knob: vertical drag, shift for fine, wheel, arrow keys, double-click resets to default, click the readout to type a value
- [ ] Travel follows the taper (log for frequency); stepped parameters snap
- [ ] Value formatting by unit: Hz/kHz, ms/s, dB, %, semitones. Tidy bounds ("0.01 Hz–20 kHz", never long decimals)
- [ ] **Value ring on the knob border** showing the set value
- [ ] **Modulation arc** showing the range the modulation sweeps, plus a moving marker for the live value, so the cutoff knob visibly moves at the LFO's rate
- [ ] `choices` parameters as segmented buttons; on/off parameters as toggles
- [ ] Tooltip: name, value, range, and "modulated by …"
- [ ] MIDI learn: right-click → Learn, move a hardware control, mapped *(later)*

**Live displays**

- [ ] LFO card: waveform preview with a phase dot from real phase values
- [ ] Output meters, L/R, with peak hold and a clip mark
- [ ] Scope on Output; a mini scope when hovering any output port *(later)*
- [ ] Envelope card: the curve, with a marker showing where each voice is
- [ ] Activity dots on event ports when notes pass; held notes shown on the keyboard module
- [ ] Faint activity on cables carrying signal *(optional; only if cheap on the Pi)*

**Playing**

- [ ] Computer keyboard plays notes (two octaves; Z/X shift octave), sent through the engine interface
- [ ] Panic button that stops all notes
- [ ] On-screen keyboard strip *(later)*

**System bar**

- [ ] Audio status: starts by itself on launch. Shows device, sample rate, buffer and latency in ms, CPU %, and a dropout count. States: starting, running, no device (with a fallback), device lost then reconnecting
- [ ] MIDI: all devices on by default, an activity dot per device, toasts when one is plugged or unplugged, per-device on/off
- [ ] OSC: on/off, port, "allow network" switch (off by default), a small recent-messages monitor *(later)*
- [ ] Clock: BPM and run/stop when a Clock module exists

**Patches**

- [ ] New, open, save, save as, rename; unsaved-changes dot
- [ ] Recent patches; a starter-patch library (about 6)
- [ ] Autosave and restore on launch
- [ ] Import/export file; unknown fields preserved

**Editing**

- [ ] Undo/redo for everything. One knob drag is one step; one multi-cable stereo connection is one step
- [ ] Select all, copy, paste, duplicate, delete

**Preferences**

- [ ] Motion on/off; performance profile Normal or Pi; UI scale
- [ ] Show/hide port labels; theme tokens in one file
- [ ] About: version and build

**Feedback and errors**

- [ ] Toasts for actions; a clear message when the engine refuses something (and the change rolls back)
- [ ] Dev-only debug panel: FPS counter; fake engine switches to simulate device unplug, CPU spike, dropouts and a refused edit

## Fake engine behaviour

The fake engine imitates the real one well enough that every display shows believable values. It makes no sound. It lives in `src/engine/FakeEngine.ts` and only talks through `EngineClient`.

- **Keeps its own copy of the graph** from `loadGraph`, `addModule` and `connect`, and refuses a few things the real one would: event ↔ signal cables, or an unknown module type when added.
- **Runs a simple 30 Hz clock** and computes:
  - LFO output and phase from `rate`, shape and depth;
  - envelopes per note, from attack, decay, sustain and release;
  - the effective value of every modulated parameter: base + amount × source, clamped to its range;
  - Output meters from a rough level estimate, plus a synthetic scope waveform.
- **Sends one telemetry message per tick** with only what changed: effective values, LFO phases, envelope positions, meters, scope samples and note activity.
- **Fakes system status:** two audio devices and two MIDI devices, a CPU figure that rises with module count, a latency from the buffer setting, and an "auto-started" running state at launch.
- **Debug switches** (dev panel) to simulate: device unplugged, MIDI device plugged in, a CPU spike, dropouts, and a refused edit.

The telemetry shape is what matters most. The real engine will send the same fields, so the code team only has to fill them with real numbers.

## Pi performance rules

On the Pi, the UI and the audio share the same CPU, so the UI must stay light. Target: 60 fps on the laptop, and at least 30 fps on the Pi with 20 modules and 40 cables while modulation is moving.

1. **Animate only transform and opacity.** Rotate knob needles and move phase dots with `transform`. Never animate width, top, left or shadows.
2. **One animation loop.** A single `requestAnimationFrame` loop applies the latest telemetry. No per-knob timers or CSS keyframe loops for signal-driven motion.
3. **Redraw cables only when something moves.** Recompute a cable's path when its module is dragged, or when panning or zooming; never every frame.
4. **Draw live displays on canvas.** Scopes and waveforms use a small `<canvas>`, not hundreds of SVG points.
5. **Skip what isn't visible.** No telemetry drawing for modules off-screen, and fewer updates when zoomed far out.
6. **Pi profile.** Lower telemetry drawing to 30 Hz, drop the module shadows, and simplify the cable activity effect. One switch in preferences, and automatic when the app detects a slow device.
7. **Measure it.** The dev panel shows FPS. Each phase reports the frame time for the demo patch on the Pi in Chromium.

## Done checklist and handover

The prototype is done when a scripted test can play through a whole session, and a person can do the same on the Pi without hitting a dead button.

- [ ] All 21 modules in `wirescape_modules.json` render with working controls, with no module-specific code in the generic panel
- [ ] A Playwright test builds a patch from scratch: Keyboard → VCO → VCF → VCA → Output, ADSR on the VCA, LFO modulating the VCF cutoff. It plays notes and checks that the cutoff knob's live marker moves. It saves, reloads, and checks the patch is identical
- [ ] The same test passes at 50%, 100% and 185% zoom
- [ ] Undo back to an empty canvas and redo to the full patch both work
- [ ] Loading a patch with an unknown module and unknown fields, then saving it, keeps them
- [ ] The FPS figures for the Pi are recorded in `DESIGN.md`
- [ ] `DESIGN.md` lists the design tokens, gesture rules, animation timings and every open question
- [ ] Every interactive element has a `data-testid`; `window.__wirescapeTest` exposes state, load and save

**What the code team gets:** the project folder, `DESIGN.md`, the test suite and the `EngineClient` interface. Their job is to write `RealEngine` with the same interface, first for the native host on the Pi, then for the browser test bench, and keep every UI test passing.

## Out of scope and open questions

**Not in the prototype:** real sound (no Web Audio beeps either, so nobody mistakes fake for real), real MIDI or OSC, file system access beyond browser download/upload, packaging, and any C++.

**Questions to settle with the agent early:**

- [ ] Pi screen: resolution, and is it a touchscreen or mouse and keyboard? This decides hit-area sizes and whether touch gestures are in phase 1
- [ ] Card sizes: does each module type keep its own size (as in the sketch), or follow a grid of fixed widths?
- [ ] Several cables into one signal input: replace or add? Confirm with the code team
- [ ] Signature panels: how far CHAOS-lite, Step Sequencer and Macro Oscillator depart from the generic layout
- [ ] Dark theme now or later?
