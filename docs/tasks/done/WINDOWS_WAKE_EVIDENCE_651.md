<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Countable Windows wake-only retry evidence (#651)

Follow-up to merged #637/#639. Read Keel's Low follow-up approval and
Coda's independent CI-observability finding on PR #639. A successful hosted
Windows wake-only retry currently logs attempt outcomes only to subprocess
stderr that protocol_tests.py suppresses via capture_output=True.

Expose optional, bounded `wake_retry` evidence in successful CLI JSON only
when the original strictly classified wake-only failure is followed by a
fully clean second run. The protocol test caller re-emits both attempts in
the CI console and writes one summary count line to GITHUB_STEP_SUMMARY if
provided. A double wake failure remains nonzero and its bounded stderr
tail includes both attempts. Normal one-attempt runs remain unchanged.

Add real-subprocess negative assertions to the existing unit test matrix.
Do not change retry eligibility (hosted Windows only), 1-retry cap,
strict numeric/overrun thresholds, seed/duration, hardware selection,
CI workflow, sanitizer policy, native realtime path or protocol wire.

```acceptance
tier: T1
size: <= 150 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [additional retry, altered benchmark limit or gate, skipped test, CI policy or realtime change, hardware/LAN access]
```

Patch guide impact: none — CI test-harness evidence only.
Signed: ChatGPT (GPT-6 builder), 2026-11-11
