<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Portability: pinned Macro dependency context (#159)

Read the existing Macro CMake FetchContent configuration and portability workflow.
Configure native headlessly to populate its existing pinned dependencies and patches;
add the production Macro include roots, TEST define and RNG forced include to that
module's two Cortex compile variants. Do not build host targets, duplicate pins,
fetch new sources, skip a module or relax compile failures. Compile-only coverage
is not a Daisy runtime/linking/memory proof. Existing bare-metal failures remain
visible and require separate scope if more than dependency context is needed.

Planner named .github/workflows/portability.yml on issue159; independent Claude
exact-head gate remains required. No tools/ci or ABI changes. Workflow-only fix
needs no device or Pi rerun.

```acceptance
tier: T2
size: <= 160 changed lines, <= 3 files
must_pass:
  - ci: static (actionlint .github/workflows/portability.yml)
  - ci: cortex-m7 with both compile variants and every module reported
stops: [new dependency, new upstream edit, skipped test, relaxed check, actual bare-metal incompatibility beyond context]
```
