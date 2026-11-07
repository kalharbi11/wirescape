<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1E-b (#258): null-sink benchmark summary

Expose render timing for #235 without changing the protocol or opening devices.
Read first: `native/sidecar/DESIGN.md`, `session.h`, `session.cpp`,
`protocol_tests.py` and `protocol_unit_tests.cpp` in `native/sidecar/`.
Use the fixed 201-bin histogram, late-block counter and null-only summary flag.
Keel's #274 review requires the DESIGN contract and this discoverable acceptance
task. Codex Lead's continuation allowance extends the existing four-file PR to
six files for those two documents; the original 120-line cap remains.

```acceptance
tier: T2
size: <= 120 changed lines, <= 6 files
must_pass:
  - ctest --test-dir build/native -R sidecar_ --output-on-failure --no-tests=error
  - pi: ci/pi runner builds and runs ctest -R sidecar_ with null audio and fake MIDI
  - pi: contention benchmark: overruns=0 and p99<50; no display reports BLOCKED
stops: [device access in tests, protocol change, realtime allocation, threshold loosening]
```
