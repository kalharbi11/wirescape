<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Claude check-ins: routine (Sonnet, every 4 h) and escalation (Opus)

Every check-in is a **fresh session**. Never continue an old chat. State lives in `STATE.md` and on #29.
Tools: the `mcp__github__*` tools (no `gh` CLI), REST-style, never GraphQL. Tail logs, don't read them whole.

## Routine check-in (Sonnet)
1. Read `STATE.md`, including **Deferred for Opus**. Then the newest 3 comments on #29, then open PRs
   (number, labels, head sha7, base, draft).
2. Do what Sonnet may do (`docs/WORKFLOW.md` §3):
   - set `status/ready-to-merge` after a green acceptance run
   - post T0/T1 gate lines, and Claude lines for planner-doc PRs that touch no CLAUDE_PATHS
   - label hygiene, retarget stacked PRs, answer questions the task file already answers
3. Sort every item that needs Opus (T2-Claude gate, `needs-planner`, a Stop, a disputed High, a plan or
   setup decision) into **urgent** or **can wait**:
   - **Urgent:** fewer than 2 agents (Codex, UI, docs) have unblocked work in the queue; or the item blocks
     #182 Jam work or the merge gate; or it has waited 24 h or more; or it is a security finding.
   - **Can wait:** everything else. Add one line to **Deferred for Opus** in `STATE.md`
     (`- #N sha7: question (since YYYY-MM-DD HH:MM)`) and move on. Don't escalate it.
4. If anything is urgent, **start a new session** with `create_session` (model `claude-opus-5-5`, the same
   environment) using the escalation prompt below. Put every urgent item **and** all deferred items in
   one escalation, then empty **Deferred for Opus**. At most one escalation per check-in.
5. If nothing changed, post nothing. Otherwise post one short comment on #29 and update `STATE.md`
   (small PR, T0, ready label) only when something merged, was deferred or was escalated.

## Escalation prompt (paste as the new session's prompt)
```
Wirescape planner escalation (Opus). Read docs/planner/CHECKIN.md and docs/planner/STATE.md,
then decide ONLY these items; no full sweep:
- #<N> head <sha7>: <the question in one line>. Reviewer: <verdict + link>. CI: <summary link>.
- ...
Use the mcp__github__ tools, REST only. For each item, post a ruling, or post an approval line when it
is sound. Then post one #29 queue comment, update STATE.md only if state changed, and stop.
```

## Daily planner check-in (Opus, 17:30 New York)
Same as the escalation, but the items are **Deferred for Opus** plus the queue refill (fewer than
3 `status/todo` items). Post one #29 queue comment and rewrite `STATE.md`.
