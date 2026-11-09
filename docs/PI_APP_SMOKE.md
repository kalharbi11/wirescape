<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi ARM64 Electron device-off qualification (#446)

This is a **source-build functional smoke**, not owner desktop installation,
physical-display performance, a packaged app, audio/MIDI evidence, or a soak.
It runs only against the checked-out exact PR head on the private `pi5` runner,
with `ci/pi` and **`ci/pi-accept`** on the draft PR. A hosted x86
run, skipped Playwright test, queued private run, or Xvfb render alone does
not establish completed Pi acceptance.

## Owner-managed, one-time prerequisites

Use 64-bit Linux on the Pi 5 with a maintained Node LTS, Python 3,
CMake/Ninja/C++ compiler, Git, `libportaudio.so.2`, and the distro
shared libraries required by ARM64 Electron. Provide a graphical session
with `DISPLAY` (X11 or an **existing** Xvfb) or `WAYLAND_DISPLAY`.
Inspect missing native dependencies with `ldd` on the Electron
binary. Provision any missing packages/display **outside** the smoke as an
explicit owner-managed setup step; agents/scripts never apt, sudo,
pip-install, start Xvfb, alter the session, SSH, or open sound/MIDI devices.

## Exact-head smoke command

```sh
python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke
```

The command refuses a dirty checkout or non-ARM64 host, then runs the
existing native `tools/dev.py build --headless --config Release` and
tracked Studio/Desktop `npm ci` and Desktop `npm run build`
commands. It never fetches or switches the Git ref, installs a desktop
launcher, starts a service or requests real devices. Total owned execution
is bounded to 1500 seconds (under the private runner's 1800-second
per-command limit). An unavailable display or shared library is
**BLOCKED**, never reported as passed.

The existing desktop Playwright harness launches real Linux ARM64 Electron
and `wirescape-engine` with `--null-audio`, no
`--confirm-*`, and an explicit temporary `--profile-dir`.
That app option, only when supplied, sets Electron `userData` and
`sessionData` before readiness, and keeps the sidecar
`--settings` inside the same directory. It also covers window-state,
default patches folder, hello/catalogue, one native command acknowledgement,
UI ready and clean shutdown. Normal owner launches without the option
retain their existing settings and profile paths. A Chromium
`--user-data-dir` switch is **not** evidence of Electron isolation.

## Reports and failure recovery

The ignored, checkout-local report directory contains `summary.json`,
stage logs and `playwright.log`. Failed Playwright screenshots/traces
are under `build_reports/pi-app-smoke/playwright/`. The report
records full source SHA, architecture, display backend, binary SHA256,
stage results and actual test counts. Failure/timeout terminates only the
launched child group and removes the temporary profile. A skipped/zero
test, wrong ELF architecture or missing report is a nonzero exit.
Read the failing stage log, manually provision missing prerequisites and
rerun the *same head*. Xvfb verifies function but not physical Pi display
responsiveness; existing #274 and #296 remain separate.
