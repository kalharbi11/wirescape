<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #468: Qt retirement and R5c, with non-deleting golden preparation

Read first: `AGENTS.md`, issue #468/latest lead ruling, `docs/planner/CLEANUP.md`,
`docs/tasks/R2_DOCUMENT_COVERAGE.md`, `native/tests/golden_render.cpp`,
`tools/golden.py`, `native/document/CMakeLists.txt`.

## Gates and scope

This single R3 branch starts with the authorized non-deleting golden port.
R1 (#464), W1 (#475) and R2 (#478) are merged; final current-main Windows,
replacement proof, independent review and scoped labels still gate deletion/merge.
No Qt/archive files, agent policy or archive tag change in the preparation.

Keep `golden_render_hashes` and `golden_event_stream`: 48000 Hz/frames, block128,
seed42; finite output and fault checks, two fresh render instances, SHA256 and
changed-event sensitivity. Preserve canonical little-endian float bits and every
serialized event field, four bounded UMP words and unpadded payload bytes.
Python's existing hashlib owns hashing; no added runtime/crypto dependency.
The native serialization oracle uses an independent Python struct-packed vector.
The old Qt renderer from the same immutable base can be supplied to
`tools/golden.py check --compare-renderer PATH` for actual per-case hash equality.

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
Before merge tag the reviewed current-main last-Qt/archive base as immutable
lightweight `archive/qt-final` once, containing the actual prerequisite merge SHAs.
Private Pi Qt-mode removal is a separate PR under this same issue.

```acceptance
tier: T2
size: <= 30808 changed lines, <= 234 files
must_pass:
  - python -m unittest tools.foundation.tests.test_golden -v
  - python tools/dev.py build --headless
  - ctest --test-dir build/native --output-on-failure --no-tests=error -R '^golden_(render_hashes|event_stream|serialization)$'
  - python tools/dev.py test
  - bash -lc '! git grep -n "find_package(Qt6" -- native'
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [product test loss, threshold change, new dependency, ABI/runtime protocol edit, unreviewed retirement, tag movement]
```

The no-Qt grep deliberately remains unsatisfied during non-deleting preparation.
A preparation report is not final R3 acceptance, deletion authorization or approval.

Signed: Codex Lead (Codex, delegated R3 golden-port builder), 2026-11-09.
