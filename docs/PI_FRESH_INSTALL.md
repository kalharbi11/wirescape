<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Raspberry Pi 5: fresh source build and first-run checks

**Version 1.0:** the Pi uses this runbook ([overview](INSTALL.md#raspberry-pi-5)).
The last Pi-certified commit is `cd7999e` (below). Once 1.0's own Pi
preflight and app smoke pass, its SHA in [releases/v1.0.md](releases/v1.0.md)
replaces it here. Owner Pi installation follows the Windows 1.0 test. This guide covers Electron + Studio + `wirescape-engine` on
64-bit Raspberry Pi OS. It does not provide a packaged installer.

The source-build tools merged in
[#450](https://github.com/kalharbi11/wirescape/pull/450) (`fcf38b9`), and
the isolated app smoke merged in
[#452](https://github.com/kalharbi11/wirescape/pull/452) (`20aabc0`).
The #463 runtime fixes are also on main. They must pass preflight and the
real ARM64 app smoke at **one combined source revision** before this becomes
an owner-ready recipe. Do not mix their separate branch build outputs.

## Current preparation status (2026-11-10)

The owner's first Windows session is complete. The owner confirmed Node
22.23.4/npm 10.9.9 and Xvfb/xauth for the Pi runner. The cache/read-only,
profile/settings-path and short-ELF review findings have been repaired.

| Gate | Actual evidence | Limit |
|---|---|---|
| #450 preflight | [`1074cfa`: 28 passed, zero blocked](https://github.com/kalharbi11/wirescape/pull/450#issuecomment-6741944378) | Separate PR head |
| #452 app smoke | [`e6df3d2`: ARM64 Electron + native device-off smoke passed](https://github.com/kalharbi11/wirescape/pull/452#issuecomment-6746232670) | Functional display only; separate PR head |
| Combined #463 + #450 + #452 source | [`550b2ea` (docs-only over main `cd7999e`): preflight 28/28 and ARM64 Electron + native smoke passed](https://github.com/kalharbi11/wirescape-pi/actions/runs/42720681421) | Accepted SHA `cd7999e` (Keel evidence-transfer rule, #483) |
| Physical display, sound and MIDI | Owner-only and pending | Xvfb cannot certify physical display performance |

Keep the SHA guard below. #453 must integrate current main, including the
combined-main test compile repair #485/#486, and obtain actual `pi5/accept`
for both preflight and app smoke. Under
[Keel's ruling](https://github.com/kalharbi11/wirescape/pull/453#issuecomment-6746264774),
a passing docs-only head certifies its identical non-doc main parent; only then
may that full main-parent SHA replace the placeholder. Queued, failed,
old-head or partial results are not acceptance.

[Review and evidence notes](reviews/PI_SETUP_READINESS.md) retain historical
failures and their disposition; neither those failures nor cloud mocks replace
the required combined run.

## 1. Inventory and preserve the old installation

Keep the old checkout, patches, profiles, desktop shortcuts and autostart
configuration. Close running Wirescape before the eventual first launch. Use
an independent empty target such as `$HOME/Projects/wirescape-pi-fresh`.
Do not copy old `build`, `.tools`, `node_modules`, `dist` or CMake caches.

These inventory commands do not install or launch the app:

```sh
uname -m
getconf LONG_BIT
python3 --version
node --version
npm --version
cmake --version
ninja --version
df -h "$HOME"
```

Expect `aarch64` (or `arm64`) and 64 bits. An x86 host or a 32-bit Pi OS image
cannot supply Pi app evidence. Record missing tools; do not continue past a
failed prerequisite check.

## 2. Owner-managed prerequisites, when installation is authorized

Use a C++17 compiler, Git, Python (3.12 recommended), pkg-config, CMake 4.2.3,
Ninja 1.13.0, and Node **22 LTS** with its matching npm, following CI and Keel's
ruling. The preflight also accepts maintained LTS majors 24/26; use
22 for this first test. An older runner Node version is an environment blocker,
not a reason to relax the check.

If an existing user-managed nvm installation is available, the owner can select
Node 22 there. Otherwise follow the official
[Node download instructions](https://nodejs.org/en/download) or
[nvm installation instructions](https://github.com/nvm-sh/nvm#installing-and-updating)
for Linux ARM64, then reopen the intended build shell and verify `node --version`
and `npm --version`. Do not uninstall system Node or rewrite global PATH as part
of this experiment. A runner maintainer must arrange the same Node selection for
the runner account; changing an interactive shell alone does not change a service.

On Raspberry Pi OS, common native prerequisites include `build-essential`,
`git`, `pkg-config`, `python3-venv`, `libasound2-dev`, `libportaudio2` and
`libasound2-plugins`. The owner installs only missing packages through the OS
package manager. Electron also needs NSS/GTK/ATK/X11/GBM/ALSA shared libraries;
use the preflight's missing-library report and the distribution's package names.
Bookworm and newer releases can differ on `t64` package names. Do not blindly
copy package names from another distribution or disable loader checks.

The Qt SDK is not a prerequisite for this source app. Full private-runner
legacy document tests have their own Qt Core requirement in
[PI_RUNNER.md](PI_RUNNER.md); do not confuse that with Qt UI installation.

## 3. Select the single accepted source revision

The accepted source is main `cd7999e4a02308df0db918cfbab6974b97b6782f`, validated on the Pi
runner ([run](https://github.com/kalharbi11/wirescape-pi/actions/runs/42720681421)). Use exactly this SHA; a later main needs new Pi evidence.
Run this in a new terminal; all environment changes below affect this shell.

```sh
set -eu
AcceptedSha='cd7999e4a02308df0db918cfbab6974b97b6782f'
case "$AcceptedSha" in ''|*[!0-9a-f]*) echo 'No accepted Pi SHA yet'; exit 1 ;; esac
[ "${#AcceptedSha}" -eq 40 ] || { echo 'Use a full 40-character accepted SHA'; exit 1; }
Root="$HOME/Projects/wirescape-pi-fresh"
[ ! -e "$Root" ] || { echo 'Target exists; preserve it and choose a new empty path'; exit 1; }
mkdir -p "$(dirname "$Root")"
git clone https://github.com/kalharbi11/wirescape.git "$Root"
cd "$Root"
Root=$(pwd -P)
git fetch origin
git switch --detach "$AcceptedSha"
[ "$(git rev-parse HEAD)" = "$AcceptedSha" ]
[ -z "$(git status --porcelain)" ]
[ -f tools/pi/preflight.py ] && [ -f tools/pi/app_smoke.py ] || {
    echo 'Accepted source is missing a required setup tool'; exit 1;
}
unset WIRESCAPE_QT_DIR WIRESCAPE_PORTAUDIO CMAKE_PREFIX_PATH Qt6_DIR
unset QT_PLUGIN_PATH QML2_IMPORT_PATH ELECTRON_RUN_AS_NODE
unset ELECTRON_SKIP_BINARY_DOWNLOAD WIRESCAPE_TEST_STUB
export npm_config_cache="$Root/.tools/npm-cache"
export ELECTRON_CACHE="$Root/.tools/electron-cache"
```

If CMake/Ninja must be provisioned for this user, an owner can use a venv under
the new checkout, preserving the OS Python environment:

```sh
python3 -m venv .tools/build-tools
. .tools/build-tools/bin/activate
python -m pip install cmake==4.2.3 ninja==1.13.0
cmake --version
ninja --version
```

Stop on any download, integrity, permission or tool failure. Do not disable TLS,
ignore lockfile hashes, use `sudo npm`, or substitute an old app's binaries.
First configuration fetches pinned native sources; `npm ci` needs registry access.

## 4. Check, then build (devices stay off)

The commands in sections 4–6 require the accepted combined revision from section 3.

```sh
python3 tools/pi/preflight.py --check-only
sh tools/pi/update-and-build.sh "$AcceptedSha"
```

Preflight is offline and read-only; the earlier first-import write was repaired
in merged #450. A blocked result means stop and follow the named recovery step. The updater fetches and verifies this exact revision,
checks prerequisites again, builds the Qt-free Release engine, runs locked npm
installs, then builds Studio and Desktop. It defaults to **two** native build
jobs; use `--jobs 1` on the updater if memory is constrained. Never use an
unbounded build on the Pi.

Inspect `build_reports/pi-source-build/<sha>/summary.json` and stage logs:
`native`, `studio-ci`, `studio-build`, `desktop-ci`, `desktop-build`. A failed
stage must stop later stages. Keep the transcript/report with the source SHA.
The printed real-device launch command is for a later owner decision.

A genuine own cache must match this checkout's normalized `native/` path,
architecture and generator, with Qt UI disabled. Do not move a runner-generated
cache based on an obsolete detector failure; use the merged preflight's predicates. If a cache truly belongs to another checkout or architecture, inspect it
and preserve it under a different name before rebuilding; never auto-delete it.

## 5. Test the actual app with an isolated temporary profile

Use the Pi's existing graphical desktop terminal. Check that `DISPLAY` (X11)
or `WAYLAND_DISPLAY` is available. If neither exists, the smoke can wrap its owned
app-test child in already installed `xvfb-run -a`. It does not install Xvfb or
alter an existing desktop session; missing display support remains blocked.

```sh
python3 tools/dev.py test
python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke
```

The smoke rebuilds through the existing driver and npm lockfiles without
fetching or switching source. It verifies ARM64 ELF artifacts, real Electron,
UI readiness, native hello/catalogue, a real command acknowledgement, profile
paths, and owned clean shutdown with `--null-audio` and no confirmation flags.
It uses a temporary Electron `userData`/`sessionData` and native settings profile
and removes that test profile afterward. Nothing saved there is a backup.

Require `summary.json` to name the accepted SHA, `status: passed`, ARM64, the
actual display backend and binary hashes, with **one expected test and zero
unexpected, skipped or flaky tests**. Inspect stage logs and Playwright output
under `build_reports/pi-app-smoke/`. Missing display/libraries, wrong ELF,
timeout or skipped tests are blockers. The separate #452 run passed; the
combined selected source still requires this actual Pi execution.

For hosted PR evidence, both `ci/pi` and `ci/pi-accept` are required. Check
`pi5` and `pi5/accept` on the **current full SHA**. A queued run, an old-head
pass, or `pi5` alone does not run the task-specific smoke. Private-run links
require collaborator access. Never SSH into the Pi for agent validation.

## 6. Keep a separate owner test profile and launch manually

After the automated gates pass, a deliberate owner launch can retain test
patches in a fresh profile. Use exactly one settings destination; normally
omit `--settings` so the profile option selects its own native settings file.
Merged #452 validates that the settings destination stays inside this profile.

```sh
Profile="$Root/.tools/owner-first-run-profile"
"$Root/apps/desktop/node_modules/.bin/electron" \
    "$Root/apps/desktop/build/main.js" \
    --engine "$Root/build/native/wirescape-engine" \
    --profile-dir "$Profile" --null-audio
```

Confirm Studio loads, `engine: real` appears, a copy of a native patch loads,
save/reopen works within this profile, and Quit stops the engine. This is still
a device-off check. Back up wanted patches from the profile before any future
cleanup. Normal app launches without `--profile-dir` use the default profile.

Only the owner, after those checks, may relaunch the same command replacing
`--null-audio` with `--confirm-audio --confirm-midi` for the deliberate sound/MIDI
test. Record the selected device, rate/block size, audible result, dropouts and
save/reload result separately. Functional Xvfb results do not certify physical
display responsiveness, real audio or MIDI. The longer #274/#296 checks remain
separate, as does Groove's feature acceptance.

## Installation, rollback and updates

Do not run desktop-entry/autostart helpers during this isolated test: their
current default-profile launcher needs a separate target/profile review before
it can replace an old shortcut. Source build and smoke do not install services,
launchers or a `.deb`/AppImage. No release is published by this guide.

Rollback uses the preserved old checkout and its old launcher; do not copy new
binaries into it. For a later update, choose another accepted full SHA, preserve
patches and use a fresh checkout/profile, then repeat the gates. Closing the
terminal discards the temporary environment choices. The guide does not certify
an idempotent installer or clean-machine distribution.

Signed: Codex (Pi readiness reviewer and owner-guide builder), 2026-11-08 America/New_York.
Updated: Codex (delegated Pi guide repair cooperator), 2026-11-09.
