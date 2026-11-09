<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Audio service

Original Wirescape service code, GPL-3.0-only. Uses the existing PortAudio API
header (`native/third_party/portaudio/portaudio.h`, upstream MIT licence in that header)
without copying or modifying DSP. Loads the user's installed PortAudio binary:
`libportaudio.so.2` on Linux (ALSA host API), the existing DLL names on Windows.
No new PortAudio sources or revision are fetched; this is a runtime system library.

The callback has preallocated mono scratch, stereo duplication and gain/clamp at
the device boundary. It never allocates, locks or logs. Diagnostics run on control.
The runner owns the engine until stop/abort, close and termination complete. A
broken backend that cannot prove shutdown leaves its engine/context quarantined
until process exit; library code remains resident. No audio devices are opened
by CTest. Enumeration code is compiled but agents do not invoke it.

Live callbacks use `app_engine_render_live` with `app_engine_clock_ns()`, the
same steady-clock nanoseconds as MIDI ingress and Qt Studio. This publishes the
D13 clock anchor each block. PortAudio DAC timestamps are not used: no mapping
from that backend clock to the MIDI steady clock is established here. Offline
WAV rendering stays on `app_engine_render_channels` for deterministic frame time.
The internal `callback_render.h` seam shares the exact live render, interleave,
gain/clamp and fault path with device-free tests; its scratch is preallocated.
