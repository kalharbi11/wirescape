<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design and provenance

Original Wirescape code, GPL-3.0-only. No third-party source or new dependency.

The ABI2 module never opens sockets, allocates, logs, waits or locks in process/reset. Its private bus is used if no host bus is injected at prepare; the control thread owns any external OutBus lifetime. CV samples and monotonic gate edge counters use release-published block counters; notes use a 64-slot atomic SPSC ring with producer-owned head and consumer-owned tail. The sidecar is responsible for trailing CV sends and gate edge ordering, and for stopping/joining before graph retirement (#731).

Patch JSON is parsed and validated off the render thread into a private binary config. Unknown JSON keys remain in Studio's source patch and are exposed only as bounded diagnostics.
