<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi hardware checks: Phase 0 record and the owner's audio/MIDI steps

## 1. Phase 0 record (2026-11-07): stopped and deferred, not passed
The browser/kiosk class demo was stopped. The web UI ran on its simulated engine (fake audio and MIDI device lists), and kiosk mode had no way to leave the app. The plan moved to the standalone app (`docs/PLAN.md`, Phase 1).

| Item | Result |
|---|---|
| Pi Release headless build of `~/wirescape-demo`, branch `demo` (#222 + #221) | **Passed** |
| PortAudio outputs seen | 0 HDMI, 1 M8 USB Audio, 2 CM477-30757 USB Audio, 3 PulseAudio |
| `wirescape play` on device 2 | Process ran: CPU 13–19%, `xruns=1` (stable), `faults=0`. **Audible output not confirmed.** |
| CM477 missing from PortAudio's list while playing; ALSA `Subdevices: 0/1` | Expected: our process holds the ALSA device exclusively. Not a bug by itself. |
| M8 over USB MIDI, endpoint `M8 MIDI 1\|M8:M8 MIDI 1` | **Verified at Linux level** with `aseqdump`: notes 36/40/43/48, CC21 (96, 127), MIDI channel 1 |
| M8 notes/CC changing Wirescape's sound | **Not verified** |
| Web UI in Chromium on the Pi | Smooth (owner). Fake devices only, by design of the simulated engine |

## 2. Leaving a kiosk window (if one is still open)
Press **Alt+F4**. If that doesn't work, open a terminal (Ctrl+Alt+T) and run `~/wirescape-demo/tools/pi/demo.sh stop`, or `pkill -f chromium`.

## 3. One-time setup for the commands below
```sh
cd ~/wirescape-demo
CLI=$(find build -name wirescape_cli -type f | head -1); echo "$CLI"
tools/pi/demo.sh stop 2>/dev/null; pgrep -a wirescape_cli || echo "no player running"
```
No rebuild is needed: the `demo` branch build from 2026-11-07 is what these steps use.

## 4. Is the USB adapter itself making sound? (no Wirescape involved)
```sh
aplay -l                         # note the CM477's card number, e.g. "card 3"
alsamixer -c 3                   # raise PCM/Speaker, press M to unmute if it shows MM, Esc to exit
speaker-test -D plughw:3,0 -c 2 -t sine -f 440 -l 2
```
- **You hear a tone:** the adapter, cable and speakers work. Go to §5.
- **You hear nothing:** the problem is outside Wirescape (volume, mute, cable, headphones, adapter). Fix that first; Wirescape can't be louder than this.

## 5. Wirescape sound, then the M8
**5a. Sound without MIDI** (the Six Essentials demo plays by itself from its clock):
```sh
"$CLI" play --list-devices       # use the CM477's exact name, as printed
"$CLI" play native/examples/Six_Essentials_Demo.json --confirm-audio \
  --device "<CM477 name>" --rate 48000 --block 256
```
Ctrl+C stops it. **Pass:** audible sound, and `xruns` stays flat.

**5b. M8 notes and CC** (writes a local copy of the patch with the M8 enabled; it is never committed):
```sh
mkdir -p ~/.cache/wirescape
for n in 1_chaos 2_subtractive 3_macro; do python3 -I -c "
import json,sys; d=json.load(open(sys.argv[1]))
d['devices']=[{'kind':'midi','name':'M8 MIDI 1','system_name':'M8:M8 MIDI 1','enabled':True}]
json.dump(d,open(sys.argv[2],'w'),indent=1)" native/examples/class_demo_$n.json ~/.cache/wirescape/m8_$n.json; done
"$CLI" play ~/.cache/wirescape/m8_2_subtractive.json --confirm-audio --confirm-midi \
  --device "<CM477 name>" --rate 48000 --block 256
```
Play notes from the M8, then automate CC21–28. Repeat with `m8_1_chaos.json` and `m8_3_macro.json`.

**Pass, per patch:** M8 notes play and stop cleanly (no stuck notes after the M8 stops); each of CC21–28 audibly changes something; no growing `xruns`; `faults=0`. If notes don't sound, run `"$CLI" play --list-midi --confirm-midi` and check that the printed name and system name match the two strings in the snippet exactly.

Post the results (pass or fail per step, plus any error text) on the Phase 1 tracking issue. This is the first Phase 1 hardware fact.

## 6. Phase 1 exit check (when P1B + P1F + P1E have merged)
```sh
cd ~/wirescape-demo && git fetch origin && git switch --detach origin/main
tools/pi/update-and-build.sh            # from P1E; incremental after the first run
tools/pi/install-desktop-entry.sh       # once; adds "Wirescape" to the menu with audio + MIDI consent
```
Open **Wirescape** from the menu. **Pass:** a normal window (minimize, close and Esc out of fullscreen work); sound running with no Start button; Settings lists the CM477 and the M8 (real names); each class demo patch plays from the M8 and CC21–28 move the sound; knobs are live; a cable edit applies with at most a short gap; meters move; Panic works. Record pass or fail per item on the Phase 1 tracking issue.

## Rules
Only you run `--confirm-*` commands. Start at a low volume. Never commit device names, recordings or the `~/.cache/wirescape` copies.
