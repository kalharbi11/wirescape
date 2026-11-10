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
- Confirm writes one `midi.cc_to_cv` row (next free row of the first module with one;
  otherwise a new module, plus a `midi.in` feed if the patch has none), cables `ccN` to the
  knob (modulation, amount 1) and keeps `ui.ccMap` labels truthful. Re-learning a mapped
  knob updates its row. Forget removes that knob's CC cable and a freed trailing row.
- Wired implicit-default outputs keep their meaning (holes are filled with explicit
  default rows). Fourteen-bit rows drop the flag when a controller above 31 is learned.
- One user gesture is one undo step.

## Decision for Keel/Compass (departure from "through `ccMap.set`")
The whole change (row, module, feed cable, mapping cable) is sent as **one `loadGraph`
live replan**, the same path as a MIDI input change, instead of separate `addModule`,
`ccMap.set` and `connect` edits. RealEngine queues edits independently, so a refused
`ccMap.set` would leave a cable onto an unconfigured default row. With one replan a
refusal restores the previous document and graph exactly. `ccMap.set` stays in the
protocol for a later narrow edit path; nothing here removes it.

```acceptance
tier: T1
size: <= 500 changed lines, <= 8 files
must_pass:
  - bash -lc "cd apps/studio && npm ci && npx playwright test tests/midi-learn-ui.spec.ts tests/mock-bridge/midi-learn.spec.ts --trace off"
  - bash -lc "cd apps/studio && npm ci && npm test"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [native/protocol/catalogue format change, parameter ID change, real devices in tests, threshold loosening]
```

Signed: Claude Sonnet 5.5 (Studio UI builder), 2026-11-10
