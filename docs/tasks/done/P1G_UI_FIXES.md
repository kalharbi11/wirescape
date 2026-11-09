<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1G (#234): UI fixes from the owner's Pi notes (Phase 1, independent)

The owner tried the UI on the Pi (2026-11-07). Overall: "the smoothness and everything looks great". Fix these five things. Don't restyle anything else.

**Where:** `apps/studio/` once P1A has merged; before that, `kalharbi11/wirescape-ui` (P1A then imports the result). Follow the UI repo's `docs/DESIGN.md`, `docs/PERFORMANCE.md` and `docs/DECISIONS.md`, and record each design change in `DECISIONS.md`.

**Read first:** `src/panels/widgets/canvasBase.ts`, `WavePreview.ts` (the LFO card), `EnvelopeCurve.ts`, `Scope.ts`, `src/theme/tokens.css`, `src/system/` (Settings), the `midi.in` panel (`public/panels/midi.in.json` and the generic panel builder in `src/panels/layouts.ts`).

1. **Sharp lines in the ADSR, LFO and other canvas displays.** The owner sees them as pixelated. Likely cause: `CanvasPane.resize()` sizes the backing store from `offsetWidth × devicePixelRatio`, ignoring the patch-canvas zoom (a CSS transform), so the bitmap is upscaled whenever zoom is above 100%. Fix: scale the backing store by `dpr × canvasZoom`, capped by the Pi profile budget in `PERFORMANCE.md`; re-render on zoom change (debounced, not per frame); and keep the cached static layers in sync. Verify at 50/100/185% zoom with `deviceScaleFactor: 1` and `2`.
2. **Slightly grey canvas background** so modules pop: a very light grey, not white. Change the canvas background token only. Keep contrast rules and the side-rail and cable colours. Screenshot before and after.
3. **LFO card: a moving wave instead of the dot.** Replace the phase dot with a scrolling oscilloscope-style trace of the LFO output: the most recent ~1–2 cycles drawn left to right and moving, in the existing waveform style. Use the telemetry `phase` (or `outputs` when present) and the shape/rate params to draw the trace; when telemetry is unavailable, show a static waveform, not fake motion. Keep it inside the existing Pi frame budget (one cached path, redrawn at the ticker rate).
4. **Bug: the dot doesn't travel the whole cycle at any rate.** Find the cause before replacing the dot. Check the FakeEngine phase, the phase → x mapping in `WavePreview.ts` and Ticker sampling. The new trace must cover the full cycle; add a test that samples the drawn head position across one full cycle at slow and fast rates.
5. **MIDI device choice as menus.**
   - **MIDI In module:** a dropdown on the panel for `config.endpoint`, with "All enabled devices" plus each MIDI device from `listMidiDevices()`. It's stored in the module's config (existing field; no ID change). An unknown saved endpoint stays selected and is shown as "(not connected)".
   - **Settings → MIDI:** the same style of menu as the audio device selector, listing devices from `listMidiDevices()` with an enable toggle per device. It must show whatever the engine reports: FakeEngine's list now, the real M8 once P1C lands.

**Tests:** Playwright for each item: canvas backing-store size at zoom, the background token value, LFO trace motion and full-cycle coverage, the MIDI In dropdown (choose, save, reload), Settings MIDI menu enable/disable round-trip. Existing tests stay green; update screenshots only where these changes require it.

```acceptance
tier: T1
size: <= 700 changed lines, <= 14 files
must_pass:
  - ci: web job green (or wirescape-ui's browser check before P1A)
stops: [module/param/port ID change, test removal, new dependency, restyling beyond the five items]
```
