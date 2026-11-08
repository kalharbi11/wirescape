<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T394 — diagnostic origin fixtures, no gate edits (#394)

**Purpose:** Prepare a human-reviewable fixture/design contract for #361. This
child defines expected origin classifications only; it does not decide or
implement suppression, parser rules, CI exit codes or dependency audits.

## Scope

- Review first-party source/header and pinned FetchContent/stmlib diagnostic
  examples, including mixed, outside-root and ambiguous symlink/relative paths.
- Keep the original tool, severity, location and message; distinguish observed
  provenance from clearly labelled synthetic test data.
- Preserve unknown origin as **unknown**, never implicitly exempt.
- Keep all code under `tools/ci/**`, CI workflows, analyzer options and
  first-party gating untouched. No hardware, downloads or dependencies.

## Files

- `docs/tasks/T394_DIAGNOSTIC_FIXTURES.md` (this task)
- `docs/reviews/DIAGNOSTIC_OWNERSHIP_FIXTURES.md`
- Up to three compact JSON corpus files in `tools/diagnostics/fixtures/`

## Acceptance

```acceptance
tier: T0
size: <= 180 changed lines, <= 5 files
must_pass:
  - python -m json.tool tools/diagnostics/fixtures/diagnostic_origin_cases.json
  - python -c "import json,pathlib; p=pathlib.Path('tools/diagnostics/fixtures/diagnostic_origin_cases.json'); a=json.loads(p.read_text()); assert len(a['cases']) >= 7; assert all(c['synthetic'] and c['diagnostics'] for c in a['cases']); assert all(d['tool'] and d['severity'] and d['location'] and d['message'] and d['expected_origin'] for c in a['cases'] for d in c['diagnostics'])"
  - bash -lc 'git diff --check origin/main...HEAD'
stops: [new parser or classifier, CI gate change, suppressions, new dependency]
```

**Review boundary:** #361 still requires Keel's T2 ownership and gating
decision before any `tools/ci/**` or workflow implementation.
