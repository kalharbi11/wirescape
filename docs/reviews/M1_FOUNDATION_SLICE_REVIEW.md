# ABI 2 foundation slice — Claude review gate

> Relocated during the GitHub migration (M0-1): the spec is now `docs/specs/ABI2_SPEC.md`, the guide `docs/SDK_AUTHOR_GUIDE.md`, the snapshots `docs/reviews/`. File lists below show the original pre-migration paths. Links to `build_reports/` refer to local, untracked evidence.

October 26, 2026. Implementation checkpoint: **34b948e1d1404e6108ea3ceeec3ee951959e1c4a**. This review and final evidence-collector / CTest benchmark wiring additions are a following checkpoint; `git log -1` identifies that final commit. Nothing was pushed. **STOP: do not begin Track C, additional ports, UI/patch migration or adapters until Claude reviews this slice through the user.**

The isolated foundation is implemented and functional offline acceptance passes. It is **not accepted for product migration or performance**: mono throughput misses the requested ±5% target, ARM is unverified, and the experimental semantics below require review. The existing app remains on ABI1.

## Read order and preservation

Read [spec](../specs/ABI2_SPEC.md), [SDK author guide](../SDK_AUTHOR_GUIDE.md), [communication log](../history/CLAUDE_CODEX_COMMUNICATION.md), then module2.h, SDK, engine2 scheduler/ingress, allocator and test drivers. The adopted [ABI2 plan](../specs/ABI2_PLAN.md) and [communication plan](../specs/COMMUNICATION_MODULES_PLAN.md) remain the parent direction; the pasted foundation-slice instruction sets the hard stop.

All **92 protected source hashes match** [baseline manifest](ABI2_BASELINE_MANIFEST.json): module.h, engine.h, engine.cpp, native/modules/** and native/ui/**. No user_data, Surge binaries or old patch format were changed. Baseline tag and private backups remain intact. The only preexisting build-source edit is three root native/CMakeLists.txt lines adding the isolated engine2 subdirectory; existing targets/logic are unchanged. START_HERE's two-line Claude edit and Claude's two supplied plans were adopted, not replaced.

## What was built

- Experimental C99-compatible module2 header with versioned C entry, struct_size, planar signals, event ports, parameter metadata, transport, memory, state, query and latency callbacks.
- Original MIT C++17 SDK: generated callbacks/validation/exception boundary, constant-aware views, event splitting, ramp/Schmitt/trigger/PCG, little-endian state helpers, own SSE2/NEON/scalar f4 and bounded-domain tan approximation. No approximation is used by the ports.
- Offline C++ engine2: signal/event/modulation graph, deterministic SCC scheduling and back-edge delay, normalized parameter pipeline, fixed producer registry/queues, fake clock/drift mapping, bounded message arena, generation acknowledgement, release reserve/emergency reset, stereo buses and ABI1 shim.
- Original MIT fixed-storage voice allocator; GPL SDK ports for VCO/LFO/VCF/ADSR/VCA. Only ws2_ binaries use these identities. The ABI1 app loader/package continues using ws_ binaries.
- notes_to_cv and config-dependent rows test modules; independent Ring Mod authoring exercise; CTest targets and a deterministic Python evidence collector.

## Actual evidence

Final whole-project `python tools/dev.py check --package`: **PASS**, `build_reports/foundation/check-20261026T230043Z.json`. 27 Python tests and **28 CTest suites** (16 existing +12 ABI2), UI smoke at both sizes, startup, existing45-case offline benchmark/render and portable package passed. Audio remained off. The report records the dirty precommit source fingerprint and implementation HEAD plus dirty final test-wiring changes because testing preceded the final checkpoint; it must not be presented as a clean-HEAD provenance record.

Frozen numeric snapshot: [ABI2_ACCEPTANCE_SNAPSHOT.json](ABI2_ACCEPTANCE_SNAPSHOT.json). ABI2-specific `build_reports/abi2/summary.json` records commands, return codes, logs, WAV hashes/stats, new binary hashes/exports, baseline verification and the **separate failed performance target flag**. Reproduce after Release build and the Debug target build with `python tools/bench/abi2_foundation.py`. It performs no install/download/device action. Raw reference WAVs and immutable hashes live in native/tests/fixtures/abi2/. Fresh render bytes match those fixtures.

| Test | Result | Exact evidence / bounds |
|---|---|---|
| A equivalence | PASS within tested scope | All five mono ports, defaults/unmodulated, 3 rates × blocks1/17/256: max absolute error **0** each. Raw Six Essentials graph shim at48k/256 is bit-identical for steady and four scripted PARAM changes; no app0.2 gain. |
| B matrix | PASS | 135 configurations: five ports × channels1/4/16 × blocks1/17/256 ×44.1/48/96k. Two instances, three blocks, reset/repeat sample hash; all finite and deterministic. |
| C allocations | PASS instrumented scope | Static SDK/DSP process/reset and engine ingress/render/reset/channel1→16→3 count zero C++ new/new[]/aligned/nothrow allocations. Poly synth also counts zero. Not a malloc interposer for arbitrary DLL code. |
| D constants | PASS | CONSTANT propagation, unpatched values and constant-aware guide tests. VCF direct counter4 for four voices first block, zero new calculations next block; changing mono cutoff adds4. |
| E modulation | PASS | ±amount, upper clamp, two summed cables, log taper, poly CV driving non-PER_VOICE parameter, normalized240-sample ramp at48k; sources capped by prepared channels. |
| F poly rules | PASS | Mono broadcast, SUM, FIRST, changing channels and zero-channel-as-unpatched. Zero override freezes source state. |
| G events | PASS tested scenarios | Three-producer same-frame fan-in, same-block emit/egress, stable ties, release reserve/coalescing/emergency reset, arena overflow,100k PARAM events over480k simulated frames/10s, plus actual concurrent writers/render. Immediate/fake timestamp mapping tested. |
| H feedback | PASS signal-loop proof | Self-loop exact32/1-frame delay, insertion-order equality, outside-loop node gets full128-frame block. Subnormals flush to zero as intended. Complex mixed event/modulation cycles need more adversarial tests. |
| I allocator | PASS | Rotate/Reuse/Reset/Mono priorities/legato, oldest steal, identity/source, sustain/source release, bounded exhaustion. MPE stub only. |
| J poly synth | PASS | Three notes, eight later notes with steals, LFO→cutoff modulation, eight gates/envelopes, VCO→VCF→VCA, identical stereo buses. Pitch error max2.6706342e-05 cents; first attack sample and480th peak verified; releases exactly quiet by frame57599. |
| K rows/state | PASS | Three→two rows keeps row_0 cable, reports one removed row_2 cable, byte-exact20-byte known state, opaque future version retained. |
| L generation | PASS | Panic invalidates enqueue until render acknowledgement; explicit stale generation rejected, processors quiet after reset. |
| M portability | PASS desktop subset; ARM NOT RUN | Header compiled as C with MSVC; C++17 SDK/five ports build/run with RTTI/exceptions off; scalar VCA/f4 matches independent multiply reference within1e-6. Forbidden DSP includes absent. No existing inspected ARM toolchain; no installation attempted. GCC/Clang/NEON/Daisy unverified. |
| N performance | MEASURED; mono target MISSED | Tables below. Mean ABI2 mono/ABI1=1.890×; poly8/eight ABI2 mono=0.913×. Not an optimized/runtime-stability claim. |

Engine tests also pass **Debug CRT heap integrity and before/after heap balance**, including threaded shutdown with joined producers. Initial16-byte difference was the debug iterator proxy of an eagerly initialized unused static empty vector; moving fallback construction to the invalid-ID branch removed it. No ASan claim. Logs: acceptance-0.log (CTest), acceptance-1.log (Debug CRT); benchmark runs inside CTest under build_reports/abi2/.

Synth artifact: `build_reports/abi2/poly_synth.wav` and `build_reports/abi2/poly_synth.json`:96k stereo frames, peak4.873094, RMS0.852157, release tail0, faults0. Raw voice sum can exceed1: this file is for analysis, without a limiter or normalization; do not mistake the raw mix for a mastered output. No physical loopback/audibility was tested by this slice.

## Benchmarks

48kHz,256 frames,256 warmup +4,000 measured calls per case, same Release compiler on this laptop. Times are microseconds. Worst is the largest observed desktop scheduling sample, **not a hard upper bound**. The benchmark order is fixed and no statistical confidence interval is claimed.

| Case | Mean µs | p99 µs | Observed worst µs |
|---|---:|---:|---:|
| abi1_mono | 5.693 | 6.700 | 22.600 |
| abi2_mono | 10.759 | 14.100 | 80.800 |
| abi2_poly8 | 76.922 | 90.500 | 202.400 |
| abi2_eight_mono | 84.266 | 114.300 | 1014.600 |
| abi2_modulated | 18.200 | 25.300 | 91.900 |
| vcf_constant | 4.246 | 4.500 | 38.400 |
| vcf_audio_rate_cutoff | 4.854 | 4.700 | 875.100 |

The first five cases measure complete graph rendering. abi2_modulated adds LFO plus normalized cutoff cable; it is not an isolated filter comparison. Last two rows measure only actual SDK VCF callbacks with the same audio input, constant versus varying per-sample cutoff. No fast-math/approximate DSP or quality reduction was used. Mono ±5% is missed. Likely investigation targets are repeated buffer/view/finite validation, graph edge scans and per-sample modulation work; these are **hypotheses, not a profiler result**. VCA currently gathers four lanes to stack arrays; other voice-heavy ports retain scalar math. Release LTO/PGO was not enabled for these isolated targets. Request a bounded profiler/optimization pass after architecture review rather than relaxing the threshold.

## Equivalence and author ergonomics

Default mono/unmodulated comparisons have zero measured differences for VCO, LFO, VCF, ADSR and VCA. This does not establish equality for arbitrary automated patches: normalized host smoothing and float parameter signals replace per-module double physical-unit ramps; logarithmic cutoff smoothing intentionally differs. VCO connected pitch now means1V/oct with Frequency/220 tuning ratio; explicit pitch wins over event frequency. ADSR connected gate wins over notes and uses Schmitt thresholds. See each port README for precedence and limits. Source values/parameter identities retain ABI1 meanings for the unconnected/default path.

VCF source comparison: native/modules/vcf/module.cpp **95 lines**; native/modules2/vcf/dsp.h **36 lines** + module.cpp **3 lines**. The SDK is237 lines and shared helpers are separate. Fewer module lines are partly compact formatting, not proof of lower conceptual complexity. Double IIR state, exact tan and the old tiny-tail rule are retained. Engine owns smoothing/modulation; wrapper owns boundary checks.

Independent guide exercise: native/tests2/guide_ringmod/. The worker used the guide/template for a poly A*B*Level processor and real generated callbacks, rates/channels/frames, constants and poisoned-array checks. It found (1) no complete test bootstrap initially and (2) unclear ch(c) value-view semantics; a template tests.cpp bootstrap and explicit guide explanation were added. Module JSON indices versus default values were also clarified. CTest passes. This was the same workspace/SDK, not a blind external author usability study.

## Deviations / clarifications from the parent plan

1. Minimal root CMake subdirectory wiring was required to make CTest/dev.py run new targets. It is the only preexisting build-source change, aside from adopting Claude's existing entry-point edit. Test WAV fixtures use the expressly requested native/tests/fixtures/abi2 location and the requested benchmark collector lives in tools/bench.
2. Finite queues cannot guarantee unbounded lossless NOTE_OFF: reserved capacity plus countered emergency generation reset/global CHOKE is implemented; this needs Claude acceptance. The global budget counts event emission and each destination admission, so fan-out consumes capacity.
3. Timestamped offset is **prepared max_block**, not feedback_block or variable actual callback length. Slope drift filtering is bounded EWMA on fake anchors. No jitter-free driver promise.
4. MPE, MIDI protocol decoding, sample-accurate transport-event interpretation and real dispatchers remain reserved. PARAM_MOD uses explicit voice channel; note_id resolution belongs to a future adapter. Stereo is two ports/bus sides; summing voices has no normalization.
5. Own f4 wrapper is present, but only VCA uses explicit vector arithmetic. VCO/LFO/VCF/ADSR group voices by4 while retaining scalar precision math. Only the bounded-domain tan approximation is included; other approximations/f8 are not part of this task's narrowed deliverable.
6. Per-module tests are centralized in native/tests2/module_tests.cpp rather than duplicated tests.cpp files in each port folder. Module README/DESIGN/json/licence and actual ws2_ DLLs are provided. Shared matrix/fixtures reduce drift.
7. The engine is offline/test-only. New live app/patch support, migrations, any additional Essentials, hardware/plugin adapters, Track C and D16 host features were deliberately not started.
8. Latency meter currently reports maximum scheduled dependency path across all nodes, including event dependencies, not only output-reaching audio paths. No compensation. This needs tightening before adapter use.
9. Schema/example parse and basic structure checks pass; a full JSON Schema validator was not installed, and relational graph validation is not implemented. The draft is not executable app input.
10. Mono performance target and Release LTO direction are not met; ARM/GCC/Clang/NEON build evidence absent. Numeric equivalence takes priority over silently substituting faster approximate DSP.

## Questions for Claude (review first; no automatic follow-on work)

1. Accept the finite release reserve/emergency reset contract, including global rather than per-source emergency scope and budget accounting across fan-out?
2. Accept prepared-max-block timestamp lookahead and bounded fake-clock EWMA as the basis for later real-driver measurements?
3. Should PER_VOICE become an enforced host policy, and how should adapter-resolved note identity interact with PARAM_MOD channel addressing?
4. Prioritize scheduler/view caching, boundary validation or DSP vectorization first after profiling the1.890× mono result? Keep finite guards and equivalence thresholds intact.
5. Tighten latency to output-reaching signal paths before adapters? How should explicit feedback latency be displayed versus accumulated forward latency?
6. What adversarial mixed signal/event feedback and concurrent panic/drain tests should gate Track C? Current signal self-loop proof is narrower than all possible SCCs.
7. Are payload1472,1MiB state,128MiB allocation caps and up to64 requests/node appropriate before large-memory CHAOS/loopers? Exporters must budget host-independent DSP state, not desktop queues.
8. Accept the query-before-cable-authoring / staged re-query policy, or require a side-effect-free query interface separate from instance construction?
9. What smaller proof should gate a real MIDI/OSC source after this review: fake sink dispatch scheduling, timestamp perturbation, producer stop ordering, then actual hardware? Sequencers should reuse the same allocator/event semantics.
10. Which exact ARM toolchain/target and thin adapter should be validated first? Desktop C++17/source separation alone does not prove unchanged Daisy firmware compilation.

## Staffing and recoverability

Coordinator used the inherited user-selected setting; one focused worker at a time. Sol Medium worker owned original allocator/tests and the later guide exercise; Luna Medium worker drafted spec/schema, then coordinator corrected draft-to-code mismatches. No High/Ultra/Max escalation was used. Runtime/tool telemetry does not provide a per-task token cost; no token/quota totals are invented. All workers are stopped/completed; an unused historical pending worker was interrupted. No future automation was created.

The code checkpoint is 34b948e1d1404e6108ea3ceeec3ee951959e1c4a. Final documentation checkpoint contains this review, current checkpoint/communication entry, the DLL-export evidence collector and CTest benchmark wiring. Tests ran before commits; use summary binary hashes and protected hashes rather than treating commit timestamps as test provenance. The user manually brings this file to Claude. Do not message another chat or continue implementation automatically.

## Added file tree and implementation diffstat

All additions in the implementation checkpoint plus this review are listed below. The final documentation checkpoint also updates BUILD_CHECKPOINT, the communication log and START_HERE. The adopted Claude plans account for754 lines of the implementation diff, and repeated full GPL licence files account for3,370; these are not all new DSP.

```text
docs/ABI2_BASELINE_MANIFEST.json
docs/ABI2_PLAN.md
docs/ABI2_SPEC.md
docs/CLAUDE_CODEX_COMMUNICATION.md
docs/COMMUNICATION_MODULES_PLAN.md
docs/FOUNDATION_SLICE_REVIEW.md
docs/ABI2_ACCEPTANCE_SNAPSHOT.json
docs/SDK_AUTHOR_GUIDE.md
docs/patch_v2_draft.example.json
docs/patch_v2_draft.schema.json
native/engine2/CMakeLists.txt
native/engine2/engine2.cpp
native/engine2/engine2.h
native/engine2/tests/benchmark.cpp
native/engine2/tests/engine_tests.cpp
native/engine2/tests/header_c99.c
native/engine2/tests/sdk_tests.cpp
native/include/wirescape/MODULE2_LICENSE.txt
native/include/wirescape/module2.h
native/modules2/adsr/DESIGN.md
native/modules2/adsr/LICENSE
native/modules2/adsr/README.md
native/modules2/adsr/dsp.h
native/modules2/adsr/module.cpp
native/modules2/adsr/module.json
native/modules2/lfo/DESIGN.md
native/modules2/lfo/LICENSE
native/modules2/lfo/README.md
native/modules2/lfo/dsp.h
native/modules2/lfo/module.cpp
native/modules2/lfo/module.json
native/modules2/vca/DESIGN.md
native/modules2/vca/LICENSE
native/modules2/vca/README.md
native/modules2/vca/dsp.h
native/modules2/vca/module.cpp
native/modules2/vca/module.json
native/modules2/vcf/DESIGN.md
native/modules2/vcf/LICENSE
native/modules2/vcf/README.md
native/modules2/vcf/dsp.h
native/modules2/vcf/module.cpp
native/modules2/vcf/module.json
native/modules2/vco/DESIGN.md
native/modules2/vco/LICENSE
native/modules2/vco/README.md
native/modules2/vco/dsp.h
native/modules2/vco/module.cpp
native/modules2/vco/module.json
native/sdk/LICENSE
native/sdk/fastmath.h
native/sdk/signals.h
native/sdk/simd.h
native/sdk/template/DESIGN.md
native/sdk/template/README.md
native/sdk/template/module.cpp
native/sdk/template/module.json
native/sdk/template/tests.cpp
native/sdk/wirescape_sdk.h
native/tests/fixtures/abi2/manifest.json
native/tests/fixtures/abi2/shim_scripted.wav
native/tests/fixtures/abi2/shim_steady.wav
native/tests2/allocation_watch.cpp
native/tests2/allocation_watch.h
native/tests2/dsp_fixture.h
native/tests2/guide_ringmod/DESIGN.md
native/tests2/guide_ringmod/LICENSE
native/tests2/guide_ringmod/README.md
native/tests2/guide_ringmod/module.cpp
native/tests2/guide_ringmod/module.json
native/tests2/guide_ringmod/ringmod.h
native/tests2/guide_ringmod/tests.cpp
native/tests2/integration_tests.cpp
native/tests2/module_loader.h
native/tests2/module_tests.cpp
native/tests2/notes_module.cpp
native/tests2/rows_module.cpp
native/tests2/test_modules.h
native/voice/LICENSE
native/voice/README.md
native/voice/allocator.cpp
native/voice/allocator.h
native/voice/tests.cpp
tools/bench/abi2_foundation.py
```

Implementation-only `git diff --stat 06be2dc 34b948e1d1404e6108ea3ceeec3ee951959e1c4a` (the final review-bookkeeping diff follows this checkpoint):

```text
 START_HERE_CURRENT_BUILD.md                  |   2 +
 docs/ABI2_BASELINE_MANIFEST.json             |  98 ++++
 docs/ABI2_PLAN.md                            | 433 +++++++++++++++++
 docs/ABI2_SPEC.md                            |  95 ++++
 docs/CLAUDE_CODEX_COMMUNICATION.md           |  15 +
 docs/COMMUNICATION_MODULES_PLAN.md           | 321 +++++++++++++
 docs/SDK_AUTHOR_GUIDE.md                     |  18 +
 docs/patch_v2_draft.example.json             |  51 ++
 docs/patch_v2_draft.schema.json              | 108 +++++
 native/CMakeLists.txt                        |   3 +
 native/engine2/CMakeLists.txt                |  75 +++
 native/engine2/engine2.cpp                   | 655 ++++++++++++++++++++++++++
 native/engine2/engine2.h                     |  70 +++
 native/engine2/tests/benchmark.cpp           |  26 ++
 native/engine2/tests/engine_tests.cpp        | 158 +++++++
 native/engine2/tests/header_c99.c            |   6 +
 native/engine2/tests/sdk_tests.cpp           |  37 ++
 native/include/wirescape/MODULE2_LICENSE.txt |  21 +
 native/include/wirescape/module2.h           | 121 +++++
 native/modules2/adsr/DESIGN.md               |   1 +
 native/modules2/adsr/LICENSE                 | 674 +++++++++++++++++++++++++++
 native/modules2/adsr/README.md               |   7 +
 native/modules2/adsr/dsp.h                   |  41 ++
 native/modules2/adsr/module.cpp              |   3 +
 native/modules2/adsr/module.json             |  20 +
 native/modules2/lfo/DESIGN.md                |   1 +
 native/modules2/lfo/LICENSE                  | 674 +++++++++++++++++++++++++++
 native/modules2/lfo/README.md                |   7 +
 native/modules2/lfo/dsp.h                    |  34 ++
 native/modules2/lfo/module.cpp               |   3 +
 native/modules2/lfo/module.json              |  18 +
 native/modules2/vca/DESIGN.md                |   1 +
 native/modules2/vca/LICENSE                  | 674 +++++++++++++++++++++++++++
 native/modules2/vca/README.md                |   7 +
 native/modules2/vca/dsp.h                    |  22 +
 native/modules2/vca/module.cpp               |   3 +
 native/modules2/vca/module.json              |  17 +
 native/modules2/vcf/DESIGN.md                |   1 +
 native/modules2/vcf/LICENSE                  | 674 +++++++++++++++++++++++++++
 native/modules2/vcf/README.md                |   7 +
 native/modules2/vcf/dsp.h                    |  36 ++
 native/modules2/vcf/module.cpp               |   3 +
 native/modules2/vcf/module.json              |  18 +
 native/modules2/vco/DESIGN.md                |   1 +
 native/modules2/vco/LICENSE                  | 674 +++++++++++++++++++++++++++
 native/modules2/vco/README.md                |   7 +
 native/modules2/vco/dsp.h                    |  34 ++
 native/modules2/vco/module.cpp               |   3 +
 native/modules2/vco/module.json              |  18 +
 native/sdk/LICENSE                           |  21 +
 native/sdk/fastmath.h                        |  10 +
 native/sdk/signals.h                         |  59 +++
 native/sdk/simd.h                            |  40 ++
 native/sdk/template/DESIGN.md                |   1 +
 native/sdk/template/README.md                |   1 +
 native/sdk/template/module.cpp               |  16 +
 native/sdk/template/module.json              |   1 +
 native/sdk/template/tests.cpp                |  19 +
 native/sdk/wirescape_sdk.h                   | 237 ++++++++++
 native/tests/fixtures/abi2/manifest.json     |  11 +
 native/tests/fixtures/abi2/shim_scripted.wav | Bin 0 -> 65580 bytes
 native/tests/fixtures/abi2/shim_steady.wav   | Bin 0 -> 65580 bytes
 native/tests2/allocation_watch.cpp           |  39 ++
 native/tests2/allocation_watch.h             |  13 +
 native/tests2/dsp_fixture.h                  |  33 ++
 native/tests2/guide_ringmod/DESIGN.md        |   3 +
 native/tests2/guide_ringmod/LICENSE          |  21 +
 native/tests2/guide_ringmod/README.md        |  12 +
 native/tests2/guide_ringmod/module.cpp       |   4 +
 native/tests2/guide_ringmod/module.json      |   1 +
 native/tests2/guide_ringmod/ringmod.h        |  35 ++
 native/tests2/guide_ringmod/tests.cpp        |  97 ++++
 native/tests2/integration_tests.cpp          |  55 +++
 native/tests2/module_loader.h                |  49 ++
 native/tests2/module_tests.cpp               |  61 +++
 native/tests2/notes_module.cpp               |   3 +
 native/tests2/rows_module.cpp                |   3 +
 native/tests2/test_modules.h                 |  49 ++
 native/voice/LICENSE                         |  21 +
 native/voice/README.md                       |  24 +
 native/voice/allocator.cpp                   | 243 ++++++++++
 native/voice/allocator.h                     | 101 ++++
 native/voice/tests.cpp                       | 156 +++++++
 tools/bench/abi2_foundation.py               | 104 +++++
 84 files changed, 7539 insertions(+)
```

For the entire final change, run `git diff --stat 06be2dc..HEAD`; protected-byte verification is authoritative for the preserved existing sources.
