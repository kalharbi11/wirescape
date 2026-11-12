<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Qt cppcheck translation-unit classification (#653)

Regression follow-up to #621: Qt moc and UI translation units importing
product headers must still receive cppcheck's Qt keyword model.
The existing product-header-first rule remains for non-Qt source owners.

Read first: `tools/ci/static_checks.py`, `tools/ci/test_static_checks.py`.

## Implementation
- Classify any TU under `native/ui/` or a build `*_autogen/` directory as `qt` before checking transitive headers.
- Preserve native classification for product-owned non-Qt TUs.
- Regress Qt UI with engine headers and generated moc sources, without weakening existing test cases.
- Do not change analyzer suppressions, baseline, flags, check name or selection.

```acceptance
tier: T2
size: <= 60 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/ci -p test_static_checks.py
  - ci: guard, linux-arm64, sanitizers green
stops: [suppressions, baseline relaxation, skipped units, check-name change]
```

Signed: ChatGPT (GPT-6 builder), 2026-11-11.
