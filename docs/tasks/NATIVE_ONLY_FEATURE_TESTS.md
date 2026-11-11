<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Native-only feature tests (#566)

Read first: AGENTS.md, board #473 owner mandate and issue #566.
Owner retires Qt-specific test scaffolding in current feature work; broad
CI/legacy Qt cleanup follows the current queue. No new Qt compatibility tests.
Keep this product test independent of Qt-aware includers or Qt installations.

Remove only the synthetic emit-macro probe from groove_runtime_tests.cpp.
Keep its direct Groove DSP include and every graph/config/reload/allocation
assertion. Production headers and analyzer/CI policy stay outside this patch.
Existing Chord and Notes-to-CV writers handle their own scoped test scaffolding.
Owner authorization in #566 covers retirement of this compatibility probe.

```acceptance
tier: T0
size: <= 90 changed lines, <= 2 files
must_pass:
  - python -c "from pathlib import Path; s=Path('native/document/groove_runtime_tests.cpp').read_text(); assert '#define emit' not in s and '#ifndef emit' not in s and 'must restore' not in s; assert '#include \"../modules2/groove/dsp.h\"' in s"
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R groove --output-on-failure --no-tests=error"
  - ci: guard, linux-arm64, windows-x64, sanitizers and static green on the exact head
stops: [native behavior assertion removal, threshold change, production DSP or ABI edit, CI policy edit, broad Qt deletion, new dependency]
```

Patch guide impact: none — retired compatibility probe, native behavior unchanged.
Signed: Codex Lead (Codex), 2026-11-11
