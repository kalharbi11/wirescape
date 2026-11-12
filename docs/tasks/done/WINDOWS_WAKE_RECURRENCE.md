<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #570 Windows null-sink wake recurrence

Tier T2 correctness repair for the Windows seed-278, 48 kHz / 256-frame
null-audio protocol soak. See issue #570, prior scheduler #513/#520 and
Keel's strict zero-overrun ruling on #564. The 7.75 ms observed wake
lateness exceeds one 5.33 ms block even though rendering itself stayed
within budget; this is not evidence of a DSP overrun.

## Contract and implementation

The Windows null sink requests an interruptible high-resolution kernel
waitable timer (with older-OS fallback) while retaining MMCSS / 1 ms
timer-resolution pairing. Deadlines remain absolute steady-clock block
boundaries, not relative render-completion sleeps. A timer that signals
early must rearm for the same deadline; a late wake is still counted
by the unchanged RenderHistogram, with no deadline forgiveness.
Control signals a separate manual-reset event before joining the sink,
including the 512 ms long-block case. The event is closed only after join.
The real PortAudio callback, D5 fade/gap clock, OSC host, MIDI and
protocol bytes are outside this change.

Read first: `native/sidecar/session.{h,cpp}`,
`native/sidecar/null_sink_wait.h`,
`native/sidecar/protocol_unit_tests.cpp`,
`native/sidecar/protocol_tests.py`, issue #570 and #520.
The deterministic unit regression injects clock/wait responses for
100 ns rounding, a 5.33 ms early wake/rearm, long-block stop, timer
failure fallback and overdue catch-up. Keep the strict seed-278 soak
and all existing assertions unchanged. No devices or LAN.

Patch guide impact: none — scheduler only, no patch-facing behavior.
No migration; revert only the bounded null-sink timer/event path if needed.

```acceptance
tier: T2
size: <= 800 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R abi2_sidecar_ --output-on-failure --no-tests=error
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native -R sidecar --output-on-failure --no-tests=error
stops: [test weakening, ABI header edit, new dependency, device access, LAN access, CI change, size overrun]
```
