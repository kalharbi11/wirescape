<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #432 Groove S1(a): UI-independent native two-track instrument

Keel's preauthorization: #432 comment 6731562789, protected path `docs/tasks/GROOVE_S1*.md`. Child of #429; S0 #430 is merged. This is the native half of S1, **not** the Studio editor or a finished release.

## Read first
- `AGENTS.md`, #432 latest handoff/Keel contract, `docs/WORKFLOW.md`, `docs/PLAN.md` Now/Decisions
- `docs/sequencer/{README,GROOVE}.md`, `docs/SDK_AUTHOR_GUIDE.md` §§7–12
- `native/modules2/groove/{config.h,dsp.h,README.md,DESIGN.md,module.json}`
- `native/document/{runtime_module_config.h,groove_runtime_tests.cpp,runtime_module_config_tests.cpp}`
- `native/sdk/examples/step_sequencer/{dsp.h,tests.cpp}`

## Scope
Native structured config: preserve `version:1`, extend to one or two tracks of one to 32 steps; omitted steps are rests on the shared 32-step/two-bar cycle. Optional step `mod1`/`mod2` in [0,1] and optional track `armed`/`mute`; unknown JSON refused, untouched authored source retained by Studio. Transient POD reserves 4×64, is validated quiescently, and is not persisted. Existing S0 patch data and `t1_*` slots and scalar `run` remain supported.

Implement two independent note owners on a shared audio-frame sixteenth clock, matched note-on/off identities, pitch (C4=0 V), gate (0/1), two trigger-held mods per track, velocity-zero rests, ties/rests/stop preserving mod and pitch. Add `t2_*` outputs and appended edge controls per Keel #432. Queue global/track stop/start for shared boundary, immediate mute/release, reset precedence, safe pending-release retry, no new note-on while a release is pending. First high input value is baseline; only low→high following loaded ACK starts. Do not modify UI wrappers, renderer, protocol, ABI, or the independent example sequencer.

## Evidence, stop conditions, rollback
Native regression tests include 44.1/48/96 kHz, blocks 1/17/64/128/256, 100+ loops, phase sync, same-frame precedence, start/stop queues, independent mute, two mod lanes, velocity-zero, note identity, queue admission/release retry, saved v1 config, refused candidates, stopped reload, downstream signal and event routes, and no render allocations. If native playhead/completed STOP cannot be observed with existing telemetry, **stop and ask Keel**; do not invent protocol fields. Studio editor/ack/reconciliation and owner listening belong to S1(b) and release acceptance.

Rollback disables new Groove native registration/adapter while leaving authored JSON untouched and unavailable with a truthful warning; never rewrite or drop saved patches. No live-edit framework or speculative SDK changes.

```acceptance
tier: T1
size: <= 900 changed lines, <= 20 files
must_pass:
  - ctest --test-dir build/native -R 'groove|runtime_module_config|ui_catalogue_sync' --output-on-failure
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native -R 'groove' --output-on-failure
stops: [ABI or protocol edit, unreviewed format/transport change, new dependency, test removal or threshold relaxation, size overrun]
```

Signed: ChatGPT (GPT-6, Groove S1 native builder), 2026-11-08.
