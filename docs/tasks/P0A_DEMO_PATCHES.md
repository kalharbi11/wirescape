<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P0A: class demo patches (Phase 0, due 2026-11-07 morning, New York)

Three native v2 patches for the Pi class demo (`docs/DEMO_CLASS.md`). Data only: no engine, module or UI code.

**Read first:** `native/examples/chaos_usb_midi.json` (start from its shape), `native/document/runtime_module_config.h` (the `midi.cc_to_cv` `config.rows` format: up to 8 rows, `channel` 0–16, `controller` 0–127, `smoothing_ms` 0–2000), the module manifests under `native/modules2/*/module.json` for parameter IDs and ranges, and `docs/specs/patch_v2_draft.schema.json`.

**Build:**
- `native/examples/class_demo_1_chaos.json`: two CHAOS-lite instances, as in `chaos_usb_midi.json`, plus `midi.cc_to_cv` with 8 rows. Cable the CC outputs as modulation to the most audible CHAOS parameters and a mixer level, with amounts that stay musical across the full knob travel.
- `native/examples/class_demo_2_subtractive.json`: MIDI In → MIDI→CV (poly 4) → VCO → VCF → VCA ← ADSR, an LFO on the cutoff, and CC rows on cutoff, resonance, LFO rate/amount, envelope attack/release, VCO shape and output level.
- `native/examples/class_demo_3_macro.json`: Macro Oscillator (poly 4) with CC on model/harmonics/timbre/morph, plus VCF and ADSR.
- CC numbers: use the owner's numbers from the issue if posted. Otherwise use CC 21–28 on channel 0 (any channel, if 0 means omni in the config; check the header) and say so in the handoff, so the owner can edit them.
- Leave `devices` as `[]`. MIDI ports are injected locally by P0B, never committed.
- Settings: 48 kHz, `max_block_frames` 256.
- Add one row per patch to `native/examples/README.md`.

**Test:** a small Python test (unittest, in the existing `tools/foundation/tests` style, registered as a CTest named `class_demo_patches` that calls `wirescape_cli play`) renders each patch offline for 10 s and checks for a successful exit, finite output, and that the patch loads through `runtime_patch`. Silence is acceptable without MIDI, but no NaN and no load error.

**Owner check (not in must_pass):** sounds good with the real controller at the rehearsal.

```acceptance
tier: T0
size: <= 500 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "class_demo|runtime_patch" --output-on-failure --no-tests=error
stops: [new module, engine change, device access, ID change]
```
