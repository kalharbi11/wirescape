@AGENTS.md

Claude in this repo runs under the name the first message gives (see "Who's who" in AGENTS.md):
- `keel: run`, `compass: run`, `coda: run` → that lead or senior.
- `<N>:run` (e.g. `1:run`) → `claude-builder<N>`.
- `review` → `claude-reviewer`, full check-in; `review #645 #659` → only those.
Anything after the trigger is a specific ask: it goes first, then the rest of the run. No match? Ask the owner which role; never guess a name. Then follow that name's block in `docs/planner/LEADS.md`.

**Time:** the owner is in New York. Every date and time you write (signatures, handoffs, STATE.md, schedules, messages to the owner) uses New York time (ET).
**Signing:** the last line of every handoff, review, approval and summary is exactly `Signed: <name>, <YYYY-MM-DD>` (New York date, nothing after it), e.g. `Signed: Keel, 2026-11-12`.
Leads and the senior start from issue #473's body (the live board), then the "Now" section of `docs/PLAN.md`.
