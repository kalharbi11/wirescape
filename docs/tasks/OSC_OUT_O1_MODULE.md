<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #730 OSC Out O1: `osc.out` module, OutBus and config parsing

Read first: `docs/specs/OSC_OUT.md` §1, §2, §4, §6, then `native/modules2/osc_in/` (config.h, dsp.h, tests.cpp, module.json), `native/document/runtime_module_config.h` (the `osc.in` branch), and the `new-module` skill or `docs/SDK_AUTHOR_GUIDE.md`.

- **First commit, as a draft PR today:** copy the spec §4 types verbatim into `native/io/osc/out_bus.h` and `native/modules2/osc_out/config.h`, then implement `out_configuration()`. O2 (#731) codes against the same types in parallel. Any change to them is a stop (Keel).
- Then: module DSP (`dsp.h`, `module.cpp`, `module.json`, README, DESIGN), `runtime_module_config("osc.out", …)` with strict validation and unknown-key preservation, catalogue regeneration, unit tests per spec §6 (module half).
- Realtime: the DSP only does atomic stores and ring pushes. Poly inputs read channel 0 (`FIRST`).

```acceptance
tier: T2
size: <= 650 changed lines, <= 12 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - git diff --check
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [ID or config-shape change from the spec, ABI header edit, allocation or lock in process, new dependency]
```

Patch guide impact: O4 (#733) writes it; this PR says `Patch guide impact: deferred to #733`.
Signed: Keel, 2026-11-13
