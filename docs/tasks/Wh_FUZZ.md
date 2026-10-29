<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-h: OSC/UDP codec fuzzing (after J5 merges) (#94)

**Wait for:** J5 (#72) merged. **Do:** libFuzzer targets (clang `-fsanitize=fuzzer,address,undefined`) for the OSC packet parser and address matcher, seeded from J5's corpus. CI: 60 s per target on PRs touching `native/io/osc/` (non-required). Pi nightly: 30 min per target; crashes are minimized and added to the corpus test, and an issue is opened.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 250 changed lines, <= 6 files
must_pass:
  - fuzz targets build with clang and run 60 s each without findings
  - ctest --test-dir build/native -R "osc" --output-on-failure      # corpus test still green
  - ci: linux-arm64, sanitizers, guard green
  - pi: 30 min per target in the nightly, zero crashes
stops: [ABI edit, new dependency other than clang's libFuzzer]
```
