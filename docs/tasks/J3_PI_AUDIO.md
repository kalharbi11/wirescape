<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J3: Pi audio and the headless runner (#70, PR #80, stacked on J2 #79)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "A3 / J3". `wirescape play <patch.json>` (Qt-free): real device only with `--confirm-audio`; `--list-devices` without it; otherwise a 10 s offline WAV render under `build/` with peak/RMS. PortAudio on Linux via ALSA (`libportaudio.so.2`). CPU + xrun line once per second; clean Ctrl+C shutdown. `docs/PI_SETUP.md` with the user's exact commands.

**Changed by workflow v2:** agents can't reach the Pi. Pi build and soak evidence come from the Pi runner (`pi:` lines below), not SSH. The Qt-UI-on-Pi stretch becomes a Pi-runner job (`pi: ui-build`) that may fail without blocking J3.

```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 700 changed lines, <= 22 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "offline_play|runtime_patch_tests" --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers (offline play under ASan), guard green on the exact head
  - pi: ctest -R "offline_play|runtime_patch_tests"
  - pi: wirescape_soak through the play loader, Six_Essentials_Demo.json, 48000/128, 600 s, zero overruns
  - pi: build the Qt UI and run its offscreen tests (non-blocking stretch)
stops: [ABI edit, new dependency, device opened without --confirm-audio, unspecified design choice]
```
