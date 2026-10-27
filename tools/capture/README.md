<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Audio capture harness

Stage 1 is entirely offline: `capture_offline_selftests` in CTest renders native
patch v1 through the unchanged ABI1 C API and tests synthetic captures. It covers
silence, clipping, mismatch, dropout, discontinuity, truncation, latency, rate
conversion, deterministic scheduled events and the combined Noise demo.

`reference.py` uses the patch's nodes, values, connections and outputNode; events
are ordered objects with frame, node, type, value and optional parameter/source/note.
The sample rate is 48 kHz, duration at most five seconds, and output gain is 0.05
(engine host gain 0.2 times harness attenuation 0.25). No device is created.
`analysis.py` supplies known tones, gated tones and sweeps. Every reference gets
an aperiodic, band-limited pilot, allowing alignment within 750 ms without confusing
cycles of a steady tone. Analysis uses linear rate conversion, not byte equality.

Initial engineering tolerances are explicit in `analysis.TOLERANCES`: correlation
at least 0.95, gain 0.5–1.5, fitted residual RMS at most 15%, timing within 20 ms,
DC below 0.005, and sampled response within 6 dB after gain compensation. Ten-ms
windows distinguish a missing active signal from intentional envelope silence.
These detect substantial regressions; they are not perceptual certification or a
calibrated hardware specification. Fixed spectral bins are diagnostics, not a full
transfer-function measurement. No speakers, microphones or device streams were tested.

## API research and Stage 2 design

Use Windows WASAPI shared-mode loopback on the default **render** endpoint, never
a microphone. Windows SDK/COM needs no third-party runtime. The endpoint stream
contains the system mix: monitor active sessions and report other active processes
or failed isolation checks as ambiguous; similarity alone cannot prove isolation.
Process-tree loopback isolates a PID tree but needs Windows build 20348 or later,
uses asynchronous activation, and is not tied to a particular endpoint. Endpoint
loopback directly measures the selected endpoint and has broader OS availability.
The manual runner will own only its own playback/capture streams, stop both on
failure, bound duration/storage/CPU, and require `--confirm-audio` before any API.
It will report endpoint output, not physical speakers or the GUI's live callback.

Sources: [Microsoft loopback documentation](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
and [Microsoft process-loopback sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback).
The equivalent Microsoft-maintained Markdown sources were read during implementation.
