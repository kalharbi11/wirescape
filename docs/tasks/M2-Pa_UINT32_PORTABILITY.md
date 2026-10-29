<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M2-Pa: Cortex-M7 uint32_t template portability (#56)

**Scope:** Explicit `uint32_t` template type for mixed-width bounds in the
SDK and ABI2 ports. Preserve DSP/equivalence. In the same PR, make
`.github/workflows/portability.yml` fail on any compile error rather than
report green. Both default and `-fno-exceptions -fno-rtti` modes must pass
for every existing ABI2 module. No upstream/third-party modules are implied.

```acceptance
tier: T0
budget: 1 ChatGPT CI iteration
size: <= 40 changed lines, <= 6 files
must_pass:
  - ctest --test-dir build/native -R "abi2_(module_tests|scalar_ports|portable_ports|engine_tests)" --output-on-failure
  - ci: cortex-m7 both columns pass for every ABI2 module; job exits nonzero on failure
  - ci: linux-arm64, windows-x64, sanitizers, guard green; equivalence 0
stops: [ABI edit, DSP math change, equivalence delta, new dependency, test removal]
```
