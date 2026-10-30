<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J4: MIDI In, MIDI→CV and CC→CV (#71, PR #81, stacked on J2 #79; retains required J3 CLI/runtime dependencies)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "B3 / J4"; behaviour reference `docs/design/BETA08_MUSICAL_CORE_BRIEF.md` §1–2 (IN-01..07, CV-01..12, fixtures 1, 2, 4).

## Answers to the brief's §6 P0 questions (planner, final)
1. **IDs** (frozen once merged): modules `midi.in`, `midi.to_cv`, `midi.cc_to_cv`; port IDs as in brief §1–2 (`midi`; `pitch`, `gate`, `velocity`, `aftertouch`, `pressure`, `bend`, `mod`, `notes`); CC→CV outputs `cc1`…`cc8`. `essentials.quantizer` / `essentials.basic_sequencer` are deferred until after J9 and not decided here.
2. **Minimum C1 subset before M4:** one backend (libremidi, WinMM + ALSA, MIDI 1.0 acquisition; UMP types preserved), endpoint identity = saved name + system-name fallback, explicit enable list, one open per endpoint, hot-plug (vanished → offline + release its notes; returning → rebind by name), bounded SPSC ingress per callback context, counters (received, dropped, malformed), a fake backend for every test. Owner: Codex in this issue. Not in J: Learn, Mappings panel, MIDI Out, MIDI Clock, Activity panel.
3. **Allocator semantics:** `native/voice/` is the oracle (no second allocator). Rotate/Reuse/Reset/Mono as implemented there. Repeated Note On of a held key: FIFO (oldest activation releases first). Stealing: NOTE_CHOKE before the replacement NOTE_ON at the same offset. Release by (source, channel, key, note_id). Channel count = `voice_count`, changed only at block boundaries.
4. **MPE postponed.** RPN 0 is ignored and counted; bend range is the manual parameter (default ±2 st). **`retrigger`:** J ships only `on_new_note` (no `retrigger` parameter exposed); `off` is a post-J9 item. This settles PR #81's open question.
5. Sequencer question: moot for J (the user writes their own; Template B in `native/sdk/examples/step_sequencer/`).

CC rows: 7-bit by default, per-row 14-bit toggle (MSB CC 0–31 + LSB CC+32).

File cap60 is the planner's 2026-10-29 10:30 decision on#29.

Self-decisions (WORKFLOW3c): caps1800→2400lines (+33.3%),60→70files (+16.7%)
retain required J3 CLI/runtime/audio dependencies while refreshing on J2 for
the owner's demo. No tests removed; J3 standalone acceptance is not claimed.

```acceptance
tier: T2
budget: 2 Codex sessions (remaining)
size: <= 2400 changed lines, <= 70 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "midi|note_identity|runtime_module_config|abi2_host_keyboard|app_patch_tests|app_engine_tests" --output-on-failure
  - ctest --test-dir build/native -R "^abi2_midi_to_cv_tests$" --output-on-failure   # brief fixtures 1/2/4 MIDI portion, blocks 1/17/128, 44.1/48/96 kHz; quantizer remains post-J9
  - python -m unittest discover -s tools/foundation/tests -p "test_pi_ui_probe.py"
  - pi-run: python tools/pi_ui_probe.py
  - ci: linux-arm64, windows-x64, sanitizers (TSan on the service/ingress tests), guard green on the exact head
  - pi: ctest -R "midi|note_identity"
stops: [ABI edit, new dependency beyond libremidi, real MIDI port opened in a test, unspecified design choice]
```
