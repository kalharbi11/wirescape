<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #747 CI: Chromium for Studio task acceptance

Read first: `.github/workflows/ci.yml` task-acceptance job, `.github/workflows/web.yml`, `apps/studio/playwright.config.ts`, `tools/foundation/tests/test_ci_triggers.py`.

- Preserve all selected task commands, tests, assertions, retries and required checks.
- Inside the existing Studio Playwright predicate, locate executable host Chromium in Web CI's order, set `WIRESCAPE_CHROMIUM` in the environment inherited by acceptance, and fail closed if unavailable. Keep ffmpeg provisioning.
- Add regression coverage of conditional setup, executable validation and inherited environment before acceptance. A static source test alone does not prove browser launch: selected Studio acceptance below must actually run.
- Scope: workflow, additive CI regression, this task and planner STATE only. Keel authorizes `.github/workflows/ci.yml` and approves CI policy; independent Claude-family T2 review required for GPT implementation.

```acceptance
tier: T2
size: <= 180 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests
  - bash -c 'cd apps/studio && npm ci && npm test'
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [test or threshold weakening, changed required check, new dependency, device or LAN access]
```

After merge: #738 integrates main and reruns selected accept 732; prior Web success does not certify the new head.
Patch guide impact: none — CI browser setup only.
Signed: Codex Lead, 2026-11-14
