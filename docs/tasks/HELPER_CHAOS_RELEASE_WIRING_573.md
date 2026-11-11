<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS keyboard release topology regression (#573)

T0 test-only helper. Owner wants existing CHAOS sounds preserved, not deleted to
work around UI problems. Read issue #573, #531 review, `tools/foundation/tests/test_patch_library.py`
and the four named public CHAOS keyboard JSON patches. Keep all devices off.

Add structural regression for keyboard gate → ADSR gate, ADSR envelope → both VCA
gain_cv inputs, CHAOS L/R audio → those VCAs, and no keyboard gate → CHAOS gate.
Use module-type ID lookup, not hard-coded node numbers. Mutation of a forbidden
gate edge must fail. No change to JSON patches or audio/silence thresholds.
Patch guide impact: none — tests-only existing release wiring.

```acceptance
tier: T0
size: <= 130 changed lines, <= 2 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_patch_library.py
  - python tools/patch_library.py check apps/studio/patches --shared
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [patch JSON edits, production DSP, new threshold or test suppression, dependency change]
```
