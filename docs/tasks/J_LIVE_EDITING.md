<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Live editing and transport behavior (#186)

Follow issue #186 and owner report #182, after canvas interaction fix #183.
Diagnose silent-after-Start: distinguish keyboard focus, gate, graph and device.
Start with a real Main.qml transport/mouse/keyboard regression using a fake Studio
object; tests must never open devices. Return keyboard focus after transport use.
Then allow add/move/connect/disconnect while running via a quiescent graph swap,
with no realtime allocation or destruction and a documented bounded transition.
Preserve failed-edit rollback, connections, held-note/release handling and explicit
device safety. The existing one-second teardown timeout is not an audio dropout
bound. Never auto-start physical audio in CI. No new public ABI or IDs.

```acceptance
tier: T2
budget: 2 bounded implementation passes
size: <= 700 changed lines, <= 10 files
must_pass:
  - ci: windows-x64 studio_ui_tests transportStartReturnsKeyboardFocus using fake-only transport
  - ci: offline/fake audio live-edit tests: bounded transition, zero process allocations, preserved connections and failed-edit rollback
  - ci: windows-x64, linux-arm64, sanitizers including TSan, guard
  - user: live patching on Windows
stops: [ABI header edit, threshold change, device access in tests, new dependency]
```
