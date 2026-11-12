<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-h: OSC/UDP codec fuzzing (after J5 merges) (#94)

**Start now** (Keel, 2026-11-12): the codec and matcher (`native/io/osc/codec.h`, `pattern.h`) have been on `main` since slice A; the fuzzers don't wait for #72 to close. A new CI job under `.github/workflows/` needs Keel's approval (CI policy). **Do:** libFuzzer targets (clang `-fsanitize=fuzzer,address,undefined`) for the OSC packet parser and address matcher, seeded from J5's corpus. CI: 60 s per target on PRs touching `native/io/osc/` (non-required). Pi nightly: 30 min per target; crashes are minimized and added to the corpus test, and an issue is opened.

**Revised by Coda, 2026-11-12 (senior, inside phase).** The first must_pass line was prose, so `accept 94` could never pass. It is now the runnable commands below. Coda ran them on head `b10c244` with clang 18 + libFuzzer (x86-64, ASan+UBSan): the codec target ran 32.2M executions in 60 s and the pattern target 1.3M in 60 s, with zero findings. The **CI job (60 s per target on PRs touching `native/io/osc/`) and the Pi nightly (30 min per target)** need a protected workflow and a CI-policy ruling, so they moved to the follow-up #676 (Keel rules on it). This PR adds no workflow. The fuzzers write new units and crash artifacts only under `build/fuzz/`, never into `native/io/osc/fuzz_corpus/`. If the runner lacks the libFuzzer runtime (`libclang-rt-18-dev`), report that as an environment gap. It is not a test failure, and the lines must not be weakened.

**Corpus SPDX policy (Keel, 2026-11-12):** codec and pattern seeds are SPDX-headed ASCII hex; the acceptance commands strip comment lines and decode the actual bytes into `build/fuzz/seed/` before libFuzzer reads them. No binary corpus files are committed.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 250 changed lines, <= 6 files
must_pass:
  - cmake -S native -B build/fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DWIRESCAPE_OSC_FUZZ=ON && cmake --build build/fuzz --target abi2_osc_codec_fuzz abi2_osc_pattern_fuzz
  - python3 -c "import pathlib as p; d=p.Path('build/fuzz/seed/codec'); d.mkdir(parents=True, exist_ok=True); [(d/(f.stem+'.bin')).write_bytes(bytes.fromhex(''.join(x for x in f.read_text().splitlines() if not x.lstrip().startswith('#')))) for f in p.Path('native/io/osc/fuzz_corpus/codec').glob('*.hex')]"
  - build/fuzz/abi2_osc_codec_fuzz -max_total_time=60 -artifact_prefix=build/fuzz/ build/fuzz/seed/codec
  - python3 -c "import pathlib as p; d=p.Path('build/fuzz/seed/pattern'); d.mkdir(parents=True, exist_ok=True); [(d/(f.stem+'.bin')).write_bytes(bytes.fromhex(''.join(x for x in f.read_text().splitlines() if not x.lstrip().startswith('#')))) for f in p.Path('native/io/osc/fuzz_corpus/pattern').glob('*.hex')]" && build/fuzz/abi2_osc_pattern_fuzz -max_total_time=60 -artifact_prefix=build/fuzz/ build/fuzz/seed/pattern
  - ctest --test-dir build/native -R "osc" --output-on-failure      # corpus test still green
  - ci: linux-arm64, sanitizers, guard green
stops: [ABI edit, new dependency other than clang's libFuzzer, a workflow file in this PR (that is #676)]
```
