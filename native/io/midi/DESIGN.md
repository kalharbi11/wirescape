<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Qt-free MIDI acquisition and ingress

Wirescape service/adapter code is GPL-3.0-only. libremidi5.4.3 is pinned by immutable
FetchContent archive SHA `390707b5d18b590509e823386f03fa712ef6ac1b` with SHA256.
Its LICENSE.md records new contributions BSD-2-Clause, RtMidi-derived code MIT,
and ModernMidi-derived code BSD-2-Clause; fetched source retains those notices.
The minimum acquisition backends are WinMM on Windows and ALSA sequencer on Linux.
No system install is performed: ALSA public headers are fetched from alsa-lib
`81d70a27136840fa8fb1bec6eb8e04fc1475a380` (LGPL-2.1-or-later notices retained),
assembled under the build directory; libremidi dynamically loads libasound.so.2.
No ALSA library source is built or vendored. Optional network/keyboard/PipeWire/JACK/
WinUWP/Windows MIDI Services backends and unpinned optional dependencies are disabled.
Real acquisition is MIDI1; the engine/module path preserves typed MIDI2 UMP for
future backend support and fake protocol tests. No MIDI2 hardware claim is made.

`real_backend(false)` returns null before constructing an observer. The user enables
endpoints in the app or runs an explicit --confirm-midi path; agents/tests use the
fake backend and never enumerate/open real devices. Enumeration/open/close happen
on service/control threads. Endpoint selection stores names+system-name fallback,
not indices. Ambiguous names remain offline. Runtime source IDs stay stable during
the session and distinct across enabled slots. One open serves any subscriber count.

Each endpoint owns a bounded SPSC4096 ring with256 release reserve. Overflow is
counted; critical saturation requests a source-choke fallback and discards stale
queued traffic before it. UMP release classification is shared with engine D8.
MIDI1 zero-velocity Note On is critical; MIDI2 zero-velocity Note On is not. The
service callback copies fixed packets only; module DSP never calls the backend.
The worker is the sole engine producer writer. Timestamps are SystemMonotonic ns
and are mapped through D13 by the host. Polling hot-plug500ms, dispatch sleep1ms;
these are acquisition policies, not audio-thread sleeps or timing guarantees.

Stop joins the worker, stops/joins backend callbacks and drains source chokes while
the engine still exists. Backend faults close acquisition and emit chokes as well.
No producer survives engine replacement/destruction. Counters cover valid received
packets, dropped/oversize/malformed/ring-full/disconnect/send-fail and ignored RPN0.
Fake tests cover identity/reorder/rebind/missing endpoints, protocol distinctions,
fan-out, reserve/emergency, callback allocation count and teardown. Real-device
latency/hot-plug stability remain for the user's J9 session.
