<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove G1 native v2 config (#496)

Task-specific acceptance wrapper for issue #496. Source of truth: `docs/tasks/GROOVE_PLAYABLE_V1.md` and the issue's Codex Lead seed/instance-ID ruling. This does not change the shared #432 contract.

## Scope / review

- Add accepted v2 JSON: track lengths 8/16/32/64, quarter-step gates and conditional note trigs; preserve native v1 rendering and lossless authored v1 input.
- Keep all published port and parameter IDs. G3 #497 owns the appended observation ports; normally integrate its source without rewriting either branch.
- Add only control-side numeric instance-ID seed plumbing, native fixed-size scheduler updates, tests and patch capability guidance. No public ABI/SDK edits.
- Device-free native tests, exact-head CI and private Pi acceptance; Coda or another Claude-family reviewer must provide T2 COMMENT on the final head.
- Stop on changed ABI/protocol, incompatible v1 rendering, allocation or locks on render thread, changed thresholds or ambiguous STOP timing.

```acceptance
tier: T2
size: <= 1600 changed lines, <= 20 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R groove --output-on-failure --no-tests=error"
  - bash -lc "cd apps/studio && npm ci && npm test"
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the exact head
stops: [existing Groove parameter/port ID change, v1 rendering change, live editing during playback, chords, more than 2 tracks, protocol change outside §7 telemetry]
```

Signed: ChatGPT (GPT-6, Groove G1 native continuation builder), 2026-11-10
