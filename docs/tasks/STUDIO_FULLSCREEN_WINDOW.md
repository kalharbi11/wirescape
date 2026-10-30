<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio window controls (#148): fullscreen and minimize

Owner request: add compact, clearly labeled Full screen (toggle to Windowed) and Minimize actions to the existing top toolbar; add the `--fullscreen` CLI option; pressing Escape exits fullscreen. Use Qt window states and the native OS restore mechanism, with no fake state and no audio/device startup.

Read first: `native/ui/qml/Main.qml` (toolbar and existing Escape handling only), `native/ui/qml/StudioButton.qml`, `native/ui/main.cpp`, and `native/ui/tests/ui_tests.cpp`. Coordinate shared QML file with #145; do **not** change the sidebar, legend, library, cable curves, or viewport. No ABI, DSP, port/parameter IDs, patch formats, added dependencies or platform-specific hardware access.

Verification: Device-free offscreen `studio_ui_tests` covers the Full screen button, its reversible toggle, Escape exiting fullscreen even when a child control has focus, and Minimize action. The Windows Qt `windows-x64` job is the build and acceptance authority. Test the `--fullscreen` parsing and window state, preserve normal launch and existing keyboard/unsaved dialog paths. UI must remain usable at minimum 1000×650 and standard desktop sizes.

```acceptance
tier: T1
budget: bounded ChatGPT task
size: <= 260 changed lines, <= 4 files
must_pass:
  - ci: windows-x64 green on exact head, with studio_ui_tests passing offscreen and window control behaviors verified
  - ci: static analysis green on exact head
stops: [ABI header edit, new dependency, format or ID change, tests removed, unapproved threshold change, >4 files]
```
