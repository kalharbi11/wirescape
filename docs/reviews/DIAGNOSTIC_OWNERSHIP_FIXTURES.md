<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Diagnostic origin fixtures for #394 / #361

This is a **design/fixture review**, not a classifier or CI-gate change. Every
record in `tools/diagnostics/fixtures/diagnostic_origin_cases.json` is
**SYNTHETIC**: its location and message illustrate a contract boundary, not
a captured or reproduced analyzer warning. #361 separately reports actual
401-diagnostic fallout (400 transitive upstream, one first-party). Do not
attribute the synthetic text to that CI log.

## Expected classification contract to review with Keel

- `first_party`: tracked Wirescape source **or included header**, regardless
  of whether the translation unit also imports pinned code.
- `pinned_dependency`: a verified diagnostic path under the exact immutable
  FetchContent root (including a separately patched stmlib `SOURCE_DIR`).
  Keep original diagnostics visible; classification does **not** authorize
  suppression or changed exit-code semantics.
- `unknown`: outside known roots, unresolved relative paths, symlink aliases
  without resolved targets, or any unproven provenance. Never auto-exclude.
- The mixed case carries two independent outcomes within one compile unit;
  compilation ownership must not overwrite diagnostic-location ownership.

Source pins checked against current repository CMake (not inferred from names):
`native/modules2/macro_oscillator/CMakeLists.txt` declares eurorack
`f4cfa7697095a28d292bff758beb51deb49b5a86` and stmlib
`e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`. The latter uses a
module-binary-tree `SOURCE_DIR` and an explicit portability patch.
`native/document/CMakeLists.txt` pins nlohmann/json to
`9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`. These fixture paths
are example relative paths, **not** guaranteed resolved canonical paths.
Each source record retains tool, severity, path/line/column, message, raw
diagnostic and proposed origin/provenance. Nothing chooses whether a
third-party finding should gate; #361 needs Keel's separate T2 decision.

Check using `python -m json.tool` plus the task's required-fields command.
No protected `tools/ci/**`, workflow or analyzer setting is changed.
