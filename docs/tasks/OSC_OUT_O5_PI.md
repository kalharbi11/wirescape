<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #734 OSC Out O5: Pi evidence and the owner Pi check

Read first: `docs/specs/OSC_OUT.md` §6, `docs/PI_RUNNER.md`, #731's `pi:` line.

- After O2 (#731) merges: one `ci/pi-soak` run on `main` with null audio, the `osc-out-01` patch, and `tools/examples/osc_out_receiver.py --count-seconds 60`. Record messages per second per row (≤ rate + 1), gate edges are paired, `dropped` is 0, engine CPU and dropouts. Link the private run.
- Then batch the owner check on #581: copy-paste steps on the Pi (start the receiver, load `osc-out-01`, move the LFO rate, play notes) plus one line on the same steps for Windows.

```acceptance
tier: T0
size: <= 60 changed lines, <= 2 files
must_pass:
  - bash -c 'git diff --check origin/main...HEAD'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [real device, LAN or SSH, a threshold set after seeing the result]
```

Patch guide impact: none — evidence only.
Signed: Keel, 2026-11-13
