<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio / Web / Wirescape-UI parity review — 2026-11-07

**Scope.** Standalone Electron Studio (`apps/desktop` wrapping `apps/studio`), deployed WebAssembly + AudioWorklet Studio, and the reference source `kalharbi11/Wirescape-UI` (`ui/wirescape-ui-kit.html`, `docs/DESIGN.md`, `docs/PERFORMANCE.md`). Baseline `main` at `092576c`, before issue #347.

**Evidence standard.** Reviewed both GitHub repositories' source trees, the kit-to-Studio widget map in `apps/studio/AGENTS.md`, theme/CSS, UI widgets, both telemetry producers, current browser/Electron tests, and the published demo URL. Added Chromium phone emulation tests with screenshot attachments. **No running Electron instance, real iPhone, Pi screen, or interactive browsing/screenshot of the published site was available in this session.** Static code inspection is not a substitute for those tests.

## Template parity

| Kit requirement | Studio / Electron | Web status | Review |
|---|---|---|---|
| White/graphite/rust colors, Plex type, card/port style | Imported theme/cards/widgets | Shared renderer | Recompare at the same zoom and DPR |
| Large/medium/small knobs, numeric, fader, segmented, toggle | Implemented `src/panels/widgets` | Shared | Check dial geometry and finger hit targets |
| Wave/LFO, filter response, ADSR dot, meters | Display widgets implemented | Shared widgets | **P0: underlying engines omit most dynamic telemetry** |
| Modulation nodes, cable sag and live state, header info | Implemented in canvas/infobox | Shared | Test end-to-end touch mapping |
| Full kit extra controls (slider, detent, tape, XY) | Deliberately not built: no current parameter role requires them | Same | **P2**, only if needed for an actual module |
| Motion budgets | One rAF Ticker, culling, dirty canvas and 30 Hz Pi profile | Same | Preserve truthful motion, throttle only redraw |
| iPhone active patch name, drawer dismiss, touch hits | Lacked defaults and interaction | Same source | **Fixed in #347**, awaits browser/phone verification |
| Full set of modules/hardware device controls | Native Electron/engine | WASM demo filters MIDI, omits hardware MIDI/OSC/device choice | Expected product-scope difference; disclose honestly |
| Dark theme | Explicitly unfinished | Same | **P2**, not visually certified |

## Why many animations are absent

**This is not solely a slow-web-app problem.** `src/panels/widgets/WavePreview.ts` needs `Telemetry.phase` and says "phase not available" if missing; `EnvelopeCurve.ts` needs `envelope`; `Knob.ts` and `FilterDisplay.ts` need effective post-modulation values; port/cable activity uses event and signal samples.

The standalone's `native/sidecar/session.cpp::Session::telemetry()` currently creates empty `effective`, `phase`, `envelope`, `scope` and `eventActivity` objects. The WebAssembly `native/web/web_engine.cpp::wsw_telemetry()` emits only `meters`, `scope`, `outputs`, and `heldNotes`. Before this PR, `src/web/WebBridge.ts` also erased any additional telemetry provided by the worklet; it now forwards those future fields.

**Still missing — P0 engine task:** real phase, envelope, effective post-modulation values and event activity need bounded, audio-safe native/wasm collection and 15–30 Hz UI transfer. Add native+wasm conformance tests. Do not fabricate activity from a CSS animation or pretend a signal exists. Keep the one-rAF redraw path, cull invisible modules, throttle visual canvases to 30 Hz (not DSP/audio) where needed. The kit's measured filter graph (~0.22ms/frame) and LFO (~0.13ms/frame) suggest performance optimization before removal of core visuals; actual phone benchmarks remain necessary.

## Changes implemented in issue #347

1. Phone patch title retained in the top bar with ellipsis instead of hidden under 760px.
2. Mobile library gets outside-tap dismiss backdrop, semantic expanded state, close on adding a module, and resize cleanup.
3. Coarse-pointer new sessions use Touch hit-area profile; a stored user choice takes precedence.
4. Remove false "NO AUDIO ENGINE" library copy; distinguish WebAssembly AudioWorklet from native bridge in About.
5. Browser start copy no longer calls mobile browsers unsupported; `AudioContext.resume()` is **requested during Start tap**, rather than after async WASM/worklet setup.
6. WebBridge passes `phase`, `effective`, `envelope`, `eventActivity` through when actually emitted by the engine. The engine itself is unchanged.
7. Chromium mobile UI tests attach screenshots and assert navigation, geometry, touch preference; web tests assert audio-start call ordering and non-erasure of supplied telemetry.

## Prioritized follow-ups not done here

**P0** — restore authentic signal telemetry in both engines, then compare running LFO phase, envelope, filter modulation and event-pulse visuals against the reference kit. Treat as an engine/realtime/protocol review, not a CSS-only task.

**P1** — real iOS Safari portrait/landscape with expanded/collapsed browser bars; audio start/retry and background resumption; two-finger pinch from background/header/knob/port; add/move/connect/modify/remove modules; open/save/download/share; on-screen keyboard press/release and page scroll; long patch names; narrow 320px layouts; voice-over labels; safe areas. Audit right-click-only mapping actions and devise a coherent long-press/touch action path. Ensure any unsupported MIDI/OSC controls are hidden or explicitly unavailable.

**P1** — inspect Playwright screenshots of full studio, LFO, ENV, VCF, MIX, output and information popovers side-by-side with the original kit at matched zoom/DPR. For performance, instrument a dense 20-module/40-cable patch and measure phone frame time, canvas invalidation and battery; simplify only expensive drawn detail (e.g., graph sampling/30 Hz cap), not truthful response.

**P2** — formalize Calm mode vs current Motion boolean/Pi profile, finish dark theme, polish accessible focus and small inputs, and import slider/detent/tape/XY only if catalogue roles require them.

## Evidence and independent-review gate

- `apps/studio/tests/mobile-gestures.spec.ts`: Chromium 390x844 touch emulation, patch name, menu/dismiss, module-add, target defaults; screenshots attached as `mobile-library-open.png`, `mobile-canvas.png`.
- `apps/studio/tests-web/web-demo.spec.ts`: Chromium 390x844 DPR3, web startup/keyboard/toolbar, overflow geometry, `resume` before WASM fetch, no telemetry erasure; screenshots `web-iphone-start.png`, `web-iphone-canvas-and-keys.png`.
- Existing tests cover simulated two-finger pinch, wasm real-sound peak, file/share and keyboard. **Playwright screenshot attachments are not automatically persisted after successful CI** because current workflow uploads `test-results/` only on failure.
- Reviewer: run Studio typecheck/build/Playwright, Electron shell under Xvfb, `web-demo` engine/site/live jobs as relevant, and `git diff --check`. Confirm no engine/ABI/protocol/dependency/protected-workflow changes. Give one independent code review.
- Physical **iPhone Safari**, **Electron desktop/Pi/Windows** and real-device audio require an owner/device-capable agent; Chromium emulation **must not** be recorded as completing those gates.

**Status:** source audit and focused UI patch for independent review. No physical-device acceptance claim.

Signed: ChatGPT (GPT-6, UI audit and builder), 2026-11-07
