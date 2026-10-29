<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pinned DSP adapter and support audit

Macro Oscillator: DSP port of Mutable Instruments Plaits by Emilie Gillet,
`pichenettes/eurorack` commit `f4cfa7697095a28d292bff758beb51deb49b5a86`;
stmlib commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
Original code under MIT; Wirescape adapter under GPL-3.0-only. Sources are fetched
by immutable archive URLs plus SHA256, not vendored. Every compiled upstream DSP
translation unit and generated resource retains its original MIT notice; the
stmlib MIT licence applies to `dsp/units.cc` and utility/DSP headers. STM firmware,
programming tools, serial code, VCV code and graphics are excluded.

## Source closure and hardware boundary

Compile `plaits/dsp/**/*.cc`, `plaits/resources.cc` and `stmlib/dsp/units.cc`.
Upstream sources have only the planner-authorized portability patches described below. `TEST` selects upstream UserData::ptr's
null-returning implementation, so the factory preset path never reads a hardware
flash address. Firmware/device initialisation and UserData::Save are never called.
All 24 engine entries remain selectable; no engine is excluded.

## Random state isolation

Upstream `stmlib::Random` has process-global mutable state. `rng_scope.hpp` includes
that original declaration under an unused private compatibility name and supplies
an API-compatible storage wrapper with the same 32-bit LCG arithmetic. This is an
instance-state boundary, not a synthesis rewrite: each voice saves/restores its
own RNG register around Init/Render; the temporary register is thread-local.
GNU builds reserve static TLS at load time, preventing lazy TLS allocation on the
render thread. No lock serialises engines. Concurrent and interleaved noise-engine
tests compare separate instances against a standalone stream; different seeds must
differ. A separate oracle compares words, floats and samples against the unchanged
pinned RNG class for four seeds across 10,000 iterations. Global constant lookup
tables are shared read-only.

## Timing, state and memory

Each prepared channel owns a Voice object (FAST) and one 16,384-byte workspace
(LARGE), reused by its engines just as upstream does. There are eight maximum
prepared channels and a fixed 12-frame FIFO each. Controls are sampled at quantum
start; Render runs only when twelve input frames have accumulated. Output starts
with twelve zero frames; both streams use upstream signed-16-bit/32768 scaling.
All allocation is in prepare; process and reset are allocation-free. Reset rebuilds
the Voice object in already reserved storage and initialises its workspace with a
seed mixed by channel. No upstream object pointers are serialized. State v1 is a
version marker; the host stores parameters, and loading valid state resets DSP.
This follows the planner's explicit patch-state decision, not mid-note continuation.

Direct reference tests instantiate pinned Voice and compare models 0, 8, 10 and 13
bit-for-bit after the 12-frame delay. All 24 models cover the host-block matrix,
reset, finite output and constant-flag checks. Tests reject unsupported rates and
insufficient prepare storage. ARM/Daisy support is reported from actual CI/probes;
there is no unmeasured bare-metal compatibility claim.

## Authorized portability patch (planner 2026-10-28)

`patches/unsigned-pow2-shift.patch` applies at FetchContent time to eurorack
`f4cfa7697095a28d292bff758beb51deb49b5a86`, `plaits/dsp/fm/dx_units.h:79`.
Pow2Fast's exponent adjustment shifts the unsigned representation of x_integral,
then converts that 32-bit result back before the original addition. This avoids
left-shifting negative signed values while preserving the Release bits. No model
is excluded; no sanitizer is suppressed. The patch runner accepts an already
applied patch and fails on any other source mismatch. `_USE_MATH_DEFINES` is set
for MSVC consumers so existing upstream math constants are declared.

Before patching, GCC 14.2.0 x86_64 `-O3 -DNDEBUG` at 48k/128 produced these FNV-1a
64-bit reference hashes over the main-then-aux float32 bit streams (low byte first),
48,000 samples per stream, seed 42, trigger connected/high, default controls:

| Model | Unpatched Release hash |
|---|---|
| 2 | `210b80dca5fe2915` |
| 3 | `ba4c1cadbda672a5` |
| 4 | `8ee3f2f04c6dd061` |

`signed_shift_reference` asserts non-silence and records hashes for each platform
family. Per the planner decision on #29 (2026-10-29), hashes from another build
are report data, not assertions; exact equality is gated by the same-build
patched-versus-original Release oracle below. ARM64's unpatched and patched Release hashes were identical in CI run
40759170616 (`abi2_macro_reference`, Linux ARM64): model2 `e5cf182ea7892761`,
model3 `0aa6ab841f075ed1`, model4 `4ebac16c1be444b5`. These are recorded separately
from x86-64; no cross-family equality is assumed.
The existing direct-reference, all-24-model, allocation and isolation tests remain.

### Additional planner authorization, issue #29 (2026-10-28)

- `msvc-specializations.patch` guards four header-only specialization declarations
  with `_MSC_VER`; MSVC otherwise treats these uninitialized const declarations
  as definitions (C2737). The class declarations and initialized definitions in
  `algorithms.cc`, including every opcode/renderer, remain unchanged.
- `empty-resource-table.patch` adds a null sentinel to the otherwise empty
  `lookup_table_i32_table` (C2466). The pinned DSP closure never references that
  table; no used lookup table or model is changed.
- `apply.cmake` normalizes patch CRLF to LF and uses `git apply --ignore-whitespace`
  for application and reverse checks, so Windows checkouts can apply/reapply the
  same pinned patches. Source/context mismatches still fail configuration.

The Release-only `abi2_macro_reference` test builds an independent upstream oracle
with just the exponent-shift header restored through a private include overlay.
The other portability fixes and source pins are identical. It compares complete
model 2/3/4 main+aux stream hashes, using the same toolchain for baseline and patch,
and publishes the equal JSON results as `build_reports/abi2/macro-reference-*.json`
(uploaded by existing native CI). The unpatched oracle is not built or executed
in sanitizer configurations; all production DSP and its normal tests remain
instrumented without suppressions. No unpatched library enters the module or app.
The normal test records hashes without comparing another toolchain's baseline.
The Release oracle requires exact patched-versus-original equality with the same
compiler and flags; sanitizer builds continue to exercise the patched DSP only.

The render helper checks both size multiplications before allocating and uses
`size_t` indices. Boundary tests include zero and overflow in either operand; no
CodeQL suppression is used.

Windows CI after C2737/C2466 was fixed exposed GNU-only trailing
`__attribute__((always_inline))` declarations in pinned stmlib `utils/dsp.h`.
`stmlib-msvc-inline.patch` preserves those hints for GNU compilers and omits only
the unsupported annotation on MSVC; the functions remain `inline` and every
arithmetic expression is unchanged. It applies to stmlib
`e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` through its own FetchContent patch step.

### Pre-review follow-up (2026-10-29)

Host RNG initialization folds the high seed word into the low word before the
per-voice offset. Seeds below 2^32 retain their output; a model17 regression
checks distinct high-word seeds and exact reset repeatability.
The unpatched oracle is now limited to single-config Release. Configuration and
toolchain flags, directory/target compile and link options, and unevaluated
option expressions conservatively disable it when instrumented. Multi-config
builds run patched tests without an unpatched target. The header overlay requires
exactly one expected patch expression; missing/duplicate tokens fail configure.
The ABI2 guard CTest covers malformed headers and instrumentation settings.
Real Ninja and Ninja Multi-Config Release configurations with UBSan in
CMAKE_CXX_FLAGS_RELEASE create neither unpatched target; a clean Release control
creates both. No instrumentation is removed or suppressed.

The placement-owned reference fixture is explicitly non-copyable. New module C++
files follow clang-format18; no unrelated existing files were reformatted.
