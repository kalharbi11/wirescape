<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Lead queue refresh (#517)

Read first: AGENTS.md, docs/planner/LEADS.md, board #473 latest Keel ruling,
#515 and current PR handoffs. Stack on #515; retarget normally after it merges.

Refresh STATE to <=60 lines: remove completed PRs from open rows; record accepted
source cd7999e4a02308df0db918cfbab6974b97b6782f with actual Pi evidence; route current
Chord/MIDI/OSC/Groove work and independent cross-family T2 reviews. Distinguish
D5 #487 from strict-soak #513. Reconcile Notes→CV task to Coda's FIFO ruling and
the senior-authorized 18-file cap, retaining 900 changed lines and every test.
No runtime, shared allocator, protocol, CI, hardware or new dependency changes.

```acceptance
tier: T0
size: <= 220 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; s=Path('docs/planner/STATE.md').read_text(); assert len(s.splitlines()) <= 60; assert all(x in s for x in ('#505', '#500', '#512', '#72', '#513', 'cd7999e4a02308df0db918cfbab6974b97b6782f', 'other', 'family')); n=Path('docs/tasks/NOTES_TO_CV_MODULE_421.md').read_text(); assert 'released FIFO' in n and 'released LIFO' not in n; assert '<= 900 changed lines, <= 18 files' in n"
  - bash -lc 'git diff --check'
stops: [runtime or protocol edits, CI policy edits, changing shared NoteToCv, removing tests or weakening thresholds]
```

Patch guide impact: none — planner and task documentation only.
Signed: Codex Lead (Codex), 2026-11-10
