<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Merge gate: count only standalone, unquoted approval lines (#173)

**Why:** `tools/merge_gate.py` accepts a gate phrase anywhere in an owner comment with the sha7 anywhere in
the body, so a quoted request, an example or a negated sentence can count as approval (#173).

**Do (on PR #175, branch as is):** a comment approves only through a line that, after stripping trailing
spaces, is exactly `<Agent>: approved for auto-merge <sha7>` (case-insensitive phrase, sha7 = current head).
Ignore lines inside fenced code (```` ``` ```` or `~~~`), indented code (4+ spaces or a tab), blockquotes
(`>`), list items, and any line with other text before or after. Keep the owner restriction, labels,
tiers and required checks exactly as they are.

**Read first:** `tools/merge_gate.py`, `tools/foundation/tests/test_merge_gate.py`, #173, review on #175.

```acceptance
tier: T2
budget: 1 Codex session
size: <= 120 changed lines, <= 3 files
must_pass:
  - python -m unittest tools/foundation/tests/test_merge_gate.py
  - tests: valid standalone T1/T2/Claude lines pass; fenced, 4-space, tab, blockquote, list, quoted,
    negated ("not approved for auto-merge"), inline-in-prose and wrong-sha lines all fail
  - ci: guard, static, acceptance green on the exact head
review: T2-Claude (gate code). Independent ChatGPT reviewer line, then "Claude: approved for auto-merge <sha7>" (Claude wrote #175)
stops: [any change to required checks, owner restriction or tier rules; workflow edits]
```
