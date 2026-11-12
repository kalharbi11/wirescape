<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #558: Current architecture and status docs catch-up

Issue #558 is a T0 documentation-only closeout. Update
`docs/ARCHITECTURE.md` and `docs/STATUS.md` for confirmed merges on
2026-11-10, with direct PR references and accurate distinctions between
shipped source, hosted evidence, Pi acceptance, and pending work.

This task does not change the Groove module or its tests, any native or
Studio runtime code, protected policy/spec paths, device permissions, or
release claims. Keep the recorded Pi accepted SHA historical and do not
treat newly merged source as re-certified owner hardware.

```acceptance
tier: T0
size: <= 150 changed lines, <= 3 files
must_pass:
  - python tools/foundation/tests/test_doc_links.py
  - ci: guard
stops: [module code changes, modified protocol or IDs, protected paths, changed test thresholds, invented hardware acceptance]
```

Signed: ChatGPT (GPT-6, docs builder), 2026-11-10
