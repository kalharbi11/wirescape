<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Shared localhost-first OSC acquisition

Original GPL-3.0-only code. One network worker polls all sockets every10ms; at most16
ports and64 routes. All subscribers on one port share a socket. Conflicting LAN
settings on the same port are rejected before opening anything. LAN requires both
module toggle and global user confirmation. Automated tests bind127.0.0.1 only.

Receive buffers are preallocated (maximum UDP payload65507bytes), with J's default
2048-byte packet cap. Oversized datagrams are consumed and counted, malformed packets
are rejected, and each valid message is normalized into the ABI's1472-byte arena bound.
At most64 datagrams per listener are consumed per poll before checking stop again.
Engine enqueue copies borrowed packet bytes synchronously; no pointer escapes a poll.
Stop joins the network worker before closing sockets and before destroying its engine.
Counters cover packets, malformed input, drops, truncation and live sockets. This is
not a claim of measured LAN latency or controller stability; those belong to user J9.
