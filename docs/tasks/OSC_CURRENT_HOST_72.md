<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC current-host integration inventory (#440, parent #72)

This is a T0 **source inventory and implementation gate**, not OSC implementation or
acceptance for the current Electron app. Parent issue: #72; old PR: #82
(`task/72-osc-monitor`, head `a2bf373`). The historical PR's 52-file
Qt/CLI-era integration is **not** evidence for `wirescape-engine` or Studio.
Codex Lead authorized this docs path in #72 on 2026-11-09.
The source comparison below is against `main` at `50380fd`; recheck on pickup.

## Reusable implementation from old PR #82

| Old source | Reusable behavior | Verification to retain |
| --- | --- | --- |
| `native/io/osc/codec.h` and codec tests | Bounded OSC 1.0 i/f/s/b/T/F/N/I decode/encode; iterative depth-limited bundle handling; malformed/array refusal | Codec boundaries, max packet/depth/argument and unsupported-tag rejection |
| `native/io/net/osc_service.{h,cpp}` | One nonblocking UDP socket per shared port; localhost bind by default; LAN requires explicit consent; fixed-size routes, listeners and bounded worker loop; stop/join | `native/io/net/tests.cpp`: two routes share a port, conflicting LAN settings refused before bind, localhost delivery, zero sockets after stop |
| `native/control/osc_host.{h,cpp}` | Parses `osc.in` config/port/LAN, starts and stops producer, routes worker timestamp through `Engine2::enqueue` as timestamped events | Real node identity and generation; no stale delivery across patch replacement/replan |
| `native/modules2/osc_in/` | Stable `osc.in`; `row1`–`row8` signal outputs and `messages` event output; bounded rows, numeric wildcard ordering, stale hold/zero | Bounded burst, sorting, timeout/reset, split-block determinism and allocation watching |
| `native/modules2/monitor/` | Stable `monitor`; event input `events`, signal inputs `in1`/`in2`; bounded tap/egress, no realtime formatting | Monitor log/lifetime/readout and no allocation in render |
| `native/io/net/graph_tests.cpp` | Loopback UDP fixture with **nonzero actual worker timestamp**, one-block scheduling lead, `0.75f` delivered value and render-allocation check | Preserve both render blocks and timestamp assertions; never retime ingress to zero |

Stable module, port and config IDs above are the #72 contract. There is no
authorization to rename them, change patch format, or add OSC Out.

## Missing from current Electron + sidecar host

1. **Module registration:** `native/control/module_catalog.inc` and the
   `apps/studio/public/wirescape_modules.json` catalogue on the inspected
   `main` do not contain `osc.in` or `monitor`. Adding a C++ module alone
   cannot make Studio present it; regenerate the catalogue and test drift.
2. **Sidecar ownership:** `native/sidecar/session.h` contains audio/MIDI
   services, but no OSC worker or control-owned lifecycle. `session.cpp`
   does not parse `--confirm-lan`; its commit/reload/shutdown paths do not
   configure or stop an OSC host. The legacy `play`/Qt host cannot substitute
   for this separate `wirescape-engine` process.
3. **Event/graph contract:** define which bounded Engine2 producer owns OSC,
   and how port sharing, generation, replan, load refusal, source removal,
   disconnect and teardown synchronize against rendering. Reuse the existing
   timestamped event model; never introduce audio-thread I/O or allocations.
   The current sidecar's real render clock must be proven with fake/null audio.
4. **Consent:** host/local UDP is **127.0.0.1 only** until both the module
   toggle and explicit owner confirmation are present. Electron launcher,
   UI and native sidecar need one approved consent contract; browser/WASM
   networking is excluded. CI/tests may use localhost only, never LAN/devices.
5. **Studio/Monitor UX:** Studio needs truthful module descriptors, a
   bounded Monitor readout with lifecycle/stale behavior, and any necessary
   preload/EngineClient data access. The current v0 stdio protocol does not
   establish a Monitor log stream or OSC-specific user-consent message.
   Do not invent fields, renderer UDP sockets or a network preload API.
6. **Acceptance:** old #82 reviews and hosted checks are not current-head
   T2 review/acceptance for an Electron integration. Verify both native
   sidecar and Studio behavior and task-specific Pi results on the new head.

## Keel: T2 contract decisions required before implementation

- Owner/lifetime: control-owned OSC service instance(s), producer index and
  Engine2 event generation on loadGraph, replan, restart, and shutdown.
- Permission: `--confirm-lan` forwarding/consent and per-patch local/LAN
  behavior, refusal semantics for unapproved LAN or occupied ports.
- Monitor: whether existing v0 status/telemetry can carry bounded logs, or
  whether an additive protocol capability/message is required. Define
  payload size, frequency, privacy and stale-clearing; no speculative wire.
- Review whether adapting legacy `osc_host.cpp` constitutes a new engine
  ownership contract. All runtime/host/protocol edits remain T2 with an
  independent other-model-family review.

These are **blocking decisions for runtime slices only**. The T0 inventory
may merge independently; #72 must remain open until actual integration.

## Proposed implementation slices after Keel's ruling

A. Port reusable OSC codec, modules and offline tests onto a fresh
   main-based branch, without Qt changes or ID/ABI/schema changes.
B. Implement control-owned OSC host in the sidecar, stopping producers
   before graph teardown. Reuse shared port and the real callback clock.
C. Connect approved consent and bounded Monitor presentation to Electron
   preload/Studio; regenerate native and web module catalogue.
D. Add sidecar process tests (fake MIDI, null audio, loopback only), burst,
   malformed/bundle, port sharing, denied-LAN, reload/churn, generation,
   dropped/late/fault counters and no-RT-allocation tests.
E. Validate the final exact head through `python tools/dev.py accept <issue>`,
   required `guard`, `linux-arm64`, `windows-x64`, `sanitizers`,
   independent T2 COMMENT review, and real `pi-run:` commands via the
   private runner/`ci/pi-accept`. Queued runs and old #82 passes do not count.
   Owner-only LAN and real-device verification stays separate.

## Acceptance for THIS inventory-only task

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; s=Path('docs/tasks/OSC_CURRENT_HOST_72.md').read_text(); assert len(s.splitlines()) <= 180; assert all(x in s for x in ('native/io/osc/codec.h', 'native/sidecar/session.h', 'apps/studio/public/wirescape_modules.json', 'Keel: T2'))"
stops: [runtime or protocol edits, ABI/ID/schema changes, any LAN/device test, threshold weakening]
```
