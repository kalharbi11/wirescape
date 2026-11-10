<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Usage proposals deferred to the big cleanup (owner, 2026-11-10)

From the owner's workflow-v4 review (2026-11-10). The doc-only parts were adopted in AGENTS.md, docs/planner/LEADS.md and docs/WORKFLOW.md §8. The owner wants to study the items below first and do them after the current tasks and the cleanup (R3–R7). Don't start them without an owner or Compass go.

## Automation (needs owner study first)
- `needs/quick-ok` label + `.github/workflows/quick_ok.yml` that calls a low-effort reviewer (`@codex review` or the Claude Code Action), routed by the author's model family. T0/T1 only; T2 stays a batch. Check billing and trigger syntax first.
- `tools/board_bot.py` on a cron workflow (zero tokens): close merged issues, fix labels, stale-claim comments, retargets, task-file moves, CI pre-triage. Helper lanes 1–2 would then shrink to "real" failures only.
- `review_pack.py --check` as a CI job for the greppable pre-review items (SPDX, tier vs paths, tests with code, size budget, Patch guide impact line, device opens, alloc/lock/log in process).

## Read surface (docs moves; fits the cleanup)
- Nested `native/AGENTS.md` (realtime, ABI, module/DSP, Pi evidence) + `CLAUDE.md` with `@AGENTS.md`; shrink root AGENTS.md again.
- Skills in `.claude/skills/` (and matching Codex prompt files): `handoff`, `review`, `pi-evidence`, `patch-design`, `t2-review`.
- `docs/CODEMAP.md` (~60 lines): path → purpose → owner tier → tests.
- Delete `docs/tasks/done/` at the archive tag (index keeps one line per issue); trim `docs/reviews/` to rulings; move old `docs/design/` studies at the archive tag.
- Split `docs/PLAN.md` into PLAN (Now + phases) and `DECISIONS.md` (one line per decision); collapse superseded decisions.

## Code layout (R6/R7 and after)
- R7 renames (`modules2→modules`, `engine2→engine`, `tests2→tests`) right after R6.
- Module folder template; drop per-module `LICENSE` and generated `TEST_REPORT.json` if confirmed generated.
- Generate the capability sections of `PATCH_DESIGNER_GUIDE.md` from `module.json` with a drift check.
- Consolidate `tests/`, `native/tests`, `native/tests2`; move loose `tools/` scripts into subfolders; `tools/workflow/` for review_pack, merge_gate, board_bot.
- `.github/copilot-instructions.md`: delete or point at AGENTS.md.

Measure after adopting: PRs merged per Claude window, owner relay messages per day.
