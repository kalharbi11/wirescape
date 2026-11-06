<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Class demo on the Pi (Phase 0, 2026-11-07)

**Goal:** a smooth demo with no new engine code. Sound starts the moment the demo starts, MIDI keys play, CC knobs move the sound, and the new web UI is on screen. Everything here uses what already works on `main`. The two helper pieces (demo patches, launcher) are tasks P0A and P0B. If they aren't merged in time, use the manual commands in section 3; they do the same thing by hand.

**What you can honestly say in class:** "This is the engine running live on a Raspberry Pi 5. MIDI and CC come from my controller. This is the new interface, running in Chromium on the same Pi. Next week the interface drives this engine directly, including live patching."

## 1. Tonight: rehearsal checklist (about 45 minutes)
Do these in order on the Pi, with the USB audio adapter, the controller(s) and the HDMI display connected.

1. **Update and build** (persistent checkout, as in `docs/PI_KIOSK.md`):
   ```sh
   cd ~/wirescape-demo && git fetch origin && git switch --detach origin/main
   CMAKE_BUILD_PARALLEL_LEVEL=2 python3 tools/dev.py build --headless --config Release
   CLI=$(find build -name wirescape_cli -type f | head -1); echo "$CLI"
   ```
   If P0A/P0B are merged, use their merge commit instead of `origin/main`.
2. **Offline check, no devices:** `"$CLI" play native/examples/chaos_usb_midi.json`. It should print peak/RMS and write `build/play-offline.wav`.
3. **Find the audio device:** `"$CLI" play --list-devices`. Note the USB adapter's index or exact name.
4. **Find the MIDI ports:** `aconnect -l` (or `"$CLI" play --list-midi` once P0B is in). Note the controller's port name.
5. **First sound, low volume:**
   ```sh
   "$CLI" play native/examples/chaos_usb_midi.json --confirm-audio --confirm-midi \
     --midi "<controller port name>" --device "<USB adapter>" --rate 48000 --block 256
   ```
   Play keys. The terminal prints `cpu=… xruns=… faults=…` once a second. xruns should stay at 0. If they climb, close other apps and keep `--block 256`.
6. **Find your CC numbers:** run `aseqdump -p "<controller port name>"` (Ctrl+C to stop), turn each knob you want to use, and note the controller numbers (`control change … param NN`). **Send these numbers to the agent doing P0A** (or post them on its issue). Up to 8 knobs.
7. **Web UI on the same Pi:** in the `Wirescape-UI` checkout:
   ```sh
   npm ci && npm run build && (cd dist && python3 -m http.server 4173 --bind 127.0.0.1) &
   chromium-browser --kiosk --app=http://127.0.0.1:4173   # or `chromium` on newer Pi OS
   ```
   Load a starter, move a knob and drag a cable. Check that the sound from step 5 doesn't glitch while the UI animates (watch xruns).
8. **Rehearse the switch:** Ctrl+C the sound, start the next patch, and check how long the gap is. With P0B: `tools/pi/demo.sh 2`.
9. **Fallback check (2 minutes):** Qt Studio fullscreen, as in `docs/CHAOS_USB_DEMO.md`. Confirm it still opens and plays, in case the headless path misbehaves tomorrow.
10. Write down what worked, plus the exact device and port names, in a local note (not in the repo).

## 2. Tomorrow: running order (about 5–8 minutes)
1. Before class: boot the Pi, connect everything, run `tools/pi/demo.sh 1` (or the manual commands). Sound is on, and the UI is fullscreen.
2. **Sound first (2 min):** play the CHAOS patch from the keyboard and sweep the CC knobs (P0A maps them to CHAOS and filter parameters).
3. **Patch switch (1 min):** `demo.sh 2` gives subtractive VCO → VCF → ADSR with an LFO; `demo.sh 3` gives Macro Oscillator.
4. **New interface (2–3 min):** on the same screen, show the module library, patching cables, modulation amounts on cables, a signature panel (sequencer, mixer), undo, and save. Say clearly that it is running on a simulated engine today.
5. **Close (1 min):** the architecture slide in one sentence: C++ realtime engine → local host → web UI in the browser, Pi first, Windows next, open source.
6. If anything stalls: Ctrl+C the sound, restart `demo.sh 1`. If headless audio fails entirely, use Qt Studio fullscreen with the same patch (press Start).

## 3. Manual commands (if P0A/P0B aren't merged)
- Sound: step 5 above, with `native/examples/chaos_usb_midi.json`. It has MIDI notes, but no CC mapping.
- UI: step 7 above.
- Stop: Ctrl+C in the sound terminal; `pkill -f http.server; pkill chromium`.

## 4. Rules that still hold
Only you run the `--confirm-*` commands. Start at a low volume. Don't commit device names, recordings or local patch copies. Afterwards, post a short debrief (what broke, what people asked) as one comment on the Phase 1 tracking issue; it feeds Checkpoint A.
