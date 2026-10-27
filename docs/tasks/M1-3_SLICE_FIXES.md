<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M1-3: ABI 2 slice fixes and the performance gate

Branch `task/<issue#>-slice-fixes`, draft PR. Follow AGENTS.md. Agent: Codex, Sol Medium. Sol High is allowed only for Part A, after two failed attempts. Size: l.

**Read first:**
1. `docs/reviews/M1-R_PLANNER_REVIEW.md`: the findings, the profile and the go/no-go conditions. **That review is the spec for this task.** Finding IDs (H1, M3, …) refer to it.
2. `native/engine2/engine2.cpp`, `native/sdk/wirescape_sdk.h`, `native/modules2/vca/dsp.h`, `native/engine2/tests/benchmark.cpp`, `native/engine2/CMakeLists.txt`.
3. `docs/specs/ABI2_SPEC.md` (sections D7, D14 and "Experimental limits").

**Explicitly allowed protected paths:**
- `docs/specs/ABI2_SPEC.md` (wording for the validation and PER_VOICE rules)
- `docs/specs/ABI2_PLAN.md` (section 9 rows only)
- `.github/workflows/ci.yml` (adding the `sanitizers` job only, **after M0-4 (#5) is merged**; merge `main` into the branch first)

`native/include/wirescape/module2.h` is **not** allowed. No ABI change is needed. If you find you need one, stop and post a `status/blocked` handoff.

**Size check:** expect about 600–800 changed lines across about 15–20 files. That is pre-approved. Stop and post a handoff if you exceed 1,000 lines.

## Ground rules (non-negotiable)
- **No DSP math changes.** Keep the double IIR state and the exact `tan`. Equivalence stays at max error 0 on defaults. The ABI 1 shim stays bit-identical.
- Zero allocations in render, process and reset stay proven by the existing tests.
- Finite guards stay. Only *where* the check happens moves (H1 step 2).
- No tolerance, threshold or test removal. Every existing test passes unchanged.
- **Measure after each Part A step**, using callgrind Ir per block. Record the numbers in the PR description table.

## Part A: performance (H1, H2)
Commit each step separately, in this order.
1. **Benchmark method (H2):**
   - `benchmark.cpp` interleaves the seven cases round-robin over 5 repetitions of 800 blocks and reports the median of the per-repetition means per case. Keep the JSON row names, adding `repetitions` and per-repetition means.
   - Add `tools/bench/abi2_cost.py`. It builds or uses the probe and runs the `abi1_mono`, `abi2_mono`, `abi2_modulated` and `abi2_poly8` graphs under `valgrind --tool=callgrind` at two block counts. It reports instructions per block from the difference, and exits 0 with "skipped" when valgrind is absent.
   - The probe is a small executable in `native/engine2/tests/` taking `<modules_dir> <mode> <blocks>`.
2. **Precompute at prepare (H1.4):**
   - Per-node lists of incoming signal edges per input, modulation edges per parameter and outgoing event edges per output port.
   - A node ID → index table, which replaces `find()` in `collect()` and the bus loop.
   - Cached taper constants per parameter: `log(min)`, `log(max/min)` and `expm1(curve)`. The values must be identical (same double expressions, just hoisted).
   - No `for(e: edges)` remains in `process_node`, `route` or `collect`.
3. **Zero-copy views (H1.1):**
   - In non-loop components, `out_view` writes straight into `global_out`.
   - `in_view` aliases the source's `global_out` for POLY ports and mono broadcast (one pointer repeated).
   - Copies remain only for `SUM`/`FIRST` with source channels > 1, back-edge delay reads, unpatched constants and loop sub-spans.
   - The ABI 1 shim path may keep copying.
4. **Validate once (H1.2):**
   - SDK `process` no longer scans inputs or parameters.
   - The output check becomes branchless: OR or max-reduce `bits & 0x7f800000` over each channel, then a precise rescan only when an exponent of 0xFF is seen. On failure, behaviour stays as today: silence plus `WS2_INVALID`.
   - engine2 checks ABI 1 shim outputs once per block and replaces a non-finite block with silence plus a fault count.
   - Add a test: a deliberately NaN-producing ABI 2 module is still silenced, and the fault counter still increments.
5. **Branch-free SDK reads (H1.3):**
   - Add `ws::Signal::row(c, scratch)`, or an equivalent name, returning `const float*` to `frames` contiguous samples. A constant is expanded into a per-instance scratch row of `max_block` floats, reserved in `prepare` and counted in `memory_requirements`.
   - Rewrite the VCA as a per-channel time loop over `f4`, with a scalar tail. Its output must stay bit-identical to today's VCA for all matrix cases. Multiplication order is unchanged: `audio*cv*gain`.
   - Update `docs/SDK_AUTHOR_GUIDE.md` and `native/sdk/template/` to show the new idiom.
6. **Parameter pipeline:**
   - When a parameter has modulation, compute `Σ amount·cv` per channel over the span using the precomputed edge list, then add the base and offset and apply the taper.
   - No per-sample edge scans.
   - Keep double arithmetic for exact equivalence.

## Part B: correctness, tests and CI (M1–M6)
1. **M1:** set `POSITION_INDEPENDENT_CODE ON` for `wirescape_voice2`.
2. **M2:** add a `sanitizers` job to `ci.yml` on `ubuntu-24.04-arm`.
   - Two steps: an ASan+UBSan build (`-fno-sanitize-recover=undefined`) and a TSan build, both `RelWithDebInfo`, headless, `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`.
   - Each runs `ctest -R abi2` (TSan may skip `abi2_benchmark`).
   - The same job runs `tools/bench/abi2_cost.py` after `apt-get install -y valgrind` and appends the Ir table to `$GITHUB_STEP_SUMMARY`.
   - It runs on every PR and on pushes to `main`, with a 30-minute timeout. The handoff tells the user to add `sanitizers` to the required checks.
3. **M3:** a TSan-run stress test with a note producer, a `panic()` thread, an egress-pop thread and the render loop, bounded to about 2 seconds. Assert:
   - the acknowledgement catches up;
   - nothing from a stale generation is delivered after its acknowledgement;
   - no voice is left gated after a panic and a render.
4. **M4:** adversarial feedback tests, as listed in the review: a 3-node mixed SCC, insertion-order permutation hashes, an event crossing a callback boundary from the last frame of a span, and `feedback_block` ∈ {1, 32, max_block}.
5. **M5:** a banned-include test in CI covering `native/modules2/**`, `native/sdk/**` and `native/voice/**`, with the list extended as in the review.
6. **M6:** parameters without `PER_VOICE` are always 1 channel, and poly modulation contributes channel 0. Tests cover both a poly LFO into a non-PER_VOICE parameter and a PER_VOICE parameter.
7. **L1** (SDK silences outputs on the early-invalid paths when the views are valid), **L2** (one shared event validator for `enqueue`, `route` and the SDK) and **L3** (document `publish_clock()` as render-thread-only in `engine2.h`). Each comes with a test where it's testable.

## Part C: docs
- `docs/specs/ABI2_SPEC.md`:
  - D14 and "Experimental limits": the single-point validation rule.
  - D7: the PER_VOICE rule.
  - D13: `publish_clock` threading.
- `docs/specs/ABI2_PLAN.md` section 9: add rows for decisions 1–12 from the review, each with a short reason, dated on the day you write them.
- `docs/ARCHITECTURE.md`: two or three lines on zero-copy views and the sanitizer job.

## Acceptance (this is the M2 gate; all must be true)
- **Mono:** ABI 2/ABI 1 median ≤ 1.05× on the `windows-x64` and `linux-arm64` CI runners, and callgrind Ir ≤ 1.05×.
- **Poly:** poly8/eight-mono reported on both runners, and ≤ 1.00×. It may rise from today's ≈0.91–0.94× because Part A helps eight separate graphs more; that's expected.
- **Modulated:** engine self-cost in the modulated probe is at most half the 68k-Ir baseline. Report the modulated/unmodulated ratio.
- `linux-arm64`, `windows-x64`, `guard` and the new `sanitizers` job are all green.
- Equivalence is still max error 0, the shim is bit-identical, zero allocations hold, and the poly synth WAV hash is unchanged. If it changes, explain exactly why in the handoff and stop for review.
- The PR description has a before/after table for every probe (Ir per block) and for both runners (median ns).
- The handoff comment uses the AGENTS.md template. Label the issue `status/review`. Never merge.

## Out of scope (put ideas under Follow-ups)
- DSP vectorisation across voices for the VCF and VCO.
- The latency rework (M2).
- Keeping untouched instances in `reconfigure()` (M2).
- The `Packet` payload slab (M5).
- Patch v2, the UI, Track C, and any new module.
