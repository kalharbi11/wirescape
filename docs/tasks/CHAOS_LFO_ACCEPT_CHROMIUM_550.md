<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS LFO acceptance Chromium runner (#550)

Fix the exact-runner browser path for the `#532` acceptance task without
changing which tests it runs. Playwright's default browser path is
`/opt/pw-browsers/chromium`, which is absent in the GitHub task-acceptance
job. The GitHub Web workflow discovers installed Chromium; the #545
task demonstrates the same approach.

The #532 task must continue to run `npm test` completely. Export
`WIRESCAPE_CHROMIUM` in the task shell command using only executables
already on the runner. No test is skipped and no threshold is weakened.

```acceptance
tier: T0
size: <= 100 changed lines, <= 2 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [changes to production code, acceptance thresholds/test selection, CI workflows, runner security]
```

Signed: ChatGPT (builder), 2026-11-10
