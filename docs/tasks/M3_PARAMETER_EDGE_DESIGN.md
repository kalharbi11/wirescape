<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3 parameter edge representation design (#177)

Retain the approved design at `docs/design/M3_PARAMETER_EDGES.md` (129 lines). This is documentation-only design work. The planner approved the representation as written in comment 6537669135. Do not change the representation, patch schema, ABI, engine, Studio, runtime behavior, or the existing design text. This task authorizes no implementation; any implementation requires a separate later issue below #182.

Read first:
- `docs/design/M3_PARAMETER_EDGES.md`

Acceptance:
- The task file identifies the approved design as the implementation authority and preserves its scope and constraints.
- The design file remains unchanged at 129 lines.
- No production, schema, ABI, engine, Studio, runtime, or device changes are included.

```acceptance
tier: T2
size: <= 260 changed lines, <= 2 files
must_pass:
  - python -m unittest tools/foundation/tests/test_doc_links.py
stops: [design representation change, implementation work, scope above budget]
```
