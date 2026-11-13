<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W gate v4 (#236): agent-neutral approval lines (workflow v3)

Make `tools/merge_gate.py` match workflow v3 (`AGENTS.md`, `docs/WORKFLOW.md` §4): any agent may approve, every approval is signed, and nothing is reserved for one agent.

**Change:**
- Accept a standalone `Approved for merge <sha7>` line, with the same parsing rules as today: owner account only, outside code, quotes and HTML comments, current head only. Require a `Signed: …` line in the same comment.
- **T0:** unchanged (ready label + green checks). **T1/T2:** an approval line is required. The gate can't verify model families; record the signer in the gate's output so it shows in the Actions log.
- **Protected paths** (the current `CLAUDE_PATHS` list): an approval line plus the owner's `allow-protected` label, or the issue's `Protected path named:` line, which the guard already checks. Remove the Claude-only requirement and the "Claude-authored control change needs ChatGPT" rule; T2 cross-family review covers it.
- Keep accepting the old `<Name>: approved for auto-merge <sha7>` lines for 30 days, then remove them in a follow-up.
- Update `tools/foundation/tests` (or the gate's existing tests) for every rule. No test removal: old-line tests stay while the old lines are accepted.

```acceptance
tier: T2
size: <= 250 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_merge_gate*.py"
  - ci: guard green
stops: [branch protection change, weakening the head-SHA or owner-account checks]
```
Protected path named: `tools/merge_gate.py`
