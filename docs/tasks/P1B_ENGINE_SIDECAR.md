<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1B (#230): `wirescape-engine`, the engine sidecar (Phase 1, after P1.0)

A Qt-free C++17 executable that owns the engine, audio and MIDI and speaks the stdio protocol in `docs/specs/` (the engine protocol from P1.0). The app (P1F) starts it. It must build and run on the Pi (Linux ARM64) and Windows.

**Read first:** the engine protocol spec, `docs/PLAN.md` D3–D9, `native/cli/play.cpp` (catalogue, `runtime_patch` load, MIDI host, PortAudio lifecycle and `--list-devices`/`--list-midi`: **reuse these, don't rewrite them**), `native/control/app_engine.h`, `native/control/midi_host.h`, `native/io/audio/portaudio_service.*`.

**Build:**
- `native/sidecar/` with `main.cpp`, `protocol.*` (line reader/writer, JSON via the existing nlohmann/json), `session.*` (commands → engine), `devices.*` (wraps the existing PortAudio and libremidi enumeration) and `DESIGN.md` (threads and limits). Extract shared bits of `play.cpp` into a small helper if needed; `play` keeps working unchanged.
- **Threads:** a reader thread parses lines into a bounded queue. **One control thread** owns AppEngine's non-realtime API, the MIDI host and device changes. A writer thread serialises replies and pushes. The audio callback stays exactly as in `portaudio_service`. Status (10 Hz) and telemetry (30 Hz) are built on the control thread from engine atomics and meters.
- **Startup (D6):** with `--confirm-audio`, audio starts on the remembered or given device right away; with `--confirm-midi`, the remembered MIDI devices are enabled. Without them it runs the `--null-audio` paced sink and fake MIDI (the test mode). If the remembered device is missing, it falls back to the default and reports that in `status`.
- **Edits (D5 fallback):** params, notes and panic go through the event queue. Topology, `setModAmount` and `loadGraph` build a candidate off the audio thread, then stop → swap → start with a ≤10 ms fade, gap ≤150 ms, all behind one function that 2A replaces with live replan. A refused candidate leaves the running engine untouched. The MIDI service stop/start around a swap stays on the control thread.
- **Device selection while running:** `selectAudioDevice` stops the stream, reopens it on the new device and restarts, with the graph kept. `setMidiEnabled` changes subscriptions on the control thread.

**Tests (CTest `sidecar_protocol_tests`, `--null-audio`, fake MIDI backend, no devices):** spawn the binary with pipes and cover hello/catalogue, every spec test vector, refusal leaves the graph unchanged, a swap during held notes (gen increments, no crash), 1000 rapid `setParam` (no audio-path allocation; use the existing allocation counter if present), oversize/malformed lines, stdin EOF → clean exit, and loading the three `class_demo_*` patches. TSan on these tests in CI.

**Size exception (workflow §5; engine-lane self-decision, 2026-11-06):**
The approved D5 master-gain ramp needs changes to internal `AppEngine` files
beyond the original 14-file sidecar budget. The maximum is increased by
exactly 50% (1,600→2,400 lines; 14→21 files), including tests and this task
file. This changes no runtime, performance or test tolerance and does not
waive any acceptance command. Reviewed diff size still must pass the gate.

**Owner check (not in must_pass):** `build/native/wirescape_engine --confirm-audio --confirm-midi --device "<CM477 name>"`, then paste `{"v":0,"id":1,"type":"listMidiDevices"}`. The M8 is listed.

```acceptance
tier: T2
size: <= 2400 changed lines, <= 21 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R sidecar_ --output-on-failure --no-tests=error
  - ci: linux-arm64, windows-x64, sanitizers incl. TSan on sidecar_protocol_tests
  - pi: build + ctest -R sidecar_
stops: [ABI header edit, network socket, new dependency, audio callback changes]
```
