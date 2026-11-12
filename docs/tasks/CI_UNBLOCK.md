<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #635 — Unblock hosted CI acceptance and sanitizer provisioning

Urgent Keel-approved CI recovery, extracted from the runtime-only portions of
#617 without importing its docs-only classifier or policy branches.

## Scope

- Preprovision the Playwright-pinned FFmpeg helper when the selected task's
  acceptance command actually invokes the full Studio Playwright suite.
  Preserve the suite, selected tests, and all assertions unchanged.
- Extend the acceptance job envelope to 30 minutes and its per-command bound
  to 1200 seconds so the full Studio/Electron test command can finish.
- In both ARM64 sanitizer lanes, retry transient `apt-get update/install`
  failures at most three times with `Acquire::Retries=3`; fail on the third
  failure. Keep all sanitizer, cost, and native test steps unchanged.
- The workflow remains full-code CI. Leave #617's docs-only classifier,
  required-check names, job selection and the merge gate untouched.

## Verification

- `actionlint` must parse the changed workflow.
- Guard, Linux ARM64 and both sanitizer lanes/aggregate must pass on the
  exact PR head; owner hardware is not involved.
- A task which does not invoke Studio Playwright remains unmodified.

```acceptance
tier: T2
size: <= 60 changed lines, <= 2 files
must_pass:
  - actionlint
  - ci: guard, linux-arm64, sanitizers green
stops: [any change to test selection, sanitizer flags, cost thresholds, required-check names, the merge gate]
```

Signed: ChatGPT (GPT-6, non-OSC T2 builder), 2026-11-11 EST.
