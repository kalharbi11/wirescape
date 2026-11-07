<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2D-d: explicit crash recovery (#295)

Read #295, App autosave lifecycle, RealEngine reconnect handling, Modal and
apps/desktop/DESIGN.md. Distinguish unclean renderer termination from pagehide;
offer the validated last autosave after unclean startup or engine exit. Suspend
autosave while the choice is pending. Restore keeps unknown data/name; decline
keeps the current document. Escape/close means decline. Never save a user file
or request an engine restart; main retains its crash-loop budget and authority.
Ordinary reload/clean shutdown keeps existing automatic last-session restore.

```acceptance
tier: T1
size: <= 400 changed lines, <= 7 files
must_pass:
  - ci: full Web/Electron regressions for app/engine crash offer, accept/decline, corrupt/missing snapshot, recovery without file overwrite and crash-loop limits
stops: [ABI header edit, ID/format change, new dependency, real devices in tests, threshold loosening]
```
