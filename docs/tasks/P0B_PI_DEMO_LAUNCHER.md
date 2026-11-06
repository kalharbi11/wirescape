<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P0B: Pi demo launcher + `play --list-midi` (Phase 0, due 2026-11-07 morning, New York)

One command for the owner to start the class demo: sound on, MIDI on, web UI fullscreen. Switching patches is also one command. Runbook: `docs/DEMO_CLASS.md`.

**Read first:** `native/cli/play.cpp`, `native/control/midi_host.cpp` (`restore` reads `devices[]` entries `{kind:"midi", name, system_name, enabled}`), `native/io/midi/libremidi_backend.cpp` (endpoint names), `docs/PI_KIOSK.md`, `docs/PI_SETUP.md`.

**Build:**
1. `wirescape_cli play --list-midi`: print each MIDI input endpoint's `name` and `system_name`, one per line, then exit. It is additive and enumerates only (like `--list-devices`), opens no port, and is run only by the owner. Add a CLI test that checks the flag parses, using the existing fake-backend gate if there is one. If enumeration can't be tested without a device, test only argument parsing and the unauthorized path.
2. `tools/pi/demo.sh` (POSIX sh, SPDX header):
   - Reads `~/.config/wirescape/demo.env`, which the owner creates; ship `tools/pi/demo.env.example`. Keys: `CLI`, `UI_DIST`, `AUDIO_DEVICE`, `MIDI_PORTS` (`name|system_name` separated by `;`), `BLOCK` (default 256), `CHROMIUM` (default: whichever of `chromium-browser` / `chromium` exists).
   - `demo.sh <1|2|3>` writes a local copy of `native/examples/class_demo_<n>_*.json` to `~/.cache/wirescape/` with `devices[]` filled from `MIDI_PORTS`, using a small inline `python3 -I` script with stdlib only. It stops any previous demo player, then starts `"$CLI" play <copy> --confirm-audio --confirm-midi --device "$AUDIO_DEVICE" --rate 48000 --block "$BLOCK"` in the background, logging to `~/.cache/wirescape/demo.log`.
   - On first start, it also serves `UI_DIST` on `127.0.0.1:4173` (`python3 -m http.server --bind 127.0.0.1`) and opens Chromium with `--kiosk --app=http://127.0.0.1:4173`. Later calls only switch the patch.
   - `demo.sh stop` stops the player, the server and the kiosk. `demo.sh status` prints the PIDs and the last log line.
   - `demo.sh --dry-run <n>` prints the commands and the generated patch copy without starting anything. This is what the tests use.
3. Test `tools/foundation/tests/test_pi_demo.py` runs `--dry-run` with a temporary `HOME` and fake env, and checks the generated `devices[]` and the exact command lines. It never runs a device command.
4. Add a short section to `docs/PI_SETUP.md` linking `docs/DEMO_CLASS.md`.

**Owner check (not in must_pass):** the rehearsal steps in `docs/DEMO_CLASS.md` §1.

```acceptance
tier: T1
size: <= 400 changed lines, <= 7 files
must_pass:
  - python tools/dev.py build --headless
  - python -m unittest discover -s tools/foundation/tests -p "test_pi_demo.py"
  - ctest --test-dir build/native -R "play|cli" --output-on-failure --no-tests=error
stops: [device access in tests, LAN bind, new dependency, engine change]
```
