<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Native event cable identity in Studio (#198)

Core dependency for #187 / PR #192. Preserve native event port identity instead
of treating every non-audio port as control. Read app_engine facade metadata,
Studio modules/validateDocument/connectPorts and controller_failure_tests.
Expose native descriptors only through the existing internal non-ABI facade;
use them for event kinds while keeping legacy audio/control hints intact.
Load/save/reload v2 event cables unchanged; reject mixed signal/event edits
transactionally. Do not edit the UI agent's QML/tests or relax runtime validation.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - ci: windows-x64 controller_failure_tests: event load/metadata/connect/save/reload and mixed-kind rollback
  - ci: linux-arm64, sanitizers, guard
stops: [public ABI header edit, patch schema or ID change, device access, new dependency]
```
