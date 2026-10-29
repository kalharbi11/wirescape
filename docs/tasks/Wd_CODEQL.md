<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-d: CodeQL default setup (#88)

**Done by the planner (2026-10-28):** CodeQL runs as an advanced-setup workflow, `.github/workflows/codeql.yml` (C/C++, Python, Actions; build mode none; `security-extended`; on PRs, `main` and weekly), so no repository setting is needed. Don't also enable "Default setup": the two conflict. **Remaining:** ChatGPT documents in `docs/WORKFLOW.md` §2 that CodeQL runs on GitHub, not the Pi. The user owns first-scan alert triage because the GitHub connector cannot read code-scanning alerts; dismiss false positives with a reason and open an acceptance-scoped issue per real finding.

```acceptance
tier: T0
budget: 1 ChatGPT session
size: <= 20 changed lines, <= 2 files
must_pass:
  - ci: first CodeQL scan on main completed (Security → Code scanning)
  - user: every CodeQL alert triaged (dismissed with reason or linked to an issue)
stops: [dismissing an alert without a reason]
```
