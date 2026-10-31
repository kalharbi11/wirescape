<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows launcher runtime and active checkout (#188)

Source: owner request #188 / #182. Keep the existing package and launch flows.
Add explicit and remembered validated Qt/PortAudio selection, show the chosen
paths and provenance, and permit overrides. No blind DLL load or filesystem scan.
Shortcuts must use a package inside the active checkout, whose manifest records
that checkout and matches its executable digest. Keep unrelated shortcut protection.
Provide an explicit Check for updates command comparing the installed build SHA
against latest main; open the existing assisted instructions rather than installing.
Preserve normal interactive MIDI preferences; never add confirmation/device flags.

Read first: tools/dev.py runtime/package commands, foundation/package_inventory.py,
install_windows_shortcuts.ps1, docs/WINDOWS_DEMO_UPDATE.md.

File/line self-decision: WORKFLOW3c allows +50% lines (500→750) for the
Windows launch fixture and modified-build regression; file cap remains8.

```acceptance
tier: T1
size: <= 750 changed lines, <= 8 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p 'test_launcher*.py'
  - ci: windows-x64 green
  - user: package with the real PortAudio DLL, launch the generated shortcut and confirm MIDI preference honoured
stops: [new dependency, protected path, devices in tests]
```
