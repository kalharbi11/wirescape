<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Web demo engine (`native/web`)

A standalone CMake project that compiles only Qt-free, device-free sources:
`engine2`, `control/app_engine.cpp` (builtin catalogue), `document/runtime_patch.cpp`,
the ABI1 `src/engine.cpp` that AppEngine links, the voice allocator and the
pinned Plaits sources used by the Macro Oscillator. No PortAudio, libremidi,
threads, files or sockets.

- **Native build** produces `web_engine_tests` (CI, device-free).
- **Emscripten build** (`emcmake`, pinned `EMSDK_VERSION` in the workflow) produces
  a standalone `wirescape.wasm` (`-sSTANDALONE_WASM --no-entry`, native wasm
  exceptions). There is no Emscripten JS glue: `apps/studio/public/web/ws-processor.js`
  instantiates it inside the AudioWorklet with a few WASI stubs (clock, random,
  discarded stdout).

**Threading.** Commands (`wsw_command`) and renders (`wsw_render`) run on the
worklet thread, between render quanta. There is no cross-thread state.

**Not the product's realtime path.** A graph commit parses JSON, builds the
candidate with `load_runtime_patch` and crossfades over 10 ms from the old graph,
all on the worklet thread. That is acceptable for a demo and is why this code
lives outside `native/sidecar`. The sidecar's D5 and live-replan paths stay the
reference. `web_engine.cpp::execution` mirrors `Session::execution`. Keep them in
step when the protocol changes; the shared Studio conformance work (#250) is the
long-term fix for drift.

**Scope.** `midi.*` modules are hidden from the catalogue and refused as
`unknown-module`. Device and OSC commands are answered by the TS bridge
(empty device lists, ignored selections).

**Third-party.** The same pins as the native build: nlohmann/json `9cca280`
(MIT) and pichenettes eurorack `f4cfa76` / stmlib `e3bd7c9` (MIT), with the
macro_oscillator patches applied. The build tool is Emscripten (MIT/UIUC), and
nothing from it is vendored.
