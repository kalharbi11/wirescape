<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Audio service

Original Wirescape service code, GPL-3.0-only. Uses the existing PortAudio API
header (`native/third_party/portaudio/portaudio.h`, upstream MIT licence in that header)
without copying or modifying DSP. Loads the user's installed PortAudio binary:
`libportaudio.so.2` on Linux (ALSA host API), the existing DLL names on Windows.
No new PortAudio sources or revision are fetched; this is a runtime system library.

**ASIO (Windows, Keel ruling 2026-11-11, owner request).** `tools/windows/setup.ps1`
builds the pinned PortAudio with its ASIO host API. It downloads the Steinberg ASIO
SDK `ASIO-SDK_2.3.4_2025-10-15.zip` (SHA-256
`d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca`) from
download.steinberg.net into `.tools/windows-setup/asiosdk/`. The SDK is dual
licensed (Steinberg ASIO License or GPL-3.0); Wirescape uses it under GPL-3.0. It is
built locally into the user's DLL and is never committed or redistributed by this
repo. `-NoAsio` restores the WASAPI/MME/DirectSound-only build. Enumeration is
host-API neutral, so installed ASIO drivers (interface drivers, FlexASIO,
ASIO4ALL) appear as `ASIO` devices with no engine change. CI keeps
`PA_USE_ASIO=OFF` (no device opens, no SDK download).

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

**ASIO driver lifetime and switching (#722).** A running PortAudio stream owns
its initialized backend, and device/ASIO lists are snapshots until that stream
is stopped and terminated. Warm polls and manual Refresh never initialize a
second PortAudio backend or probe another ASIO driver. A device change first
joins callbacks and terminates the old backend, then performs a cold device
listing and attempts the selected driver; a rejected open restores the prior
route when available, or reports a bounded reason without inventing audio.
An unavailable registry-only driver may include one sanitized PortAudio ASIO
host-error `detail` when exactly one registered driver is missing (not proof
of the individual driver's diagnosis). Driver tests use a fake backend only.
