<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1B: `wirescape-host`, a local C++ host for the web UI (Phase 1, after P1.0)

A Qt-free C++17 executable that owns the engine, audio and MIDI, serves the built UI, and speaks `docs/specs/HOST_PROTOCOL` (`.md`, written in P1.0) v0. Pi (Linux ARM64) first; it must also build on Windows, though Windows launch polish is Phase 4.

**Read first:** `docs/specs/HOST_PROTOCOL` (`.md`, written in P1.0), `docs/PLAN.md` D1/D3–D9/D12, `native/cli/play.cpp` (engine + catalogue + MIDI + PortAudio lifecycle to reuse), `native/control/app_engine.h`, `native/document/runtime_patch.h`, `native/control/midi_host.h`, `native/io/audio/portaudio_service.*`.

**Build:**
- `native/host/` with `main.cpp`, `server.*` (civetweb), `session.*` (protocol), `graph.*` (document → execution view → AppEngine) and `DESIGN.md` (pinned civetweb revision, licence and threading diagram). civetweb is pinned per D12 (vendored `civetweb.c/.h` + licence, or FetchContent with `URL_HASH`). Use only its HTTP, static-file and WebSocket features; no CGI, Lua or SSL.
- **Threads:** the civetweb worker threads parse JSON and hand commands to **one control thread** through a bounded queue. Only the control thread touches AppEngine's non-realtime API. The audio callback stays exactly as in `portaudio_service`. Status and telemetry are built on the control thread from engine atomics and meters at 10/30 Hz.
- **Startup (D6):** `wirescape-host --ui <dist> [--patch <file>] [--port 7400] [--confirm-audio] [--confirm-midi] [--device <name|index>] [--block 256]`. It prints the URL with the token. Without `--confirm-audio` it renders to a null sink paced by a steady clock. That's the test and CI mode, and it opens no device. Settings (last device, last patch) go in `user_data/host.json`.
- **Edits (D5 fallback):** `setParam`/notes/panic go through the event queue. Topology edits, `setModAmount` and `loadGraph` build a candidate engine off the audio thread, then stop → swap → start with a ≤10 ms fade-out and fade-in, targeting a gap of ≤150 ms. Keep the swap behind one function so 2A can replace it with live replan. A refused candidate leaves the running engine untouched.
- MIDI follows the existing `midi_host` (the `devices[]` restore from the patch, plus `setMidiEnabled`), with the stop/start around a swap kept on the control thread.

**Tests (CTest `host_protocol_tests`, null sink, localhost only):** a civetweb client covers the hello/catalogue, the token and Origin refusals, every P1.0 test vector, refusal leaves the graph unchanged, a swap during notes (no crash, gen increments), 1000 rapid `setParam` (no allocation in the audio path; run the existing allocation counter if available), oversize message → `limit`, and a reconnect re-load. A TSan job covers the host tests on CI.

```acceptance
tier: T2
size: <= 1800 changed lines, <= 16 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R host_ --output-on-failure --no-tests=error
  - ci: linux-arm64, windows-x64 (build only for the host), sanitizers incl. TSan on host_protocol_tests
  - pi: build + ctest -R host_ (null sink)
stops: [ABI header edit, LAN bind, new dependency other than civetweb, audio callback changes]
```
