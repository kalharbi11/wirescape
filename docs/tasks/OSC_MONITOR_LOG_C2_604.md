<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC slice C2: Monitor log telemetry and message list (#604, part of #72)

The owner wants to see the OSC/MIDI messages that reach a Monitor card. The `monitor` module has an `events` input, but the protocol carries no message log. This packet is the whole feature, end to end: spec line, sidecar, Studio card, FakeEngine, patch guide and tests. **T2** (protocol).

Keel ruling (binding, [#604](https://github.com/kalharbi11/wirescape/issues/604#issuecomment-6790998538)). Protected path named on the issue: `docs/specs/ENGINE_PROTOCOL.md`.

Read first: `docs/specs/ENGINE_PROTOCOL.md` §7 (telemetry, `eventActivity`), `native/control/osc_host.cpp`, `native/sidecar/session.cpp` (MIDI ingress and the telemetry tick), `native/document/telemetry_taps.h`, `apps/studio/src/engine/FakeEngine.ts`, `apps/studio/AGENTS.md`.

## What to build
1. **Spec:** add one bullet under §7 telemetry. `telemetry.monitor["<nodeId>"]` = the latest ≤32 entries, oldest first, each `{t, kind:"osc"|"midi", address|note, args[≤4]}`. Addresses are truncated to 64 bytes and args are finite numbers only. It is omitted when empty, cleared on every graph commit (same rule as `eventActivity`), and advertised as the `telemetry.monitor` capability in `hello`.
2. **Sidecar:** capture **on the control thread at ingress**: OSC in the OSC host, MIDI in the MIDI ingress. Never capture on the render callback. Capture only for observed `monitor` nodes whose `events` input is cabled **directly** from an `osc.in` or `midi.in` node. Use one fixed 32-slot ring per monitor, preallocated at commit, with no allocation per message. Emit at the existing ≤30 Hz telemetry rate.
3. **Web/WASM engine:** do not advertise the capability unless you implement it there. Studio must handle its absence.
4. **Studio Monitor card:** a scrolling message list (newest at the bottom; time, kind, address or note, args) with **Pause** and **Clear**. Clear is UI-side only. The list shows "direct OSC/MIDI only" when nothing is listed, and "not available" when the capability is missing. FakeEngine supplies sample entries.
5. **Patch guide:** in the OSC section of `docs/PATCH_DESIGNER_GUIDE.md`, add the wiring `osc.in messages → monitor events` (and `midi.in`), plus the 32-entry limit and the direct-only limit.

## Not in this packet
- Indirect routes (osc.in → … → monitor): out of scope by ruling.
- Learn, and a list of recently seen addresses on the OSC In card: later.

## Tests
- Protocol tests (`native/sidecar/protocol_tests.py` or the unit tests), with `--null-audio` and loopback OSC on 127.0.0.1 only:
  - entries appear, oldest first, at most 32;
  - an address over 64 bytes is truncated;
  - non-finite args are dropped;
  - the list clears on commit;
  - a monitor fed indirectly gets nothing;
  - the capability is present in `hello`.
- MIDI: use the fake MIDI path only.
- Playwright with FakeEngine: the list renders, and pause, clear, and capability-absent all work.
- Self-review states the realtime check: no capture, allocation or lock on the audio thread.

```acceptance
tier: T2
size: <= 900 changed lines, <= 14 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [render-path capture or allocation, any protocol change beyond the ruled field, LAN bind or device open in tests, ID changes, new dependency]
```

Signed: Coda (Claude), 2026-11-11
