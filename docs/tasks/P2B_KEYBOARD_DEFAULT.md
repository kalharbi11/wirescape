<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2B-d (#289): global computer-keyboard default

Read #289, #184's completed global MIDI implementation, #283, Studio AGENTS,
Keyboard.ts, App.ts Settings and existing preferences. This is a T1 UI preference
for global musical-key shortcuts, distinct from MIDI device enablement. It starts
on, remembers an explicit off choice, releases computer sources on disable, and
does not disable screen keys or open devices. Preserve typing/modal guards and
the existing engine note protocol. Source ownership is shared with #283.

Owner explicitly requested starting now; this PR stacks on #283 until that
prerequisite merges. The design note is a sixth file within the workflow's
permitted50% file allowance; the300line cap is unchanged. Do not merge before
#283. A dependency diff is not this task's acceptance size.

```acceptance
tier: T1
size: <= 300 changed lines, <= 6 files
must_pass:
  - bash -c "git diff --check"
  - ci: full Web typecheck, production build, Playwright and desktop Electron suites green
stops: [engine or protocol change, device access, new dependency, threshold loosening]
```
