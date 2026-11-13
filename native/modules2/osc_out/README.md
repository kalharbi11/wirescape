<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC Out (osc.out)

Eight CV/gate inputs (row1–row8) and a poly Notes-event input; no outputs or parameters. Signal inputs use FIRST poly mode: only channel 0 is transmitted. The realtime module only updates the fixed-size OutBus. All UDP I/O belongs to the sidecar sender (#731).

Config version 1: port (default 9001), rate_hz (default 100, range 1–500), rows (up to 8), notes (disabled by default). Rows default to enabled CV addresses /ws/row/1 through /ws/row/8 and deadband 0.001. Each row may set kind cv or gate and an optional enabled boolean. Notes default to /ws/note. Only 127.0.0.1 is permitted. Host/LAN settings are refused.

Gate transitions are counted at sample precision; note on/off events occupy a bounded 64-slot SPSC ring. If full, dropped increments without blocking. No listener is required for offline renders. See docs/specs/OSC_OUT.md for the sender's rate, gate ordering, and wire format.
