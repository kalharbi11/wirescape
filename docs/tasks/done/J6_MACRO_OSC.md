<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J6: Macro Oscillator spike, X-3 (#73, PR #77)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "B2 / J6" and `docs/design/ports/PLAITS_STUDY.md` (§3, §8, Decisions). `ports.macro_oscillator`, 48 kHz only, per-channel 12-frame FIFO + `latency_frames`, up to 8 voices, all 24 models.

## Planner decision (2026-10-28): the upstream signed-shift UB
UBSan finds a left shift of a negative value in pinned upstream `plaits/dsp/fm/dx_units.h:79` (`Pow2Fast<2>`) under valid six-op controls. **Authorized:** a minimal, bit-preserving portability patch (do the shift on the unsigned representation) applied at fetch time through FetchContent `PATCH_COMMAND` from a patch file in the repo (`native/modules2/macro_oscillator/patches/`), recorded in `DESIGN.md` with the reason and the upstream SHA. A test proves the patched output is bit-identical to the unpatched Release build on models 2–4 (record the reference hashes). All 24 models stay selectable. No other upstream edits; any further UB finding is a new stop.

Also: fix the Windows build failure on #77 (logs are readable through the GitHub API or posted by the planner; see #83 for the network setting).

**Self-decisions (review later), 2026-10-29:** the line cap is1200
(original1000, +20%, within WORKFLOW section3c's +50%) to format this PR's
new C++ files and address pre-review findings. The16-file cap and all numeric
acceptance bounds stay unchanged. The private RNG header uses a C++ .hpp
extension so header-only analysis selects the correct language.

```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 1200 changed lines, <= 16 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_macro_oscillator_tests" --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers (no suppressions), cortex-m7 (report only), guard green on the exact head
  - pi-run: build/native/abi2_macro_oscillator_tests --benchmark   # label ci/pi-accept; 8 voices x 24 models at 48000/128, worst block < 25% of the period (666.7 us)
stops: [ABI edit, upstream edit beyond the authorized patch, new dependency, sanitizer suppression]
```
