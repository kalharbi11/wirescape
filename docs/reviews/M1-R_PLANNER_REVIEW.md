<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M1-R: Planner review of the ABI 2 foundation slice

Reviewer: Claude (planner). Date: 2026-10-27. Issue #2. Code reviewed: `main` at `3ca324c`, plus the PR #4 head `88859fb`, which adds a one-line Linux `fopen` fix to `native/tests2/module_tests.cpp`. The review package is `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`.

## Verdict
**No-go for M2 today. Go once M1-3 meets the conditions at the end.**
- The contract, scheduler and concurrency design are sound. They can stay as they are.
- The performance gap is real, but it's **framework overhead, not DSP or architecture**. Three specific inefficiencies explain it, and fixing them should bring mono under target without changing any DSP math.

## Evidence I collected
| What | Result |
|---|---|
| CI `linux-arm64` (ubuntu-24.04-arm, GCC 13), run [40441122062](https://github.com/kalharbi11/wirescape/actions/runs/40441122062) | **All 24 CTest suites pass on ARM64**, including `abi2_header_c99`, the NEON (`abi2_sdk_tests`) and scalar SDK builds, the no-exceptions/no-RTTI ports, the shim and integration tests, and the benchmark |
| ARM64 benchmark *numbers* | **Not read.** They're in the artifact `linux-arm64-summaries` → `build_reports/abi2/benchmark.json`, but this session's network policy blocks the artifact storage host. M0-4 prints them into the job summary. Until then, the user can download the zip from the run page |
| Local rebuild (x86_64, GCC 13, `-O3`) | 24/24 pass |
| ASan + UBSan (`-fno-sanitize-recover`), all 12 ABI 2 suites | **Pass, no reports.** This is the first sanitizer evidence; the package had Debug CRT only |
| TSan: engine, integration, module, SDK and voice suites | **Pass, no races**, including the concurrent-producer test |
| Wall-clock benchmark here, 5 runs | Mono ratio **1.16–1.60×** run to run. A shared VM is too noisy for a ±5% gate |
| **Callgrind instruction counts** (deterministic), per 256-frame block | ABI 1 mono **87.9k**; ABI 2 mono **137.9k (1.57×)**; ABI 2 modulated **309.5k**; ABI 2 poly8 **1,037.7k** vs 8 × mono 1,102.9k (**0.94×**) |

### Where the extra mono cost goes (callgrind, ABI 2 mono, per block)
ABI 2's VCF already *saves* about 28k instructions per block compared with ABI 1: it skips the per-sample `tan`, because the cutoff is constant. That makes the framework overhead even larger than the 1.57× suggests.

| Source | Approx. instructions per block | Where |
|---|---:|---|
| **Per-sample SDK accessors.** Each `b.in(0).value(c,i)` builds a `Signal` → `Channel` and runs two branches on every sample; the VCA then gathers lanes into stack arrays and scatters them back | **≈30k** | `native/sdk/wirescape_sdk.h:54,63-68`; `native/modules2/vca/dsp.h:14-18` |
| **Two engine copies per cable.** The module output is copied `local_out`→`global_out`, then copied again sample by sample into the destination's `in` buffer through a branchy lambda | **≈18k** | `native/engine2/engine2.cpp:444` and `:352-376` |
| **Redundant finite scans.** The SDK `isfinite`-scans every non-constant input and parameter before the DSP and every output after it. In engine2, an input *is* the upstream output that was just scanned | **≈13k** | `native/sdk/wirescape_sdk.h:196-212` |
| ABI 1 engine's own overhead, for comparison | ≈3.7k | `ws_engine_render` |

Removing about 60k of the 138k is plausible, which would land near **0.9× ABI 1**.

The modulated case adds about 68k of engine self-cost for the per-sample parameter pipeline:
- It scans *every graph edge* for every sample and every channel (`engine2.cpp:396`).
- It recomputes `log(min)`/`log(max/min)` inside `to_plain` for every sample (`wirescape_sdk.h:29`). That's ≈37k of `log`/`exp` per block.

## Findings by severity
Observed defects are marked **[defect]**. Design preferences are marked **[design]** and hypothetical risks **[risk]**.

### High
**H1 [defect] The mono throughput gate is missed: 1.57× (instruction count) and 1.89× (MSVC wall clock).** Trigger: any graph of cheap modules. Consequence: on the Pi 5, every small utility module costs about 1.5–2× more than it should. Fix (M1-3 Part A, in this order, measuring after each step):
1. **Zero-copy views.** In non-loop components, `out_view` points straight into `global_out`. `in_view` points at the source's `global_out` when no reduction is needed: a POLY port with a matching channel count, or a mono source broadcast by repeating one pointer. Copy only for `SUM`/`FIRST` reductions of more than one channel, back-edge delay reads and loop sub-spans (or use `global_out + start` there).
2. **Validate once, at the producer.** The SDK keeps the *output* check but makes it branchless and vectorizable: OR-reduce the exponent bits, then fall back to the precise per-sample check only on failure. Input and parameter scans become a guarantee of the host or adapter: engine2 inputs are, by construction, validated outputs or engine-generated finite parameter values. engine2 also validates ABI 1 shim outputs once. This needs no ABI change, only a D14/"Experimental limits" wording update in `ABI2_SPEC.md`.
3. **Branch-free SDK reads.** Fetch a channel's pointer and constant flag once per channel, never per sample. Add one SDK helper that returns a contiguous pointer for a channel, expanding a constant into a prepared per-instance scratch row, so inner loops are plain `for` loops the compiler can vectorize. Rewrite the VCA to loop over time per channel, which keeps mono 4-wide instead of using 1 of 4 lanes. Update the guide's VCA/template example to use the new idiom.
4. **Precompute at prepare.** Store per-node incoming signal edges, modulation edges per parameter, and outgoing event edges per port. Replace `find()` with an ID→index table. This removes every `for(e: edges)` scan from `process_node`, `route` and `collect` (`engine2.cpp:135,308,322,358,363,383,396,573`). Cache the taper constants per parameter.

Test: the existing equivalence (max error 0), the shim (bit-identical), zero-allocation and finite-guard tests must all still pass unchanged. Add a test proving non-finite DSP output is still caught after the change.

**H2 [defect] Benchmark method can't decide a ±5% gate.**
- `benchmark.cpp` runs each case once, in a fixed order, as one 4,000-block series on a shared machine. I saw 1.16–1.60× across five local runs.
- Fix: interleave the cases round-robin across ≥5 repetitions and report the median of per-repetition means.
- Also add a **deterministic instruction-count check**: a `tools/bench/abi2_cost.py` that runs a mono/poly probe under `valgrind --tool=callgrind` when valgrind is present. Valgrind supports aarch64. Run it in the sanitizer CI job and report Ir per block. Use wall clock for the user-facing claim and Ir for the CI gate.

### Medium
**M1 [defect] `wirescape_voice2` is a non-PIC static library linked into shared modules** (`native/engine2/CMakeLists.txt:17`).
- Trigger: any sanitizer build, or any shared module that uses the allocator (MIDI→CV in M4). The link fails with "relocation … recompile with -fPIC". Today it links only by luck.
- Fix: `POSITION_INDEPENDENT_CODE ON` on the target. Test: the sanitizer CI job builds it.

**M2 [risk] No sanitizer CI.** The package asked for ASan evidence and had none. I ran it once by hand. Fix: add a Linux `sanitizers` job with ASan+UBSan and TSan, running the ABI 2 suites. Make it required before M2 starts.

**M3 [defect, test gap] The concurrency tests don't race `panic()` or `pop_egress()` against `render()`** (`native/engine2/tests/engine_tests.cpp:128-133` races only `enqueue`). Fix: under TSan, add
- one thread enqueuing notes,
- one thread calling `panic()` at random intervals,
- one egress thread popping,
- the main thread rendering.

  Assert that `applied_generation` catches up, that no stale-generation event is delivered after its acknowledgement, and that no note stays gated after a panic.

**M4 [defect, test gap] Feedback is proven only for a signal self-loop.** Fix: add adversarial SCC tests:
- a 3-node signal loop with an event back-edge and a modulation back-edge in the same SCC;
- insertion-order permutations giving identical output hashes;
- an event emitted at the last frame of a span crossing the callback boundary through `future`;
- `feedback_block` = 1, 32 and the maximum block size.

**M5 [defect] The banned-include check runs only in `tools/bench/abi2_foundation.py:72`**, so CI never runs it. Fix: make it a CTest (a Python test under `tools/foundation/tests` is fine) that scans `native/modules2/**`, `native/sdk/**` and `native/voice/**`. Add `<map>`, `<unordered_map>`, `<list>`, `<deque>`, `<functional>`, `<sstream>`, `<fstream>` and `<chrono>`.

**M6 [design] Poly modulation into a parameter without `PER_VOICE` still produces a poly parameter signal** (`engine2.cpp:381-385`). A global parameter, such as a future reverb size, shouldn't split per voice. Decision: parameters without `PER_VOICE` are always 1 channel, and poly modulation contributes its channel 0 (FIRST). Implement in M1-3 with a test. This settles review question 3.

### Low
- **L1 [defect]** Several SDK early `WS2_INVALID` returns leave outputs untouched (`wirescape_sdk.h:186-193, 203-205`). engine2 zeroes them itself, but other hosts such as the CLAP adapter wouldn't. Silence the outputs whenever the output views are valid.
- **L2 [defect]** `route()` doesn't validate `value2` or `ump_words` (`engine2.cpp:314-315`), unlike `enqueue()` and the SDK. Share one event validator.
- **L3 [risk]** `publish_clock()` is public but changes anchors owned by the render thread without synchronisation (`engine2.h:56`, `engine2.cpp:525-533`). Document it as render-thread-only, or make it private.
- **L4 [risk, defer to M5]** Each `Packet` carries a fixed 1,472-byte payload, so every ring push or pop and every future-queue copy moves about 1.6 KB whatever the event size (`engine2.cpp:35-60`). That's fine for MIDI rates. Before high-rate OSC/UDP from Pico boards, copy only `bytes` (a payload slab).
- **L5 [design, M2]** `reconfigure()` recreates and re-prepares *every* node, up to three times each (`engine2.cpp:631-654`). That resets the DSP state of untouched modules (oscillator phase, filter memory) and could click when one CC Map's rows change. M2's staged replacement should keep untouched instances.
- **L6 [design, M2]** Latency includes event and modulation edges and is a single global maximum (`engine2.cpp:265`). Settle it before adapters; see decision 5 below.

## What stays as it is
- **The contract** (`module2.h`):
  - D1–D15 are all represented.
  - Every struct starts with `struct_size`, and trailing growth is the extension path.
  - The header compiles as C99, now on MSVC and GCC/ARM64.
  - Nothing locks in a choice the plan left open.
  - The host owns allocations (there is no `free`). See decision 11.
- **The SDK shape:**
  - a plain DSP class plus the generated C glue;
  - validation and the exception barrier in one place;
  - only `<array>`/`<algorithm>` in headers, with no allocation reachable from process;
  - little-endian state helpers;
  - the VCF goes from 95 lines to 36 + 3.
- **The scheduler:**
  - Tarjan SCCs, with back-edges chosen by a DFS over ID-sorted nodes and tuple-sorted edges, independent of insertion order;
  - loop components in `feedback_block` spans;
  - back-edge delay of exactly `feedback_block` (read before write);
  - event back-edges deferred by `feedback_block` through `future`.
- **The parameter pipeline order** matches the spec: smooth in normalized space → add offset and Σ(amount·cv) → clamp → taper → step. Constant detection, including the "actually constant" check, is correct.
- **Concurrency:**
  - The SPSC ring uses correct acquire/release pairs.
  - The producer registry is fixed before `prepare()`.
  - `panic()` advances the generation immediately; reset and acknowledgement happen at render boundaries.
  - `enqueue()` refuses work until the acknowledgement.
  - Events are ordered `(frame, producer<<48 | serial)` across the critical and normal rings, so a note-on/note-off pair from one producer can't swap.
  - Bounded drains, release reserve, latest-wins coalescing, the message arena bounds check and `memory_requirements` enforcement (`engine2.cpp:198`) are all sound.
  - TSan is clean.
- **The voice allocator** uses fixed storage and is tested. **The ABI 1 shim** is bit-identical.
- **Readiness for later milestones:**
  - `query_ports(config)` with staged re-query supports CC Map and OSC rows (the `rows_module` test).
  - Event outputs and egress (node, port, frame, mapped time) support MIDI Out.
  - LARGE memory and state support tape, loop and space modules. Large media belongs in referenced asset files, not in the 1 MiB state blob.

## Decisions to record in `ABI2_PLAN.md` section 9 (M1-3 writes the rows)
1. **Release reserve and emergency.** Accept the finite release reserve and global emergency reset/choke, with fan-out counted against the budget. Add a per-source emergency scope only if a measured case needs it. Counters must show in the Activity panel (M4).
2. **Timestamps.** Accept prepared-`max_block` lookahead plus the bounded EWMA (±1000 ppm) as the basis. Make no "sample-accurate" claim until real MIDI timing is measured (M4).
3. **PER_VOICE.** Parameters without `PER_VOICE` are always mono; poly modulation contributes channel 0 (M6 above). Adapters resolve `note_id` → voice channel before submitting PARAM_MOD; the engine keeps addressing by channel.
4. **Optimisation order.** Scheduler/view caching and zero-copy first, then single-point validation, then SDK read helpers and the VCA. DSP vectorisation across voices (VCF, VCO) comes only after these, aimed at the poly ratio.
5. **Latency.** Count only *signal* edges that reach an output bus, reported per bus. Show feedback delay as a property of the loop on back-edge markers, never summed into forward latency. Implement in M2, before any adapter.
6. **Adversarial tests gating Track C.** The M3 and M4 tests above, plus the M4-entry proofs in decision 9.
7. **Caps.** Accept a 1,472-byte payload, a 1 MiB state blob, 128 MiB per allocation and 64 allocations per node. Revisit for the CHAOS tape in M7. Exporters budget DSP memory from `memory_requirements`, not from desktop queue sizes.
8. **Queries.** Accept query-before-cabling on a created but unprepared instance. `query_ports` MUST be side-effect free and cheap, and `create` MUST NOT allocate LARGE memory. The SDK already satisfies this.
9. **Gate before real MIDI/OSC (M4 entry):**
   - a fake-sink dispatch-scheduling test;
   - timestamp perturbation (±1 ms jitter, ±500 ppm drift);
   - producer stop and shutdown ordering;
   - a libremidi loopback (no hardware);
   - only then a hardware run, with the user's authorisation.
10. **ARM toolchain.**
    - GCC 13 on aarch64 Linux is now proven for build and tests.
    - Pi 5 timing comes from the optional self-hosted runner (M2).
    - The first thin adapter is CLAP (M8).
    - Daisy (arm-none-eabi-gcc, Cortex-M7, scalar `f4`) is a build-only proof in M8.
11. **Memory ownership.**
    - The host frees all of an instance's allocations at `destroy`.
    - An instance is prepared once. A rate or block change means destroy, create, prepare, then `load_state`.
    - A future `free` callback would be a trailing `ws2_host` field.
12. **Input validation.** Hosts and adapters deliver finite inputs and parameters. The SDK validates outputs (branchless). engine2 validates ABI 1 shim outputs (H1 step 2).

## Go/no-go for M2
**No-go now.** M2 may start when the M1-3 PR is merged with all of these true:
1. **Mono:** ABI 2/ABI 1 mean ≤ **1.05×**:
   - on the `windows-x64` CI runner (MSVC Release, median of 5 interleaved repetitions; the user may confirm on the laptop);
   - on the `linux-arm64` CI runner (same method);
   - and callgrind Ir ≤ 1.05× on Linux.
2. **Poly:** report poly8 / eight-mono on both runners. It must be ≤ **1.00×**: a poly graph is never slower than separate mono graphs. Part A removes per-graph overhead, which helps eight separate graphs more than one poly graph, so the ratio may rise toward 1.0. Voice-SIMD work (decision 4) lowers it later.
3. **Modulated:** engine self-cost of the parameter pipeline per block drops by at least half from the 68k-Ir baseline. Report the modulated/unmodulated ratio.
4. A sanitizer CI job (ASan+UBSan, TSan) is green and required.
5. The M3 and M4 tests exist and pass. The banned-include CTest (M5) runs in CI.
6. No regressions: equivalence max error 0 on defaults, a bit-identical shim, zero allocations, finite guards intact, and every existing test unchanged.
7. Spec text updated for the validation rule, the PER_VOICE rule and the section 9 rows.

M1-2 (the capture harness) is independent and may continue in parallel. Track C (M4) stays closed until M2.
