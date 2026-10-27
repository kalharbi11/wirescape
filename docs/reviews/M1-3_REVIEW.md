<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M1-3 review: ABI 2 slice fixes and the performance gate

Reviewer: Claude (planner). Date: 2026-10-27. PR #22, issue #6, head `54cb790`. Scope: the diff against `main` `6cc87f9` (23 files, +556/−61), the handoff comments on #6 and the CI checks. Spec: `docs/reviews/M1-R_PLANNER_REVIEW.md` ("Go/no-go for M2") and `docs/tasks/M1-3_SLICE_FIXES.md`.

## Verdict
**Conditional go.** Merge PR #22. M2-1 to M2-5 may start. One gate is still open: Windows mono. It moves to a new issue, **M1-3b (#32)**, which must be merged before **M2-6** (engine2 becomes the app engine) starts. The gate is not waived and its threshold doesn't change.

Why a conditional go rather than a no-go:
- Every deterministic and ARM measurement passes, most with a wide margin. That includes the Pi 5, the performance target.
- The miss is on one hosted runner, and the main-branch baseline wasn't measured there with the new method. The size of the gap (≈4.5% over the target) is within what MSVC codegen alone can explain, for example a loop that GCC vectorizes and MSVC doesn't.
- M2-1 to M2-5 add modules, tests and documents. None of them changes the engine hot path that sets the mono ratio, so starting them doesn't hide or worsen the gap. M2-6 makes engine2 the Windows app engine, so the gap has to be closed before then.

## Gate results
| Condition (M1-R) | Result | Status |
|---|---|---|
| Mono ≤ 1.05×, `linux-arm64` runner | 0.865× | pass |
| Mono ≤ 1.05×, `windows-x64` runner | **1.097×** | **open → #32** |
| Mono ≤ 1.05×, callgrind Ir (x86-64) | 0.860× (74.3k vs 86.3k Ir/block) | pass |
| Mono, Pi 5 (target, not a formal gate) | 0.711× (main: 1.059×) | pass |
| Poly8 / eight-mono ≤ 1.00× | 0.965× (arm64), 0.974× (win), 0.967× (Pi) | pass |
| Modulated parameter-pipeline self-cost ≤ ½ of 68k Ir | 29.3k Ir (0.454× of main's 64.4k) | pass |
| `sanitizers` job green (ASan+UBSan, TSan) | green on `54cb790`; Pi TSan pass with an LLVM 24 runtime | pass. **The user still has to make it a required check** |
| Equivalence max error 0, shim bit-identical, zero allocations, poly WAV hash | all unchanged (`ad8aec74…`) | pass |
| Before/after Ir table per probe and per-runner medians in the PR | present | pass |

## Findings
No blocking defects.

**Checked and fine:**
- **Zero-copy views (A3).** Aliasing is limited to non-loop, non-shim destinations (`direct`), and only when the source isn't a back edge. Non-loop components always run as one span from frame 0, so the aliased pointer and `start` agree. Loop nodes keep the copy path.
- **Node lookup by binary search.** `find()` uses `lower_bound` on `node_table` after prepare. This is correct only because `topology()` sorts `nodes` by ID first. I confirmed it with a repro that adds nodes in descending order: `descriptor()`, `reconfigure()` and the output-bus lookup all work.
- **Validation once at the producer (A4).** The exponent max-reduction plus precise rescan is correct for NaN and ±Inf. Shim outputs are validated once in engine2 and counted as faults. The spec wording (D14, "Experimental limits") matches the code.
- **M6.** Non-`PER_VOICE` parameters stay one channel and take poly modulation channel 0. Spec and tests agree.
- **The tests are real.** The mixed SCC test covers all insertion permutations and both edge orders at `feedback_block` 1/32/256, with an exact delivery-frame check. The panic stress test runs four threads and checks that no gate is left on.

**Low, follow-ups (not blocking):**
- **L-a. Modulation sum order.** With two or more modulation cables on one parameter, the result is now `base + (Σ amount·cv)` instead of `((base + a₁·cv₁) + a₂·cv₂)`. It's still deterministic, but can differ from `main` by an ULP. No reference depends on it today. If a fixture ever pins multi-cable modulation, pin the new order.
- **L-b. The callgrind step reports but doesn't gate.** M1-R said "Ir for the CI gate". `abi2_cost.py` needs a `--gate` (mono ≤ 1.05, exit 1), and the `sanitizers` job should use it. → #32.
- **L-c. The banned-include test scans every `.cpp` under `native/modules2/`, including `tests.cpp`.** M2 port tests that include `<vector>` or `<string>` would fail it. Decision: module `tests.cpp` files are host-side test code and are exempt. The DSP files (`dsp.h`, `module.cpp`) stay strict. → M2-2 (first port to hit it) makes that one-line change, with a test.
- **L-d. The Windows benchmark has no main baseline with the interleaved method.** → #32 collects it.

## What the user does
1. Merge PR #22.
2. Add `sanitizers` to the required status checks on `main`.
