<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC-1: Macro gate input (#341, from #337)

Tier T2. Adds an optional gate input to the Macro Oscillator without changing existing parameter or input indices, defaults, save formats, or upstream Plaits. See #337 and the original Compass ruling in `docs/tasks/OSC_TUNING_GATE.md` (currently on the module-prep branch). Gate is appended after Level.

With gate patched, Plaits trigger receives max(trigger, gate) and trigger_patched is true. When Level is not patched, gate also supplies the LPG Level and sets level_patched. A patched Level always wins. With no gate cable, trigger and Level behavior stays bit-for-bit as before. Each voice operates independently.

Native tests must show held-note sustain beyond 500 ms, release attenuation of 40 dB within one second at default Decay (LPG Colour=0 to exclude the upstream intentional HF bleed), repeated strikes and poly independence, combined trigger+gate no double-strike, and byte-exact trigger-only golden. UI shows Gate next to Trigger and the generated catalogue stays in sync.

```acceptance
tier: T2
size: <= 300 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_macro_oscillator" --output-on-failure
  - python tools/dev.py test
  - ci: Web job (npm ci && npm test) green on the exact PR head
stops: [ABI header edit, existing trigger/level behaviour change without gate, upstream Plaits source edits]
```
