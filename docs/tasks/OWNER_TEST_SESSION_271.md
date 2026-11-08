<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Owner Test Session: Pi + web, refreshed 2026-11-08 (#271)

This replaces the old #271 checklist and its scattered H–K addenda.
Keep #271 open for owner results. This document does not certify hardware.
Plan 60–90 minutes plus optional extended sessions. Start at low volume.
Record PASS / FAIL / SKIP for every row; SKIP never means passed.

## 0. Know what you are testing

Snapshot: main at ec1eea5 (2026-11-08). Build current main and record its actual SHA.
Merged: seamless edits, patch folder/browser, autosave/recovery, keyboard,
MIDI hotplug, remembered audio recovery, callback CPU/dropouts, telemetry,
VCO musical tuning, Macro gate, Blend, Resonator and Spectral base modes.
Native catalogue: 24 modules. Browser catalogue: 21 (midi.* excluded).
If testing a later SHA, record extra modules rather than enforcing this count.

Not merged at this snapshot: Delay #345, Reverb #348, Quantizer #323,
Spectral Interleave/Vocoder #310, LFO phase display #379, MIDI controls #210,
Learn seam #390/UI #287, audio route details #399, latency refresh #400,
16-step sequencer #405 + editor #410, curated patches #412, Chord #406.
The 64-parameter host cap #404 is merged; it does not itself add 16 steps.
Check the relevant PR is merged AND included in your SHA before testing extras.

Browser demo uses WASM DSP after its audio-start gesture. Pi uses Electron
and native wirescape-engine. Ordinary Vite preview can use FakeEngine:
a moving meter alone is not proof of DSP sound. MIDI/device/native-folder
checks belong to Electron. Browser silence before the start gesture is normal.

## 1. Update/build once; keep all components on the same SHA

Close old Wirescape and CLI playback. In your Pi checkout:
```sh
cd ~/wirescape-demo
git status --short
```
If dirty, save changes first; optional recoverable stash:
```sh
git stash push -u -m "before refreshed owner test"
```
Then:
```sh
sh tools/pi/update-and-build.sh origin/main
git rev-parse HEAD
python3 tools/gen_ui_catalogue.py --check
ctest --test-dir build/native --output-on-failure --no-tests=error
sh tools/pi/install-desktop-entry.sh
```
Stop on a build/test failure and report the last 30 lines. The update script
fetches, resolves one commit, builds Release engine, Studio and Electron.
Do not update halfway through the session. Save the printed SHA.
No stash is restored automatically; do not overwrite user patches/settings.
Launch the installed entry from this checkout (not the old kiosk/UI checkout):
```sh
mkdir -p ~/.cache/wirescape
~/.local/bin/wirescape 2>&1 | tee ~/.cache/wirescape/owner-session.log
```
The owner launcher authorizes real audio/MIDI. Agents/CI use fake devices only.

## 2. App, sound and patch persistence (must)

1. Settings lists CM477 and M8 by real names; choose/enable your devices.
   Sound should start without a separate Start button on normal owner launch.
2. Open Keyboard voice starter; play one note. Test minimize, F11, Esc and close.
3. Change a knob and cable: audible change, no stuck note or long interruption.
4. Save As a new named patch; change a knob, reload saved patch: saved value returns.
5. PATCHES: saved user patch is listed; bundled starter opens read-only.
   Save a starter as a user copy; the original stays unchanged.
6. Change a patch without saving; quit/reopen. Test last-session restore.
   Record any recovery offer and whether Accept restores your latest edits.
7. Computer keyboard enabled: play a chord. Disable it while held: notes release.
   Restart: OFF remains remembered. KEYS clicks still work with a Keyboard module.
   Re-enable the computer keyboard; text fields must not trigger musical notes.

## 3. Four-voice MIDI polyphony (must)

Use MIDI In → MIDI to CV.midi. Set Voices=4, a poly allocation mode
(rotate/reuse, not mono) and the intended MIDI channel/Omni.
Wire MIDI to CV.pitch → VCO.pitch; MIDI to CV.gate → ADSR.gate;
VCO audio → VCA audio; ADSR envelope → VCA gain_cv; VCA → Output.
Set VCO base to C4 (about 261.626 Hz) for conventional pitch labels.
Pitch CV is 0=C4 and 1/octave; output sums voices. Keep output gain low.

- Play C–E–G simultaneously, then C–E–G–B: hear 3 then 4 distinct pitches.
  Release one key at a time; other notes continue. Repeat with fast overlaps.
- Hold four keys, press a fifth: deterministic stealing is acceptable with
  Voices=4; no stuck voice when old/replacement keys are released.
- Repeat note-on of one key, sustain pedal down/up, bend and channel filter.
  Release sustain: all released keys stop. Panic: all notes stop.
- Disconnect M8 mid-chord, reconnect: no held/stuck voices; new notes work.
- Save/reload: Voices=4 and allocation/filter settings persist; held notes do not.
- Repeat a three/four-note computer-keyboard chord with host.keyboard Voices=4.
  If your source only sends one note, use another chord-capable source; record SKIP.

MIDI→CV already supports 1–16 voices (default four). This is different from
generating a chord from one root. Chord is design-only: current MIDI→CV accepts
MIDI events, not Chord NOTE events. Generated-chord acceptance waits for the
approved production Notes→CV allocation route. First target: triads and sevenths;
a five-note chord is optional follow-up, never a reason to delay 3/4-note evidence.

## 4. Original M8 demo regression (must, all 24 CC combinations)

Close the app before CLI playback. Generate disposable device-enabled copies:
```sh
for n in 1_chaos 2_subtractive 3_macro; do
  python3 -I -c "import json,sys; d=json.load(open(sys.argv[1])); d['devices']=[{'kind':'midi','name':'M8 MIDI 1','system_name':'M8:M8 MIDI 1','enabled':True}]; json.dump(d,open(sys.argv[2],'w'),indent=1)" native/examples/class_demo_$n.json ~/.cache/wirescape/m8_$n.json
done
build/native/wirescape_cli play ~/.cache/wirescape/m8_2_subtractive.json --confirm-audio --confirm-midi --device pulse --rate 48000 --block 256
```
Replace the MIDI endpoint/device with your actual name if different.
Play notes and sweep CC21–28 individually (M8 CCH must be CC28).
Ctrl+C; repeat for m8_1_chaos.json and m8_3_macro.json.
Each patch: notes start/stop, each of eight CCs audibly changes its mapped sound,
faults=0, xruns do not increase. Then open all three original class_demo files
in Electron and repeat notes/CCs with the enabled M8.
Never alter the committed demo fixtures to enable your private endpoint.

## 5. Every merged module: sound/control smoke (must; use small saved patches)

Add every module from the actual engine palette; missing merged modules FAIL.
For each row, change its controls, save/reload, and confirm wiring/values persist.
This is a manual smoke, not a replacement for per-module native test matrices.

| Modules | Patch/action and expected result |
| --- | --- |
| VCO, ADSR, VCA, Output | Section 3 voice: attack/release and gain work; pitch/octaves track |
| VCF, Ladder | Put each between VCO and VCA; cutoff sweep/resonance audibly affect sound |
| LFO | Modulate cutoff; rate/depth/wave change sound; no invented phase motion |
| Clock | Drive sequencer; tempo and run/stop change events |
| Function Envelope | Trigger from Clock; envelope modulates VCA; attack/decay shape changes |
| Mixer | Mix VCO and Noise; levels isolate each source, no runaway level |
| Noise | Audio into VCA/Output at low gain; level changes noise amplitude |
| Scale/Offset, Slew | LFO → each → pitch/cutoff; scaling/offset and smoothing are audible |
| Keyboard | Three/four keys, focus-loss release and Panic (section 3) |
| MIDI In, MIDI to CV, CC to CV | Section 3 plus CC21 → filter cutoff; all three show in native palette |
| CHAOS-lite | Keyboard pitch/gate into module; tune/decay controls change sound; two instances stable |
| Macro Oscillator | Keyboard pitch/gate into Macro; sample models and timbre/morph/harmonics; release works |
| Step Sequencer | Clock → clock; pitch → VCO; gate → ADSR; edit all 8 current steps; rests/stop work |
| Random Source | Clock into trigger; output modulates cutoff; range/smoothing change the result |
| Blend | Two different audio sources into A/B; balance endpoints isolate inputs; splice mode differs |
| Resonator | Keyboard pitch → pitch, gate → strike, audio → Output; tune/decay/damping/mix work |
| Spectral | Two continuous audio sources into A/B; exercise every currently advertised base mode; no NaN/silence crash |

For exact jack names use the palette/manifest; never connect an event output
to a CV signal input. Spectral modes Interleave/Vocoder are conditional on #310.
Sequence is eight steps until #405/#410 land. Test added modules with section 9.

## 6. Telemetry, live patching and robustness (must)

1. Meter/cable activity follows actual sound/events and clears on stop/removal.
   LFO phase-unavailable display must stay honest; a live measured dot requires #379.
2. Hold a chord; add/remove an unrelated LFO, connect/disconnect modulation,
   change depth, switch starters. Qualifying edits should use live replan
   (target <=20ms at 256 frames); report any audible gap. File loads may use D5.
3. At low volume unplug M8 mid-note, reconnect; repeat with audio adapter.
   Lost audio is visible; reconnect restores the chosen device without silent
   substitution. Record whether manual reselection was needed and recovery delay.
4. Engine crash: with only this test instance running, use a second terminal:
```sh
pgrep -a -x wirescape-engine
```
If exactly one test engine is listed, terminate that PID with `kill -TERM <PID>`.
Do not use a broad pkill. App stays open, reconnects, restores/ offers recovery;
accept if offered and report patch/settings/sound outcome.
5. Panic after sustain/rapid notes: silence, then new notes work.
6. Watch CPU/dropouts before and after edits/reconnect; cumulative dropouts
   must not reset or disguise increases. Record values, not only “looks fine”.

## 7. Phase 1 contention benchmark (must; about 10 minutes unattended)

Preserve the original #271 D3 gate: visible animated app plus separate null-sink
engine, 48k/256, 600 seconds, loadGraph ok, overruns=0 and p99 <50.
Close real playback; use this device-free app command instead of the installed
owner launcher, which always supplies confirmation flags:
```sh
apps/desktop/node_modules/.bin/electron apps/desktop/build/main.js --engine "$PWD/build/native/wirescape-engine" --null-audio
```
Open class_demo_3_macro.json and leave the app visible. In another terminal,
run the unchanged benchmark Python below from the same checkout:
```sh
python3 -I - <<'PY'
import json, os, subprocess, tempfile, threading, time
patch = json.load(open('native/examples/class_demo_1_chaos.json'))
settings = os.path.join(tempfile.mkdtemp(), 'settings.json')
p = subprocess.Popen(['build/native/wirescape-engine', '--null-audio', '--bench-summary', '--rate', '48000',
                      '--block', '256', '--settings', settings],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
replies, err = [], []
threading.Thread(target=lambda: [replies.append(l) for l in p.stdout if b'"re":"b1"' in l], daemon=True).start()
threading.Thread(target=lambda: [err.append(l.decode(errors='replace').rstrip()) for l in p.stderr], daemon=True).start()
p.stdin.write((json.dumps({'v': 0, 'id': 'b1', 'type': 'loadGraph', 'patch': patch}) + '\n').encode()); p.stdin.flush()
time.sleep(3); print('loadGraph reply:', replies[0][:120] if replies else 'NONE')
print('Running 600 s; keep the app window visible...'); time.sleep(600)
p.stdin.close(); p.wait(timeout=30); time.sleep(0.5)
print('\n'.join(l for l in err if l.startswith('bench ')) or 'NO BENCH LINE; stderr:\n' + '\n'.join(err[-20:]))
PY
```

## 8. Longer checks (optional now; still report SKIP)

- Run M8-loop playback in Electron 30 minutes while editing and switching patches:
  no crash/stuck notes, no increasing dropouts. This is separate from #296's
  required automated 3600s fake-device edit soak; a queued runner is not a pass.
- Owner may try 48k/128 then 48k/64 via CLI for 30s each; record xruns/faults.
- Install autostart: `sh tools/pi/install-autostart.sh`; reboot, measure desktop
  to sound and inspect boot-to-first-hello/audio-running-ms in user journal.
  Remove with `sh tools/pi/install-autostart.sh --remove`.
- Realtime check: find the test engine PID, `chrt -p <PID>`, and inspect VmLck.
  Report SCHED_OTHER if no grant; setup remains in docs/PI_SETUP.md.

## 9. Conditional additions (run after each feature is actually merged)

| Feature | Acceptance to add to this same session |
| --- | --- |
| Delay / Reverb | Dry/wet, stereo, delay clock/ping-pong and reverb freeze; release tails stable; save/reload controls |
| Quantizer | Major/minor/custom scales, root/shift, four pitches, sample/hold; sequence pitch → Quantizer → VCO |
| 16-step editor | Edit/play steps 9–16, wrap/rest/tie; save/reload; old eight-step patch still works |
| MIDI controls / Learn | Patchable controls work; arm, turn CC, persist mapping, reload; cancel/timeout/disconnect safely |
| LFO phase display | Native AND WASM dot follows measured phase; stop/stale snapshot clears it |
| New audio routes | Explicit valid mono/stereo routes/rate/buffer; stale selection refuses; no hidden fallback |
| Chord + production Notes→CV | Root60 produces 60/64/67; then 60/64/67/71; independent releases, held edits, sustain/choke/steal, repeated roots |
| Curated starters / extra Spectral modes | Every advertised patch/mode loads, sounds, saves/reloads; no overlap/crash |

Do not mark an unavailable feature failed or passed: report pending PR + SKIP.
Report a feature advertised by the tested engine but unusable as FAIL.

## 10. Browser comparison (same session, separate results)

Open https://kalharbi11.github.io/wirescape/ in a fresh/hard-reloaded tab.
Press its audio-start gesture, play Keyboard voice, then sequence/CHAOS/Macro.
Check all 21 currently web-eligible modules, especially Blend/Resonator/Spectral.
Exercise live knobs/cables, computer keyboard chords, SHARE roundtrip and SAVE/OPEN.
Check narrow viewport and touch/pinch on a real phone if available.
Record browser/device and deployment SHA/run separately from Pi SHA.
Latest successful deployment found during audit: b4ec649, run42509218198.
MIDI modules/USB endpoints/native folder and recovery are Electron-only here.
Physical Safari and actual audio remain owner checks; desktop emulation is separate.

## Report on #271

```text
Pi SHA / build+CTest result:
Web deployment SHA/run / browser/device:
2 app+save/browser/autosave/keyboard:
3 triad / four notes / fifth-note steal / sustain / reconnect:
4 CC21 22 23 24 25 26 27 28: subtractive[] chaos[] macro[]
5 each module/group PASS/FAIL/SKIP (name any missing module):
6 telemetry / live edits / MIDI loss / audio loss / restart / panic:
7 loadGraph reply + complete bench line:
8 optional soak / buffers / autostart / realtime:
9 conditional features + included PRs:
10 web sound/modules/share/file/touch:
CPU and dropouts before/after; faults:
Failure: exact patch + wiring + action + expected/actual + log excerpt
```

Attach `tail -50 ~/.cache/wirescape/owner-session.log` for failures.
Do not commit private device settings, logs, recordings or user patches.
Any #271 failure preempts new feature work. Automated #385/#386/#296 evidence
can reduce future owner repetitions only after it exists; no Phase1 gate waived.

Signed: Codex Lead (Codex), 2026-11-08

```acceptance
tier: T0
size: <= 360 changed lines, <= 2 files
must_pass:
  - python -c "from pathlib import Path; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) < 60; t=Path('docs/tasks/OWNER_TEST_SESSION_271.md').read_text(); assert all(x in t for x in ('MIDI', 'Spectral', 'Resonator', 'Blend', '600', 'p99', 'PASS / FAIL / SKIP', 'Notes→CV'))"
stops: [phase-goal change, source change, threshold change, hardware evidence claim]
```
