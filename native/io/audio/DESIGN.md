<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Audio service

Original Wirescape service code, GPL-3.0-only. Uses the existing PortAudio API
header (`native/ui/third_party/portaudio.h`, upstream MIT licence in that header)
without copying or modifying DSP. Loads the user's installed PortAudio binary:
`libportaudio.so.2` on Linux (ALSA host API), the existing DLL names on Windows.
No new PortAudio sources or revision are fetched; this is a runtime system library.

The callback has preallocated mono scratch, stereo duplication and gain/clamp at
the device boundary. It never allocates, locks or logs. Diagnostics run on control.
The runner owns the engine until stop/abort, close and termination complete. A
broken backend that cannot prove shutdown leaves its engine/context quarantined
until process exit; library code remains resident. No audio devices are opened
by CTest. Enumeration code is compiled but agents do not invoke it.
