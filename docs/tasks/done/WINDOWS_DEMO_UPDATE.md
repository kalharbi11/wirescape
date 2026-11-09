<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows: accepted-build update, packaging and shortcut guide (#155)

Document a repeatable Windows workflow for building one explicitly accepted source SHA, staging a local portable package, checking normal-window startup with audio off, and rebinding the existing Desktop and Programs shortcuts. The guide must state that QML is compiled, `launch` does not build, and packages use timestamped directories. It must not claim deployment or clean-machine compatibility.

Include the repository feature-state table as audited 2026-10-30, with exact PR/commit evidence: #149 merged, #153 pending, #145/#147 open and unmerged, #81/#124 pending. Explain that a build from a SHA cannot include unmerged branches. Give copyable PowerShell using only real CLI flags, capture the package path from `package --json`, run `startup-test --portable`, and invoke `tools/install_windows_shortcuts.ps1 -Package`. Do not choose a package by timestamp or add fixed user paths. Preserve old package folders so the user can rebind shortcuts to return; patch files remain wherever the user saved them. Include the user-only listening checklist for USB output, keyboard/MIDI, Panic/hotplug, CC, and save/reopen. No screenshots unless captured from the actual laptop; absent laptop evidence stays pending.

```acceptance
tier: T0
size: <= 220 changed lines, <= 2 files
must_pass:
  - python tools/dev.py build --help
  - python tools/dev.py package --help
  - python tools/dev.py startup-test --help
stops: [runtime change, new dependency, protected path, invented build or hardware evidence]
```
