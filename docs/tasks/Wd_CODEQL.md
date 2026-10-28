<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-d: CodeQL default setup (#88)

**Done by the planner (2026-10-28):** CodeQL runs as an advanced-setup workflow, `.github/workflows/codeql.yml` (C/C++, Python, Actions; build mode none; `security-extended`; on PRs, `main` and weekly), so no repository setting is needed. Don't also enable "Default setup": the two conflict. **Remaining (ChatGPT):** triage the first scan: dismiss false positives with a reason, open one issue per real finding (with an acceptance block), and adds a line to `docs/WORKFLOW.md` §2 table that CodeQL runs on GitHub, not the Pi.

```acceptance
tier: T0
budget: 1 ChatGPT session
size: <= 20 changed lines, <= 1 file
must_pass:
  - first CodeQL scan on main completed (Security → Code scanning)
  - every alert triaged (dismissed with reason or linked to an issue)
stops: [dismissing an alert without a reason]
```
