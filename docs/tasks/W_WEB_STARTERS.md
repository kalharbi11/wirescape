<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-web-2 (#312): catalogue-filtered browser starters

In the web demo (`window.wirescape.web`), the starter menu lists only the
starters whose modules are all in the engine's hello catalogue. The library and
Quick Add already follow the catalogue, because RealEngine replaces the
registry on hello. Desktop and FakeEngine keep the full list. An empty
catalogue hides nothing, and saved browser patches are never modified.

```acceptance
tier: T1
size: <= 250 changed lines, <= 5 files
must_pass:
  - ci: web-demo site job green with focused tests-web starter/catalogue coverage
  - ci: existing web job green (FakeEngine/Electron unchanged)
  - bash -c "git diff --check"
stops: [engine/protocol/ABI change, new dependency, silently modifying unsupported saved patches]
```
