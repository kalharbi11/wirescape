<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi runner health snapshot (#129)

Add a read-only JSON command for the Pi runner bridge. Report uptime/load, selected memory/swap totals, disk capacity, optional temperature/throttle state, `tailscaled` state, Wi-Fi type/state, selected Tailscale backend/self-online/health count, and selected warning counts in at most 300 current-boot kernel lines.

Never expose peer identities, addresses, SSIDs, connection names, credentials, full environment, raw Tailscale JSON, command errors, or journal lines. Optional commands time out within four seconds and report unavailable/timeout explicitly. The script succeeds when optional tools or readings are unavailable. The result is a snapshot, not a diagnosis. No SSH, network probes, device tests, restart, sudo, configuration change, watchdog, or new dependency.

Self-decisions (review later): cap240 lines (+20% under WORKFLOW section3c)
for missing-command/permission and null-field regressions; original3-file cap stays.

## Acceptance

```acceptance
tier: T0
budget: 1 session
size: <= 240 changed lines, <= 3 files
must_pass:
  - python -m unittest tools.foundation.tests.test_runner_health
  - python -m pip install ruff==0.14.1
  - python -m ruff check tools/runner_health.py tools/foundation/tests/test_runner_health.py
  - pi-run: python tools/runner_health.py
stops: [new dependency, scope expansion, sensitive output]
```
