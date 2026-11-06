<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P0E: make the demo patches' CC21–28 audibly obvious (Phase 0 follow-up, data + test)

**Finding (Pi, 2026-11-07):** M8 CC21 arrives (values 0, 64, 127), and a diagnostic copy with CC21 → VCA gain (amount 0.075, base gain 0) clearly works. But in `class_demo_2_subtractive.json`, the original CC21 → filter cutoff change wasn't audible. Likely causes: modulation depth/scaling, base cutoff, or oscillator/filter settings. CC28 also needs re-checking (the owner's M8 sent CC44 for CCH; the owner fixes that on the M8).

**Read first:** `native/examples/class_demo_*.json`, `native/document/runtime_module_config.h` (CC rows), the VCF/Ladder/VCA/Macro/CHAOS manifests under `native/modules2/*/module.json` (parameter ranges and tapers), `docs/specs/ABI2_PLAN.md` (modulation in normalized space: the amount is added in normalized knob space, then clamped), `native/control/midi_host_tests.cpp` (injecting CC events).

**Build:**
- For each of the 3 patches and each of CC21–28: choose base value + amount so that CC 0 → 127 sweeps a large, musical part of the parameter's normalized range (rule of thumb: ≥0.4 of normalized range, staying inside it). Cutoff in particular: base low, sweep up through most of the audible range with a resonance that makes it obvious. Document the mapping (CC → parameter, range) in `native/examples/README.md` as a short table per patch, so the owner can label M8 CCA–CCH.
- No module, ID or engine changes. `devices` stays `[]`.

**Test (CTest `class_demo_cc_depth`):** offline, per patch and per CC row: hold a note, render 0.5 s with CC = 0 and 0.5 s with CC = 127 (events through the normal ingress), and assert the measured parameter effect is large. Use the engine's effective-parameter value if exposed; otherwise use RMS (gain targets) or spectral centroid (cutoff, timbre) with a ratio ≥ 1.5 between the two renders. Thresholds go in the test with a one-line justification each.

**Owner check:** per patch, sweep CCA–CCH on the M8 and confirm each is audible (record in `docs/DEMO_CLASS.md` §1).

```acceptance
tier: T1
size: <= 450 changed lines, <= 6 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "class_demo" --output-on-failure --no-tests=error
stops: [module/param ID change, engine change, device access]
```
