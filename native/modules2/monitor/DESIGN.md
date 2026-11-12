<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design and provenance

Original Wirescape code, GPL-3.0-only, no upstream source or new DSP dependency.
OSC is an own bounded implementation of OSC1.0 with T/F/N/I tags; no third-party
codec or regex library is included. See `native/io/osc/` and `native/io/net/`.
Module process/reset/event work has no allocation, locks, formatting or I/O.
Services and host UI own sockets and formatting; private host-owned telemetry/tap
pointers are injected while quiescent and are never accepted from patch JSON.
State stores a version marker; host stores configuration, and loading resets DSP
history without resurrecting controller gates. Unknown/short state is rejected.
Telemetry uses32-bit atomics so portable DSP does not require a hidden64-bit lock.
