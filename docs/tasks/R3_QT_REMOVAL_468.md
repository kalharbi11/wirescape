<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #468: R3a Qt-free golden preservation; R3b Qt retirement follows

Read first: `AGENTS.md`, issue #468/latest lead ruling, `docs/planner/CLEANUP.md`,
`docs/tasks/R2_DOCUMENT_COVERAGE.md`, `native/tests/golden_render.cpp`,
`tools/golden.py`, `native/document/CMakeLists.txt`.

## Gates and scope

This branch/PR is the non-deleting R3a golden port under Keel's split ruling
(issue #468, 2026-11-09). R1 (#464), W1 (#475) and R2 (#478) are merged.
R3a may merge after independent T2 review, exact-head required checks and actual
`golden_render_hashes`, `golden_event_stream` and `golden_serialization` passes on
both linux-arm64 and windows-x64. Retain `Refs #468`; R3a does not close #468.
No Qt/archive files, agent policy or archive tag change belongs in R3a.
R3b carries deletion, retirement mapping, scoped labels and the archive tag.

Keep `golden_render_hashes` and `golden_event_stream`: 48000 Hz/frames, block128,
seed42; finite output and fault checks, two fresh render instances, SHA256 and
changed-event sensitivity. Preserve canonical little-endian float bits and every
serialized event field, four bounded UMP words and unpadded payload bytes.
Python's existing hashlib owns hashing; no added runtime/crypto dependency.
The native serialization oracle uses an independent Python struct-packed vector.
Before R3a merge, supply the old Qt renderer from the same immutable source tree
to the current engine/catalogue using the temporary guarded target in
`python tools/compare_golden_qt.py --qt-prefix PATH_TO_QT` (Windows CI already
provisions Qt). It verifies frozen blob8c136f28872a38024345427433afa905dbb1e318
from pre-port16e49e9302d3a3ceddaca1b52fe0fa963125ce4d without committing a duplicate
source. Existing `tools/golden.py check --compare-renderer PATH` asserts per-case hash
equality plus both renderer/source revisions. Same-build repeatability is a
different assertion; no old-vs-new pass is inferred from it.

## Exact final deletion manifest and budget

The approved manifest at118e319 has222files/30008deleted text lines/two binaries:
- `native/ui/**`:18files (including ico/png binaries).
- `native/document/{document_reader,patch_document,app_patch}.{h,cpp}` and
  `native/document/app_patch_tests.cpp`.
- `native/tests/{document_reader_tests,patch_document_tests,patch_render_tests,audio_device_tests}.cpp`
  and `native/tests/fixtures/prepare_failure.cpp`.
- `legacy/**`:105files/18752text lines; `docs/history/**`:87files/6486text lines.

Recompute against the actual prerequisite base if manifest blobs change. In
addition to the mechanically checked full cap, authored changes outside this
exact deletion manifest stay <=800changed lines/12modified-or-new files.
Never spend the deletion allowance on other source changes. Keep all six files
in `native/tests/fixtures/legacy_documents/**` and all PortAudio provenance intact.
`native/tests/golden_render.cpp` is preserved and ported, not in the manifest.

## Final coverage/retirement review

R2's table maps document refusal, native-v1 migration, unknown-data retention and
real Electron no-clobber backup/atomic persistence. Qt inspector classifications,
sequence/state inventories and migration manifests explicitly retire.
Before deletion enumerate `patch_render_tests`/`abi2_app_patch_tests` audio,
Macro/CHAOS/fake-MIDI/no-allocation/equality assertions against actual runtime,
builtin and conformance replacements; do not claim document refusal replaces audio.
Qt widget mechanics in `studio_ui_tests` retire by D15. Map controller persistence
and rollback to R2/#463 product tests and explicitly identify retired Qt hooks.
Retire the old audio wrapper only; current callback/sidecar audio recovery tests stay.
Keep Qt-free `app_document_soak` and both product golden CTests.

Final R5c records the owner's actual expanded Lead routing authority narrowly;
independent review, safety, compatibility and exact correctness/performance gates stay.
Root applies scoped allow-protected/allow-test-removal only after the mapping review.
Immediately before R3b merges, Codex Lead may create the immutable annotated
`archive/qt-final` tag on its exact current-main merge parent, containing the
actual prerequisite merge SHAs (Keel's latest ruling supersedes lightweight).
Private Pi Qt-mode removal is a separate PR under this same issue.

```acceptance
tier: T2
size: <= 800 changed lines, <= 12 files
must_pass:
  - python -m unittest tools.foundation.tests.test_golden -v
  - python tools/dev.py build --headless
  - ctest --test-dir build/native --output-on-failure --no-tests=error -R '^golden_(render_hashes|event_stream|serialization)$'
  - python tools/dev.py test
  - ci: guard
  - ci: linux-arm64 including all three golden CTests on the exact head
  - ci: windows-x64 including all three golden CTests on the exact head
  - ci: sanitizers
  - ci: actual frozen Qt vs Qt-free per-case hash comparison from the same immutable source tree, with both revisions recorded
stops: [Qt or archive deletion in R3a, product test loss, hash/settings rebaseline, threshold change, new dependency, ABI/runtime protocol edit, tag creation in R3a]
```

This acceptance block governs R3a only. Before R3b is claimed, restore the final
30808-line/234-file manifest cap (recomputed if needed), retain the 800/12 authored
cap, require a no-Qt-installed build/full test and the final no-Qt grep:
`bash -lc '! git grep -n "find_package(Qt6" -- native'`. Keep all existing required
check names. R3a's green report is not deletion authorization or final R3 completion.
A same-build hash equality report alone is not an executed frozen-Qt comparison;
record any actual `--compare-renderer` result separately with both source revisions.

Signed: Codex Lead (Codex, delegated R3 golden-port builder), 2026-11-09.
