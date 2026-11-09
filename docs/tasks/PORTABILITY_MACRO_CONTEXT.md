<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Portability: pinned Macro dependency context and adapter portability (#159)

Reuse the existing native CMake FetchContent setup to provision the exact pinned
Plaits/stmlib sources and authorized patches. The Cortex probe must compile
every module in both default and no-exceptions/no-rtti configurations, reporting
and failing on actual compiler errors. For Macro, use its production header
roots, TEST define and forced RNG adapter include. Do not duplicate pins,
fetch new sources, skip a module, or relax diagnostics.

The actual Arm compiler then exposed three mismatched `std::min` template
arguments (uint32_t is unsigned long on this target) and unavailable `M_PI`
under strict C++17/newlib. Planner approved an adapter-only correction on
issue #159: use explicit `std::min<uint32_t>` for the existing eight-voice
bounds, and provide a guarded mathematical constant before the pinned headers.
No synthesis math, ABI, parameter, upstream source, or dependency changes.
The probe is compile-only; it does not prove Daisy linking, memory budgets
or runtime correctness.

The protected workflow path `.github/workflows/portability.yml` was named and
authorized on issue #159. Tier T2 requires independent Claude-family review of
the final head. Because the compiled Macro adapter changes, validate its
existing focused tests and device-free Pi benchmark as well as exact-head CI.

```acceptance
tier: T2
size: <= 160 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_macro_oscillator_tests|abi2_macro_reference" --output-on-failure
  - ci: static (actionlint .github/workflows/portability.yml)
  - ci: cortex-m7 with both compile variants and every module reported
  - pi-run: build/native/abi2_macro_oscillator_tests --benchmark
stops: [new dependency, new upstream edit, skipped test, relaxed check, additional bare-metal incompatibility]
```
