<!-- SPDX-License-Identifier: GPL-3.0-only -->
# 2A-2 (#267): sidecar live replan with D5 fallback

After #247 and #209, route incremental topology edits through the existing
control-thread `midi::Host::replan` while audio renders. The sidecar must keep
the same candidate preparation, validation, session, generation and protocol
ack/refuse semantics. A refused live edit must not mutate the graph or routes.

**Read first:** `docs/specs/ENGINE_PROTOCOL.md` §4–5, `docs/PLAN.md`
D5–D7, `native/sidecar/session.{h,cpp}`, `native/control/midi_host.*`,
`native/io/midi/midi_service.cpp`, and Keel's review on PR #209
(issue comment 6682061588).

- Use the prepared candidate with a park deadline of at most three render
  blocks. Retry only MIDI try-lock contention, at most three additional
  attempts separated by approximately 2 ms. On exhausted contention return
  `busy` without altering the graph, routes or generation.
- Take the existing D5 stop–swap–start fallback immediately when audio is not
  rendering, for `loadGraph`/new documents, crash recovery and non-busy
  replan refusal. Preserve D5 rollback and fade guarantees.
- Exercise add/connect/disconnect/remove and refusal through the real stdio
  protocol with the paced null sink and fake MIDI only. Retain the existing
  fake Host MIDI-note/held-release tests and the 256-frame gap bound.
- No physical audio/MIDI device access in CI, no ABI/protocol change, and no
  new dependencies. T2 cross-family review by Keel.

```acceptance
tier: T2
size: <= 400 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R sidecar_ --output-on-failure --no-tests=error
  - ctest --test-dir build/native -R "^abi2_midi_live_host_tests$" --output-on-failure --no-tests=error
  - ci: windows-x64, linux-arm64, sanitizers (ASan/UBSan and TSan)
  - pi: build + ctest -R sidecar_
stops: [ABI header edit, new dependency, protocol change, removed tests, threshold change]
```
