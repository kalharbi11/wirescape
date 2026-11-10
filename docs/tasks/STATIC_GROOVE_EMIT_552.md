<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Static: cppcheck false positive on Groove emit-macro guard (#552)

Issue #552. Preserve the real-compiler failure when the SDK header fails to
restore the caller's Qt `emit` macro; remove only cppcheck's false positive.
No CI suppression, DSP or protected-path change.

```acceptance
tier: T0
size: <= 20 changed lines, <= 2 files
must_pass:
  - ci: guard, linux-arm64, windows-x64, sanitizers, static green
stops: [weakened compile-time guard, test removal, CI suppression]
```

Signed: ChatGPT (GPT-6, T0 helper builder), 2026-11-10.
