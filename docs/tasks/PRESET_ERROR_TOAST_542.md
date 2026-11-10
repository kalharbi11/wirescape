<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #542: Preset error toast (Studio)

Issue #542, T1 helper. When the module preset selector receives invalid preset
metadata, report the existing error toast instead of throwing from its change
handler. Resolve and validate before any module parameter is applied; refusal
must leave the patch, parameters and undo history untouched.

A Playwright test injects invalid preset data, selects it, checks the toast
and unchanged document/history, then verifies a corrected preset works. Keep
all existing preset values, native descriptors and parameter IDs unchanged.

Patch guide impact: none — error handling only.

```acceptance
tier: T1
size: <= 120 changed lines, <= 4 files
must_pass:
  - bash -lc "cd apps/studio && npm ci && npm test"
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [preset definition or parameter ID change, test removal, weakened assertions, new dependency]
```

Signed: ChatGPT (GPT-6, #542 builder), 2026-11-10
