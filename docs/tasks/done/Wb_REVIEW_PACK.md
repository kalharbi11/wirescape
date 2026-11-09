<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-b: `tools/review_pack.py <PR#>` (#86)

**Why:** every reviewer (ChatGPT, Claude, the external queue) reads one small file instead of exploring.

**Do:** one Markdown file under a cap (default 60 KB; `--split N` makes N files ≤ cap/N with the prompt at the top of the first): PR title/intent, diff stat, the diff with generated/fixture/binary noise trimmed (list what was trimmed), the task's acceptance block, the latest CI + accept summary (REST API), and a fixed checklist (realtime, IDs, state/versioning, thresholds, licences/SPDX, devices). `--prompt chatgpt|claude|external` selects the header prompt. Output under `build_reports/review/`. Uses `GITHUB_TOKEN`; works offline with `--diff <file>` for tests.

```acceptance
tier: T0
budget: 1 Codex session
size: <= 400 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_review_pack*.py"   # cap respected, noise trimmed, split works
  - python tools/review_pack.py --diff tools/foundation/tests/fixtures/sample.diff --task docs/tasks/J7_CHAOS_LITE.md
  - ci: linux-arm64, windows-x64, guard green
stops: [new dependency]
```
