<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Optional fullscreen launch at desktop login

The owner installs this launcher on the Pi after testing an accepted build.
Agents do not change the Pi. This starts the Studio window, not audio playback,
MIDI acquisition, autologin, a system service or a headless session. It requires
a logged-in desktop that honors XDG autostart. If that session does not process
XDG launchers, use the executable manually; do not replace the desktop setup.

Use the exact accepted demo SHA recorded on [issue #150](https://github.com/kalharbi11/wirescape/issues/150).
Current `main` alone is not proof that MIDI/CHAOS integrations are included.
Keep the persistent checkout at `$HOME/wirescape-demo`, outside the Actions
runner workspace, whose temporary source/build directories are cleaned.

## Prepare the persistent build

For a new checkout only:

```sh
git clone https://github.com/kalharbi11/wirescape.git "$HOME/wirescape-demo"
```

Replace `ACCEPTED_SHA` below before running. The subshell stops on an error
without closing the parent terminal. Preserve any existing checkout edits.
Use the existing C++17 compiler, CMake, Ninja, Python and installed Qt 6 Quick/Test
SDK. Audio later needs the PortAudio runtime `libportaudio.so.2`. See the
[Pi integration task](tasks/J3_PI_AUDIO.md) for the remaining integration scope.

```sh
(
set -eu
cd "$HOME/wirescape-demo"
test -z "$(git status --porcelain)" || { echo 'Save checkout changes first'; exit 1; }
git fetch origin
git switch --detach ACCEPTED_SHA
CMAKE_BUILD_PARALLEL_LEVEL=2 python3 tools/dev.py build --config Release
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir build/native \
  -R '^(studio_ui_tests|controller_failure_tests|audio_device_tests)$' \
  --output-on-failure --no-tests=error
)
```

Stop if any command fails. These offscreen tests do not prove physical sound or
controller operation. The Release executable is
`$HOME/wirescape-demo/build/native/Release/wirescape_studio`; its module directory
is `$HOME/wirescape-demo/build/native`. Keep this checkout while its launcher is installed.

## Owner: install at login

The tracked [desktop template](../tools/pi/wirescape-kiosk.desktop) is disabled.
After the build/tests pass, this owner-run snippet verifies GUI provenance,
writes absolute paths into a per-user copy and enables only that copy. It refuses
to overwrite an existing launcher; no `sudo`, reboot or service change is needed.

```sh
cd "$HOME/wirescape-demo"
python3 - <<'PY'
from pathlib import Path
import sys

root = Path.cwd()
sys.path.insert(0, str(root / 'tools'))
import dev

build = root / 'build/native'
exe = build / 'Release/wirescape_studio'
if not exe.is_file():
    raise SystemExit('Release Studio executable is missing; stop here.')
problem = dev._launch_freshness_error(exe, 'Release')
if problem:
    raise SystemExit('Build verification failed: ' + problem)
if any(c in str(root) for c in ('"', '%', '\n', '\r', '\\', '$', '`')):
    raise SystemExit('Use a checkout path without desktop-entry special characters.')
destination = Path.home() / '.config/autostart/wirescape-kiosk.desktop'
destination.parent.mkdir(parents=True, exist_ok=True)
text = (root / 'tools/pi/wirescape-kiosk.desktop').read_text()
text = text.replace('Exec=wirescape_studio --fullscreen',
                    f'Exec="{exe}" --fullscreen --modules "{build}"')
text = text.replace('TryExec=wirescape_studio', f'TryExec={exe}')
text = text.replace('Hidden=true', 'Hidden=false')
with destination.open('x', encoding='utf-8') as output:
    output.write(text)
print('Installed', destination)
PY
desktop-file-validate "$HOME/.config/autostart/wirescape-kiosk.desktop"
```

This does not launch Studio immediately. Log out/in when convenient, or manually
open the executable above with `--fullscreen --modules` and the module directory.
Press **Esc** to leave fullscreen; the toolbar can re-enter it. Close the app normally.
If validation fails, disable the copy using the command below before the next login.

## Output selection and disabling

While audio is stopped, refresh the audio-output list and choose the USB interface
(the owner observed a Behringer/PCM2902 device). Prefer its actual name; do not assume
a fixed ALSA card index or select HDMI simply because the monitor uses HDMI.
Start at low volume when headphones/speakers are connected. USB enumeration and
offscreen checks do not certify playback. MIDI settings still require the owner's
explicit opt-in; this launcher adds no `--confirm-midi` or auto-play behavior.

Disable future autostart without deleting builds or patches:

```sh
mv "$HOME/.config/autostart/wirescape-kiosk.desktop" \
   "$HOME/.config/autostart/wirescape-kiosk.desktop.disabled"
```

If the `.disabled` backup already exists, choose a different backup name first.
Keep that backup until satisfied with a replacement. Existing open windows are
unaffected. There is no emergency system-level service to disable.
