<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-d: CodeQL default setup (#88)

**Do:** the user enables Settings → Code security → Code scanning → CodeQL analysis → **Set up → Default** (languages C/C++, Python, Actions; build mode none). The agent then triages the first scan: dismiss false positives with a reason, open one issue per real finding (with an acceptance block), and adds a line to `docs/WORKFLOW.md` §2 table that CodeQL runs on GitHub, not the Pi.

```acceptance
tier: T0
budget: 1 ChatGPT session
size: <= 20 changed lines, <= 1 file
must_pass:
  - user: CodeQL default setup enabled; first scan completed
  - every alert triaged (dismissed with reason or linked to an issue)
stops: [dismissing an alert without a reason]
```
