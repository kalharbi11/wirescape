<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #571 — Compiler-owned cppcheck Qt model split

**Tier:** T2 (CI analyzer policy only). **Budget:** ≤600 changed lines, ≤7 files.
**Authorization:** Keel on [issue #571](https://github.com/kalharbi11/wirescape/issues/571#issuecomment-6786561484); `tools/ci/**` requires an `allow-protected` label on the PR, to be supplied by Keel, not its builder.

## Purpose

The existing single `cppcheck_changed.json` database passed `--library=qt` over
native Engine2, modules2 and sidecar translation units, including document/Web
includers of their headers. The Qt keyword model is not applicable to those
first-party units. Fix model ownership, not the diagnostics or the test gate.

## Rules

- Preserve the exact `build/static` and `build/static-web` compiler-database
  union, Ninja `-t deps` dependency ownership and stale/deleted-header checks.
- For every selected TU, inspect its compiler-recorded full dependency closure,
  including nested headers and X-macros. If the TU or any included file belongs
  to `native/modules2`, `native/engine2` or `native/sidecar`, use native
  analysis **even if** a Qt header is reachable. This also applies to
  `native/document` and `native/web` includers.
- Only a TU that has Qt header dependencies **without** a product owner uses
  `--library=qt`; unmapped sources default to native, never automatically Qt.
- Keep the existing `cppcheck changed files` check name, analyzer flags, all
  selected TUs and per-source compilation flags. Produce separate current
  native/Qt projects only when needed, using a single unchanged findings gate.
  Keep the exact vendor baseline and all first-party/new diagnostics red in
  either project.
- No new suppression, accepted diagnostics, code/port changes, CI workflow
  or threshold edits. Existing review/pinned baseline behavior remains intact.

## Validation

The unit suite checks direct, mixed, document/Web, Qt-only, unknown, changed
product header and X-macro ownership, preserved compile flags, stale dependency
refusal and deletion-only selection. Full source-matched static, guard,
portability/Windows and regular required CI must also pass. One independent
other-family T2 review is required; do not self-approve.

```acceptance
tier: T2
size: <= 600 changed lines, <= 7 files
must_pass:
  - python -m unittest tools.ci.test_static_checks
  - python -m unittest tools.ci.test_guard
  - ci: guard, static, linux-arm64, windows-x64, sanitizers green
stops: [suppressions, baseline relaxation, skipped first-party units, changed thresholds, changed check names, broad Qt deletion, workflow changes without explicit approval]
```
