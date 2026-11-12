<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC slice A: portable codec and OSC In/Monitor modules (#525)

Issue #525 selects this bounded T1 subset of parent #72; PR #518 must close
#525 only. The parent remains open for Keel-ruled sidecar host/consent (B),
Studio/Monitor UI and protocol (C), then #94 fuzzing.

## Scope and read first
- Read `AGENTS.md`, #72's 2026-11-09 Keel ruling, #525 authorization,
  `docs/tasks/OSC_CURRENT_HOST_72.md`, and the OSC/Monitor module designs.
- Port bounded OSC 1.0 packet, bundle and pattern parsing; `osc.in` and
  `monitor` ABI2 modules, registration, catalogue, offline native tests,
  Studio catalogue/port assertions and patch designer availability guidance.
- Validate a complete bundle before any child callback: malformed later
  children must cause no partial CV writes, event emissions or callbacks.
  Test both codec callback counts and OscIn CV/event state with a valid first
  child followed by a malformed second child; include nested bundles and
  burst/capacity bounds. Keep bounded work and no allocation, locks, I/O or
  logging in render.
- No UDP socket, LAN consent, sidecar lifecycle, `--confirm-lan`, Electron
  forwarding, status/telemetry protocol, SDK/ABI or new dependencies.
  Registration alone does not make live OSC ingress or Monitor UI available.
- Independent different-session T1 source review and exact-head executed
  acceptance required. The selected acceptance issue is #525, not #72 or an
  optional unselected task job. Pi checks must be device-off; no LAN/device
  access. Require actual Windows and Pi execution, not just queued labels.

```acceptance
tier: T1
size: <= 1600 changed lines, <= 30 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'osc|monitor|app_engine|app_patch|ui_catalogue_sync' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - pi-run: ctest --test-dir build/native -R 'osc|monitor' --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [socket or LAN opening, sidecar lifecycle, protocol or ABI change, partial malformed-bundle delivery, test or threshold weakening]
```

Signed: ChatGPT (GPT-6, OSC slice-A builder), 2026-11-10
