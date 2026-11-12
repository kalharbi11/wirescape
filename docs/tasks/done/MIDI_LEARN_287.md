<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #287 MIDI Learn UI (2B-b)

Studio-only T1. The engine side (`midiLearn` command, `status.midi.learn`) is merged
(#381/#390, `ENGINE_PROTOCOL.md` §4/§7); the CC rows with min/max/bipolar are merged
(#210). Compass's scope (2026-11-09 on #287) and PLAN D8 hold: the UI arms Learn,
shows the captured CC, and commits it to its own lossless `config.rows`; the engine
never writes the document; `ui.ccMap` stays a display hint.

## Behaviour
- Knob context menu (modulatable knobs, only when the engine advertises `midiLearn`):
  **MIDI Learn…** and, if a CC module is cabled onto that knob, **Forget MIDI mapping**.
- Learn bar: "Move a control on your controller…", then `CC n · ch c · device`, an "any
  channel" checkbox, Confirm (button or Enter) and Cancel (button or Esc). Arm, capture
  and the 30 s timeout are transient engine state; a dropped arm closes the bar.
- Confirm writes one `midi.cc_to_cv` row on a converter that hears the device that moved
  (its `midi.in` feed is `all_enabled` or names that `name` + `system_name`), or on one with
  no feed yet, which gets one in the same edit; otherwise a new converter, fed by a `midi.in`
  that takes the device (added, naming it, if none does). It cables `ccN` to the knob
  (modulation, amount 1) and keeps `ui.ccMap` labels truthful. A device gone or disabled
  before Confirm is refused with a toast. Re-learning a mapped knob updates its row in place
  only when no other knob shares that row and its converter hears the device; otherwise it
  forks to a free or new row. Forget removes only that knob's CC cable and a freed trailing row.
- Ownership (#500, Keel ruling): Learn/Forget go to the current playing/draft edit target.
  While the one-graph replan or refusal recovery is pending, document edits (including
  undo, cables, CC rows and labels) are blocked and visibly marked as applying.
  Refusal restores the prior document and history exactly before editing resumes. A lost load
  uses `gen` to decide whether it applied; after 10 s without a push or on `app.engineExited`,
  Studio offers `app.restartEngine` and stays locked until the pre-Learn patch is reloaded. Each restart/reload
  attempt has a 10 s bound and restores a retry action on timeout.
- Wired implicit-default outputs keep their meaning (holes are filled with explicit
  default rows). Fourteen-bit rows drop the flag when a controller above 31 is learned.
- One user gesture is one undo step.

## Keel-approved single-replan decision (departure from "through `ccMap.set`")
The whole change (row, module, feed cable, mapping cable) is sent as **one `loadGraph`
live replan**, the same path as a MIDI input change, instead of separate `addModule`,
`ccMap.set` and `connect` edits. RealEngine queues edits independently, so a refused
`ccMap.set` would leave a cable onto an unconfigured default row. With one replan a
refusal restores the previous document and graph exactly. `ccMap.set` stays in the
protocol for a later narrow edit path; nothing here removes it.

Required mock-bridge evidence (covered by the focused Playwright command below):
higher-gen keep, same-gen revert, late reply ignored, 10 s no-push restart,
restart-attempt timeout/retry, and `app.engineExited` restart/reload.

```acceptance
tier: T1
size: <= 1450 changed lines, <= 16 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npx playwright test tests/midi-learn-ui.spec.ts tests/mock-bridge/midi-learn.spec.ts --trace off'
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green on the exact head
stops: [native/protocol/catalogue format change, parameter ID change, real devices in tests, threshold loosening, timeout behavior that assumes an unanswered load was refused]
```

Signed: Claude Sonnet 5.5 (Studio UI builder), 2026-11-10
Updated (review fixes, budget per owner/Coda ruling): Claude (MIDI Learn builder), 2026-11-10
