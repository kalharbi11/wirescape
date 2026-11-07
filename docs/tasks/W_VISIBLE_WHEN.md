<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W: conditional parameter controls (#306)

Add the optional `visibleWhen: { param: "mode", values: [7] }` presentation
field approved in #306. Only panel controls and empty section labels hide;
values, saved documents, modulation rails and cables remain intact. Use base
parameter values with descriptor defaults, and refresh on document changes and
refusals. Remeasure the card and invalidate cable anchors on visibility changes.
Malformed or unknown conditions leave controls visible. Catalogue generation
must preserve the field and unknown nested metadata without rewriting it.

Read first: `apps/studio/AGENTS.md`, `src/modules/types.ts`,
`src/panels/PanelBuilder.ts`, `src/canvas/ModuleCard.ts` (paths under Studio),
`tools/gen_ui_catalogue.py`, `tools/foundation/tests/test_ui_catalogue.py`.
No Spectral integration until that module's presentation lands; use an existing
LFO descriptor fixture to test the generic behavior independently.

UI commands run in the provisioned Web job, which installs pinned npm packages,
Chromium and the video recorder. The native acceptance job has no npm dependencies.
The full Web suite includes the targeted conditional-controls regression; its
exact-head result is required external evidence, never inferred from local checks.

```acceptance
tier: T1
size: <= 250 changed lines, <= 9 files
must_pass:
  - python -m unittest tools.foundation.tests.test_ui_catalogue
  - python tools/gen_ui_catalogue.py --check
  - ci: full Web typecheck, production build, all Playwright tests including conditional-controls.spec.ts, and desktop Electron suites green
stops: [new dependency, format change, threshold change]
```
