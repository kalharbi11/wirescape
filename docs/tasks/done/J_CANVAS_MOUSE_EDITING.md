<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Canvas mouse editing (#183)

Implement the owner-observed #182 P0-B task exactly as issue #183 specifies.
Read Main.qml, ModuleCard.qml and native/ui/tests/ui_tests.cpp. Preserve layout
geometry from #145 and the current audio-running topology policy (#186 is separate).
Exercise actual press/move/release at 100%, 85% and 62% zoom: module movement,
VCO → VCA → Output, disconnect/reconnect, LFO → CV, and saved link persistence.
No devices, new dependencies, patch-format or ID changes.

```acceptance
tier: T1
budget: 1 bounded fix and CI follow-through
size: <= 300 changed lines, <= 6 files
must_pass:
  - ci: windows-x64 studio_ui_tests including canvasMouseEditing at all three zooms
  - ci: linux-arm64, sanitizers, guard
stops: [patch format change, ID change, new dependency, physical device access]
```
