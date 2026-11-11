<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS keyboard ADSR to stereo VCA release wiring (#573)

Issue #573, T0 helper. Test-only regression in
`tools/foundation/tests/test_patch_library.py` for the shipped keyboard CHAOS
patches. Requires keyboard gate to ADSR gate, ADSR envelope to both VCA
`gain_cv`, CHAOS left/right to those VCAs, and no keyboard gate to CHAOS gate;
a mutation test inserts the bad cable. No patch, DSP or engine changes; all devices off.
Patch guide impact: none, test-only regression.

```acceptance
tier: T0
size: <= 130 changed lines, <= 2 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_patch_library.py
  - python tools/patch_library.py check apps/studio/patches --shared
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [production DSP, patch JSON changes, new audio threshold, test suppression, new dependency]
```

Signed: Helper (Claude, build), 2026-11-11
