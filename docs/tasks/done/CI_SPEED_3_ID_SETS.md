<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #687 — CI speed 3: catalogue-count literals become module ID-set checks

Side lane (owner, 2026-11-12). Owner: `claude-builder2`, after #686 (reassigned by Keel: gpt-builder3 keeps
the CI T2 work, which Keel reviews in one pass). Reviewer: gpt-builder3 or Coda.

**Goal:** adding a module touches no existing test. Removing or renaming a module ID still fails.

**Read first:**
- `apps/studio/tests/session.spec.ts` (about line 228, `toHaveLength(33)`)
- `native/document/app_patch_tests.cpp` (about line 22, `app_engine_module_count(...) == 33`)
- `native/sidecar/protocol_tests.py` (about line 713)
- `tools/gen_ui_catalogue.py` and `apps/studio/public/wirescape_modules.json` (generated)

Grep for the other catalogue-size literals: `app_engine_tests`, the cost-probe table.

## Steps
1. Add one committed list of known module IDs, e.g. `tools/foundation/fixtures/known_module_ids.txt`.
   It may only grow.
2. Each catalogue-count check becomes two checks:
   - every known ID is present;
   - count == the size of the catalogue's own ID list, if a count is needed at all.
3. One Python test checks that the generated catalogue's IDs are a superset of the known list.
4. **Order:** Studio TS and the C++ engine tests first. `protocol_tests.py` and `app_patch_tests.cpp`
   only after #684 and #648 merge. If they're still open, leave those two files for a follow-up commit.
5. Leave non-catalogue literals alone (Groove step counts, latency frames, monitor sizes).

**Self-review must say:** which literals were replaced (file:line), which were left on purpose, and
`Patch guide impact: none — tests only`.

**Stops:** removing an assertion without an equivalent ID check; touching files of an open main-work PR.

```acceptance
tier: T1
size: <= 260 changed lines, <= 9 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_ui_catalogue.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
  - ci: Web (Studio Playwright) green
stops: [assertion removed without ID check, overlap with open main-work PR files]
```
