<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #731 OSC Out O2: sidecar sender, `status.osc.out`, loopback protocol tests

Read first: `docs/specs/OSC_OUT.md` §3–§6, `native/control/osc_host.*`, `native/io/net/osc_service.*`, `native/io/osc/codec.h` (`encode`), `native/sidecar/session.cpp` (OSC status), `native/sidecar/protocol_tests.py` (OSC In loopback tests), `docs/specs/ENGINE_PROTOCOL.md` §status.

- Code against the spec §4 types now. If O1's (#730) header commit is out, branch from it; otherwise add the identical headers and drop them when you rebase. Merge after O1.
- Sender worker: polls OutBus ≤1 ms, rate limit + deadband + trailing send per row, ordered gate edges, notes in order. One unconnected non-blocking UDP socket to `127.0.0.1:<port>`; errors count as `dropped`. Host fills `OutConfiguration::bus` before prepare; stop/join before retirement/replan/teardown.
- `hello.capabilities` `"osc.out"`; `status.osc.out` per spec §5. Add the additive lines to `ENGINE_PROTOCOL.md` (Keel approves the spec part).
- Protocol tests: OS-assigned localhost receiver; rate cap, a 1-sample gate pulse → 1 then 0, note order, replan and EOF join, no listener → the engine lives and nothing blocks; `dropped` via the injected send seam and gate coalescing (spec rev 2 §4, §6). `status.osc.out` is the `out` array inside `status.osc`. Updating the strict `hello.capabilities` fixtures (protocol_tests, audio_recovery_tests, golden_v0) for the new capability is required, not weakening. `--null-audio` only.
- **Pi first:** add a `pi:` line to the PR body and `ci/pi` on the draft; link the private run.
- Update `docs/ARCHITECTURE.md` (the OSC section) in this PR.

```acceptance
tier: T2
size: <= 700 changed lines, <= 11 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [non-loopback send, render-thread syscall/lock/alloc, protocol field outside spec §5, removing or loosening an OSC In test]
```

Patch guide impact: deferred to #733.
Signed: Keel, 2026-11-13
