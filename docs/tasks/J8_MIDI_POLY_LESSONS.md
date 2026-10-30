<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J8 follow-up (#165): MIDI, polyphony, standalone tests, and Pi evidence in the SDK guide

Update `docs/SDK_AUTHOR_GUIDE.md` so a module author can follow committed examples for poly note allocation and MIDI/CC modules, run a module's own callback tests with allocation watching, and request task-specific offline Pi evidence. Keep the guide API source-grounded; #163's coordinator/Learn/mapping API is planned and must not be described as available.

Read the issue #165 handoff, `docs/SDK_AUTHOR_GUIDE.md`, `docs/tasks/J8_AUTHOR_GUIDE.md`, `docs/PI_RUNNER.md`, `docs/tasks/J7_CHAOS_LITE.md`, and only the named source/example paths referenced in the guide. The CHAOS result is workload evidence for its recorded run, not a general module guarantee or device-latency result. Agents and CI do not open physical audio or MIDI devices; hardware checks are user-only.

Manually verify every introduced code path and command against the accepted PR base, and report the audit; #163 must remain identified as planned.

Keep this documentation-only change within 140 added/changed lines and 3 files. Do not change APIs, implementation, tests, thresholds, dependencies, or CMake. Do not run a Pi/device test.

```acceptance
tier: T0
budget: 1 bounded documentation pass
size: <= 140 changed lines, <= 3 files
must_pass:
  - python -m unittest tools/foundation/tests/test_doc_links.py
  - git diff --check
stops: [scope exceeds size budget, source reference absent from main]
```
