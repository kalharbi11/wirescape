<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-web (#308): browser demo (WebAssembly engine + Studio, static site)

Owner request (2026-11-07): a link professors and friends can open to patch and
hear Wirescape without installing anything. Plan decision **D14**.

**Shape.** The real engine (`engine2`, the AppEngine builtin catalogue and
`load_runtime_patch`) compiled to a standalone `wirescape.wasm`, run inside an
AudioWorklet. `apps/studio/src/web/WebBridge.ts` implements the same
five-member bridge as Electron's preload and answers protocol v0, so
`RealEngine` and the whole UI run unchanged. Built with `VITE_WIRESCAPE_WEB=1`,
deployed by `.github/workflows/web-demo.yml` to GitHub Pages.

**In:** every non-MIDI module, the built-in starters (preset menu), on-screen
and computer keyboard, live patching, browser autosave.
**Out:** MIDI, OSC, audio-device choice, Open/Save files, telemetry beyond the
output meter and held notes. The product stays the Electron app (D1); this is a
showcase build and changes no engine, protocol, ABI or patch format.

**Read first:** `native/web/DESIGN.md`, `native/sidecar/session.cpp`
(`execution`/`reply`, which `native/web/web_engine.cpp` mirrors),
`apps/studio/src/engine/RealEngine.ts`.

**Follow-ups (Codex-sized, after W-web-1 merges):**
- W-web-2: hide `qt-midi` and other MIDI starters and the MIDI library entries in web mode (filter by catalogue).
- W-web-3: telemetry parity (effective values, scope, envelope, phase) from the wasm engine.
- W-web-4: "Copy link" — a patch encoded in the URL hash, loaded on start.
- W-web-5: optional "Download patch" (savePatch → Blob download) once the owner wants files.

```acceptance
tier: T1
size: <= 1300 changed lines, <= 20 files (includes STATE.md and PLAN.md planner updates)
must_pass:
  - ci: web-demo engine job green (native web_engine_tests + wirescape.wasm build)
  - ci: web-demo site job green (tests-web smoke: start, starter loads, audible note, MIDI refused)
  - ci: web job green (Electron/FakeEngine suites unchanged)
  - bash -c "git diff --check"
stops: [engine or protocol semantics change, ABI header edit, new dependency beyond Emscripten (D14)]
```
