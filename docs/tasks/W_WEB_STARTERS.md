<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-web-2 (#312), with W-web-3/4/5: starters, telemetry, share, files

In the web demo (`window.wirescape.web`), the starter menu lists only the
starters whose modules are all in the engine's hello catalogue. The library and
Quick Add already follow the catalogue, because RealEngine replaces the
registry on hello. Desktop and FakeEngine keep the full list. An empty
catalogue hides nothing, and saved browser patches are never modified.

```acceptance
tier: T1
size: <= 500 changed lines, <= 12 files
must_pass:
  - ci: web-demo engine job green (native web_engine_tests incl. telemetry)
  - ci: web-demo site job green with focused tests-web starter/catalogue, share, download and telemetry coverage
  - ci: existing web job green (FakeEngine/Electron unchanged)
  - bash -c "git diff --check"
stops: [engine/protocol/ABI change, new dependency, silently modifying unsupported saved patches]
```
