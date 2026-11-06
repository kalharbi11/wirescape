<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P0D: live MIDI clock in the headless audio callback (Phase 0 follow-up, do first)

**Bug (found on the Pi, 2026-11-07):** `wirescape play --confirm-audio --confirm-midi` received M8 notes (visible in `aseqdump` and `aconnect`) but played nothing. `native/io/audio/portaudio_service.cpp:65` renders with `app_engine_render_channels()`, which passes `UINT64_MAX` (the offline anchor) to the engine. Timestamped live MIDI from `midi_host` then can't be placed in the block. Qt Studio already uses `app_engine_render_live(…, app_engine_clock_ns())` (`native/ui/studio_controller.cpp:389`). The owner's local one-line change to the same call made M8 notes audible on the Pi. This task makes that the production fix, with a regression test.

**Read first:** `native/io/audio/portaudio_service.cpp` and `.h`, `native/control/app_engine.h/.cpp` (`render_live`, `clock_ns`), `native/control/midi_host_tests.cpp` (how live-timestamped events are injected and rendered), `native/cli/play.cpp`, `native/io/audio/DESIGN.md`.

**Build:**
- In the PortAudio callback, render with `app_engine_render_live(engine, rows, 2, frames, app_engine_clock_ns())`. The offline WAV path in `play.cpp` stays on `render_channels` (deterministic). Optional: anchor with PortAudio's `timeInfo->outputBufferDacTime` only if the mapping to the steady clock is well defined; otherwise keep the same clock as Studio and say why in `DESIGN.md`.
- Make the callback's render step testable without a device: extract it into a small function, e.g. `render_callback_block(Context&, float* interleaved, unsigned long frames, uint64_t now_ns)`, in an internal header. The PortAudio callback calls it with `app_engine_clock_ns()`; tests call it with a controlled clock. Realtime rules hold: no allocation, locks, I/O or exceptions.
- Update `native/io/audio/DESIGN.md` (which clock anchors live rendering).

**Tests (CTest, no devices):** `audio_callback_live_midi_tests`:
1. Load `native/examples/class_demo_2_subtractive.json` (or a minimal MIDI In → MIDI→CV → VCO → VCA ← ADSR → Output graph). Inject a note-on timestamped "now" through the same ingress path `midi_host` uses. Render blocks through the extracted callback function with a clock that advances by the block period. **Assert:** non-silent output within 2 blocks, and silence again within the release time after note-off.
2. The same with an event timestamped mid-block: the onset lands within ±1 block of its timestamp.
3. Guard: rendering through the old offline call with the same live-timestamped event fails assertion 1. It is kept as a documented "fail before fix" check in the test's comment, not as a passing test of wrong behaviour.
4. Zero allocations in the render step (existing allocation counter if available).

**Also update:** `docs/tasks/P1B_ENGINE_SIDECAR.md` already requires reusing `portaudio_service`, so the sidecar inherits this fix. No change is needed there.

**Owner check (not in must_pass):** after merge, rebuild the CLI on the Pi from a clean `demo`/`main` checkout (drop the local diagnostic edit first: `git checkout -- native/io/audio/portaudio_service.cpp`). Then `m8_2_subtractive.json` plays from the M8 exactly as with the local fix.

```acceptance
tier: T2
size: <= 250 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "audio_callback_live_midi|midi_host|play" --output-on-failure --no-tests=error
  - ci: linux-arm64, windows-x64, sanitizers
  - pi: build + ctest -R audio_callback_live_midi
stops: [ABI header edit, audio device in tests, allocation in the callback, change to offline render determinism]
```
