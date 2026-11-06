<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1E: Pi + Windows bring-up and evidence (Phase 1, after P1B + P1F)

The owner gets the app running on the Pi with one script, and the Pi runner shows the engine keeps its deadlines while the app UI animates.

**Read first:** `docs/tasks/P1B_ENGINE_SIDECAR.md`, `P1F_DESKTOP_SHELL.md`, `docs/PI_RUNNER.md`, `docs/PI_SETUP.md`, `tools/pi/demo.sh` (reuse its env-file pattern).

**Build:**
- `tools/pi/update-and-build.sh` (owner-run): fetch, switch to a given ref, `tools/dev.py build --headless --config Release`, `npm ci && npm run build` in `apps/studio`, `npm ci` in `apps/desktop`. It stops on the first error and prints the app command. Only incremental rebuilds after the first run.
- `docs/PI_SETUP.md` "Wirescape app on the Pi": one-time prerequisites (Node LTS, build tools, `libportaudio2`), `update-and-build.sh`, `install-desktop-entry.sh` (from P1F), and where settings and logs live.
- The same steps for Windows in `docs/WINDOWS_DEMO_UPDATE.md` (engine via `tools/dev.py build --headless`, the PortAudio DLL as today, `tools/windows/Wirescape.cmd`).
- **Pi runner (in `kalharbi11/wirescape-pi`, its normal PR process):** (a) build engine + UI + app, `ctest -R sidecar_`; (b) a **contention benchmark**: 10 minutes with the engine in `--null-audio` at 48 kHz / 256 frames running `class_demo_1_chaos.json`, while the Electron app shows the UI's 20-module stress patch on the runner's display. It reports render p99 and overruns. Pass: zero overruns, p99 < 50% of the period. Without a display session it reports BLOCKED, not pass.

**Owner check (Phase 1 exit; not in must_pass):** see `docs/PLAN.md` "Phase 1 is done when" and the commands in `docs/DEMO_CLASS.md` §6.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_pi_update*.py"
  - pi: build + ctest -R sidecar_
  - pi: contention benchmark (zero overruns, p99 < 50% period) or BLOCKED with reason
stops: [device access in tests, network listener, changing Pi services, new dependency]
```
