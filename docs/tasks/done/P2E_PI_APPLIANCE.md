<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2E-a (#260): Pi appliance autostart and startup timing

Read issue #260, docs/PI_SETUP.md and ENGINE_PROTOCOL.md status envelope.
Per-user install/remove reuses the existing launcher; scripts never edit system files.
Owner scheduling setup is documented only. Startup timing logs first hello and
first status.audio.state=running once from app.whenReady, using the normative envelope.
Fake-HOME unit tests and device-free Electron stub tests cover behavior.
The task adds an acceptance file (7 files versus the issue's 6) within the
workflow's permitted 50% allowance; the 250-line budget remains unchanged.

```acceptance
tier: T1
size: <= 250 changed lines, <= 7 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_pi_autostart.py"
  - ci: web job green incl. boot timing Electron test
stops: [device access, system service edits, new dependency]
```
