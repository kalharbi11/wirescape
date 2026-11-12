<!-- SPDX-License-Identifier: GPL-3.0-only -->
# R3b-pre: port the four Qt-test assertion groups to Qt-free tests (#704)

**Depends on:** none (parallel with R3a #482). Must merge before R3b #468
can delete its Qt tests. Old Qt test files stay intact in this PR.

The audit in #468 (comment 6750495273) identified four test families whose
guarantees must survive Qt removal. Replace every relevant assertion with
an executed Qt-free test or name the explicit D15-retired Qt-only behavior.

1. **A — serialized patch equivalence:** v1 to saved v2 on disk, reload, render
   and compare PCM; preserve the ABI1 versus app engine bound of `1e-7`.
2. **B — loaded graph behavior:** keyboard/Macro, dual CHAOS and synthetic USB
   MIDI → CHAOS; assert four voices, finite nonzero PCM, zero callback
   allocations/faults, and off-rate refusals.
3. **C — failure rollback:** reject a failed prepare without replacing or
   destroying a running graph; reject a failed Electron file commit without
   clearing Studio's dirty marker or overwriting the prior disk document.
4. **D — actual audio lifecycle:** exercise production `audio::Stream` with a
   fake device-off PortAudio backend, from open/start through quiescent stop,
   failure cleanup, and return of engine ownership. A mock Session alone is
   not sufficient.

The PR description must map original Qt assertions to replacement test names,
existing unchanged coverage, or explicit D15-retired behavior. Devices off.
No new dependency; no ABI/protocol changes; no removed tests. Patch guide: no
changes (tests only).

```acceptance
tier: T2
size: <= 500 changed lines, <= 10 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [deleting a test, loosening a tolerance, ABI or protocol change]
```

Signed: gpt-builder1, 2026-11-12
