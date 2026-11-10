<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC slice B: sidecar host, consent and bounded status (#529)

**Dependency:** portable slice A #518/#525 must be merged to `main` before
runtime integration. Branch `task/529-osc-host`; close #529 only, keep #72
open for slice C. This is T2; Coda (Claude) reviews GPT/Codex-authored code.

## Read first
- `AGENTS.md`; #529 and Keel's #72 ruling (comment 6735453267).
- `docs/tasks/OSC_CURRENT_HOST_72.md`; merged A's
  `native/io/osc/codec.h`, `native/modules2/osc_in/`, and Monitor designs.
- Historical `task/72-osc-monitor`:
  `native/io/net/osc_service.{h,cpp}`,
  `native/control/osc_host.{h,cpp}`, `native/io/net/graph_tests.cpp`.
- Current `native/sidecar/session.{h,cpp}`,
  `native/sidecar/protocol_tests.py`, `native/sidecar/protocol_unit_tests.cpp`,
  `apps/desktop/src/` and `docs/specs/ENGINE_PROTOCOL.md` (read only
  until this subissue receives the needed protected-path authorization).

## Contract and implementation
- A single control-owned `OscHost` in sidecar `Session`, next to MIDI.
  Reconcile after every successful `loadGraph`/replan/D5 swap against
  committed `osc.in` nodes. Reuse one socket per shared port, bounded
  routes/listeners/packet storage; remove stale routes **before** their
  graph is torn down. Failed loads leave committed routes unchanged.
- Every worker delivery carries the current committed `gen` and an actual
  monotonic receive timestamp; stale-gen events are dropped. Preserve
  bounded timestamped `Engine2::enqueue` behavior and its one-block lead.
  Stop/join worker before audio/engine teardown, including restart/refusal.
  Never allocate, lock, perform I/O, format or log in render/event/reset.
- `--confirm-lan` is an owner-only launcher flag forwarded verbatim by
  Electron to `wirescape-engine`. Tests never pass it to open LAN. Without
  consent, even `config.lan:true` binds only `127.0.0.1`; with consent,
  bind LAN only for that route's explicit toggle. Preserve patch JSON.
  Busy/unbindable ports never refuse a patch: affected outputs remain zero
  and status reports the failure. No OSC Out, renderer socket, browser OSC,
  new dependencies, device access or unsolicited payload logging.
- Add optional `status.osc.routes` (at most 32) with module ID, port, bind,
  state `listening|port-busy|lan-not-confirmed`, received/dropped counts.
  No `setOsc`, no Monitor telemetry/UI in B (slice C). Record the
  authorized additive wire/flag contract in `docs/specs/ENGINE_PROTOCOL.md`
  **only after Keel repeats its protected-path authorization on #529**.
- Patch guide: update real OSC availability/consent/limits and a usable
  localhost wiring, without claiming Monitor UI or hardware verification.

## Device-off regression evidence
- Unit tests: shared-port routing and per-route counters; unauthorized
  LAN toggle falls back to loopback without patch refusal; explicit consent
  flag parsing/forwarding (without actually binding LAN); occupied port
  reports `port-busy` and zero output; bounded 32-route status.
- Native sidecar/loopback tests: malformed packets, bounded burst/drops,
  real nonzero worker timestamps and delayed event delivery; commit/gen
  reload/replan churn, refused load and route removal; clean shutdown
  with zero live sockets. Keep render allocation-watch assertions.
- Test only `127.0.0.1`, fake MIDI and null audio. No physical devices,
  non-loopback bind, real LAN packets or permission shortcuts.

```acceptance
tier: T2
size: <= 1600 changed lines, <= 24 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'osc|sidecar|app_engine|app_patch' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/desktop && npm ci && npm test'
  - pi-run: ctest --test-dir build/native -R 'osc|sidecar' --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [unmerged OSC A, unresolved protected spec authorization, new ABI or wire fields outside Keel ruling, LAN/device access, new dependency, realtime allocation or I/O, test or threshold weakening]
```

Signed: ChatGPT (GPT-6, OSC slice-B builder), 2026-11-10
