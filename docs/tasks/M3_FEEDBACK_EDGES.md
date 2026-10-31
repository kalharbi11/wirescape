<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3-3 feedback/back-edge design (#179)

Design-only task corresponding to the short acceptance in issue #179.
Read `docs/design/M3_READINESS_GAPS.md`,
`docs/design/M3_INTERACTION_STUDY.md` §4 and the engine2 feedback
API before editing. Deliver `docs/design/M3_FEEDBACK_EDGES.md`:
a user flow for explicitly authoring delayed feedback, persistence
using the existing native v2 fields, real engine-designated back-edge
markers, technical prerequisites and offline acceptance scenarios.

Do not modify controller, engine, runtime, ABI, patch schema, QML, tests
or protected paths. No device or LAN activity. This is not approval to
enable feedback editing; open decisions stay with the planner.

```acceptance
tier: T0
size: <= 250 changed lines, <= 2 files
must_pass:
  - python -m unittest tools/foundation/tests/test_doc_links.py
stops: [new patch-format field, ABI header edit, implementation code, removed test, threshold change]
```
