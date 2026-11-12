<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #686 — CI speed 2: acceptance size check counts authored lines only; parser accepts inline comments

Side lane (owner, 2026-11-12). Owner: `claude-builder2`. Reviewer: gpt-builder3 or Coda.

**Read first:** `tools/foundation/acceptance.py`: `parse` (about line 40) and `size_check`
(about line 156); `tools/foundation/tests/test_accept.py`.

## Steps
1. `size_check` leaves these paths out of the line and file counts:
   - `apps/studio/public/wirescape_modules.json`
   - `apps/studio/public/catalogue-presentation.json`
   - `docs/tasks/**`
   - `docs/planner/STATE.md`

   Put them in one named tuple at the top of the module, so adding a generated file later is a one-line
   change. The report adds `"excluded": [{path, lines}]` so reviewers can see what was skipped.
   Binary detection still applies to every path.
2. `parse`: a trailing ` # …` (space-hash) after a field value or a `- command` is a comment.
   Strip it before matching. Leave `#` inside quotes and `#123`-style issue references alone, and add a
   test for each case. Full-line comments already work; keep them working.
3. Tests in `test_accept.py`, using the module's existing helpers:
   - an over-budget diff made only of excluded paths passes;
   - one authored line over the limit still fails;
   - `tier: T1  # note` parses;
   - `- python x.py  # why` keeps the command `python x.py`.

Not in scope: budget numbers, making size advisory, format-only commit detection
(follow-up if the owner wants it).

**Self-review must say:** `Patch guide impact: none — tooling only`.

**Stops:** loosening any other acceptance check; changing how `must_pass` commands are run.

```acceptance
tier: T1
size: <= 200 changed lines, <= 3 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_accept.py
  - python -m unittest discover -s tools/foundation/tests -p test_acceptance_preflight.py
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [other acceptance checks loosened, command execution changed]
```
