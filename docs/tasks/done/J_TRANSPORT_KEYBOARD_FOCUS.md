<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Transport keyboard focus (#201)

Independent bug fix extracted from186/194. Clicking transport left audioButton
focused; the keyboard handler requires keyboardSurface. Invoke the existing
focus helper after transport and retain actual mouse/key regression using a fake
Studio object and real Main.qml. Never construct an audio/MIDI backend in it.
No topology/layout/ABI/IDs changes;186 keeps its full live-edit acceptance.

```acceptance
tier: T1
size: <= 150 changed lines, <= 3 files
must_pass:
  - ci: windows-x64 studio_ui_tests including transportStartReturnsKeyboardFocus with fake transport
  - ci: linux-arm64, sanitizers, guard green on exact head
stops: [device access, test removal, threshold change, new dependency]
```
