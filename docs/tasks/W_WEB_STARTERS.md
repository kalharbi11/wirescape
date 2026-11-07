<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-web-2 (#312), with W-web-3/4/5: starters, telemetry, share, files

In the web demo (`window.wirescape.web`), the starter menu lists only the
starters whose modules are all in the engine's hello catalogue. The library and
Quick Add already follow the catalogue, because RealEngine replaces the
registry on hello. Desktop and FakeEngine keep the full list. An empty
catalogue hides nothing, and saved browser patches are never modified.

The same PR also carries W-web-3/4/5 from `docs/tasks/W_WEB_DEMO.md`:
- **W-web-3:** cable signal values (`outputs`) and the Output scope come from the wasm engine.
- **W-web-4:** SHARE puts the gzipped patch in `#p=`, and a link opens that patch. Malformed links fall back to the normal start.
- **W-web-5:** SAVE downloads JSON, and OPEN uses the browser file picker.

It also adds a post-deploy `live` CI job that runs `tests-web` against the published Pages URL.

```acceptance
tier: T1
size: <= 500 changed lines, <= 14 files
must_pass:
  - ci: web-demo engine job green (native web_engine_tests incl. telemetry)
  - ci: web-demo site job green with focused tests-web starter/catalogue, share, download and telemetry coverage
  - ci: existing web job green (FakeEngine/Electron unchanged)
  - bash -c "git diff --check"
stops: [engine/protocol/ABI change, new dependency, silently modifying unsupported saved patches]
```
