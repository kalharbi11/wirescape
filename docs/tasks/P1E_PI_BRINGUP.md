<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1E: Pi bring-up for host + web UI (Phase 1, after P1B; scripts can start earlier)

One command on the Pi gives sound and the web UI driving the real engine. Plus Pi-runner evidence that the host and Chromium together don't starve the audio thread.

**Read first:** `docs/tasks/P1B_HOST.md`, `tools/pi/demo.sh` (P0B, reuse its env file and structure), `docs/PI_RUNNER.md`, `docs/PI_KIOSK.md`.

**Build:**
- `tools/pi/studio.sh start|stop|status`: builds nothing, starts `wirescape-host --ui <dist> --confirm-audio --confirm-midi --device "$AUDIO_DEVICE" --block 256`, reads the token URL from the host's output and opens Chromium `--kiosk --app=<url>`. It reuses `~/.config/wirescape/demo.env`. Has a dry-run mode plus a unittest, as in P0B.
- `docs/PI_SETUP.md`: a short "Studio on the Pi" section covering Node/npm for building the UI (or downloading the CI artifact), the build commands, and `studio.sh`.
- **Pi runner jobs (in `kalharbi11/wirescape-pi`, via its normal PR process):** (a) build host + UI and run `ctest -R host_` with the null sink; (b) a **contention benchmark**: 10 minutes with the host null-paced at 48 kHz / 256 frames running `class_demo_1_chaos.json`, while headed Chromium shows the UI's 20-module stress patch. It reports render-time p99 and overrun count. Pass: zero overruns, p99 < 50% of the period. If the runner has no display session, it reports BLOCKED, not pass.

**Owner check (not in must_pass), the Phase 1 exit:** `studio.sh start` → sound with no button press; load starters; play MIDI; knobs live; edit cables (short gap is OK in v0); panic.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_pi_studio.py"
  - pi: build + ctest -R host_ (null sink)
  - pi: contention benchmark (zero overruns, p99 < 50% period) or BLOCKED with reason
stops: [device access in tests, LAN bind, changing Pi services, new dependency]
```
