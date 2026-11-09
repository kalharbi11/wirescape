<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Windows real-audio runtime: device identity, D5 swap, UI/engine agreement

Source: the owner's Windows hardware test, 2026-11-09, at `c14751a` (PR #439 head,
squash-merged as `659b960`). The build and 90/90 native tests passed; the catalogue drift that
failed `ui_catalogue_sync` there is already fixed on `main`. Root-cause analysis
and rulings: Keel. All the failures in the owner's report trace to the four causes below.
They are not independent bugs.

**Status (Keel, 2026-11-09):** RC1, RC3 and RC4 are implemented on branch
`claude/bold-wozniak-fxjaxr`, RC2 as the 2 s cold-enumeration spacing. One PR, not A/B.
Native code is all in a `swap`/device path with fake-backend tests; it needs a
cross-family (Codex/ChatGPT) T2 review and the owner's Windows run below. RC5 is closed:
the owner pressed Restart engine (`engine-exit code=0 expected=true`), no crash.

## Root causes (from the code, matched to the owner's logs)

**RC1. Device identity is the bare PortAudio name, and Windows repeats names.**
`native/sidecar/devices.cpp` `audio_devices()` uses `id = name`; `portaudio_service.cpp`
`Stream::Impl` matches by name and throws "Ambiguous audio device name" when a name appears
more than once. Windows PortAudio lists every endpoint once per host API (MME, DirectSound,
WASAPI, WDM-KS), so "Headphones (Realtek(R) Audio)" appears 3–4 times. Owner listing 2026-11-09: 26 stereo
  outputs, and names repeat **within one host API** as well (WDM-KS `Headphones ()` ×4,
  `Speakers ()` ×3). Effects:
- Selecting most outputs fails with `duplicate` / "Ambiguous audio output name".
- **First launch:** `start_audio()` sets `device_` to the default device's *name*. That name is
  repeated, so every open fails, and the session stays in `reconnecting` with
  `deviceId: null` until the user picks a unique name (Sound Mapper / Primary Sound Driver).
  The log line `boot-to-audio-running-ms=473443` is that 7-minute wait.

**RC2. The device enumerator runs on the command thread on every poll.** Each `audio::Stream`
and each `refresh_devices()` builds a fresh `Backend` (`Pa_Initialize`/`Pa_Terminate`). The owner
measured a whole `wirescape_cli play --list-devices` run (process start + init + enumeration +
terminate) at **96 ms**. So re-initialising is *not* the main swap cost; this corrects Keel's
earlier ~800 ms estimate. It still costs up to ~90 ms on the command thread every 100 ms
poll while no stream is open. The ~800 ms from hello to `running` is mostly stream open/start
plus the first measured callback (MME Sound Mapper, 44.1 kHz default device at 48 kHz). Ruling 3 stands
because it is cheap; RC3 is the real fix.

**RC3. `loadGraph` always takes the D5 path, which closes and reopens the audio device, all within a 150 ms gap budget.**
`Session::commit(new_namespace=true)` never uses live replan. With real audio it runs
mute → `stop_audio()` (Pa_Stop/Close/Terminate) → `start_audio()` (cold Pa_Initialize + Open + Start)
→ waits for the fade-in, with `kGapBudget = 150 ms` measured from `begun`. Init (≤ ~90 ms) plus
Pa_Stop (MME drains queued buffers) plus open/start plus the first callback cannot fit, so **every patch switch on real Windows audio is refused** with "Audio swap exceeded gap
budget" and rolls back to the previous engine, which costs another ~800 ms of silence. The
`--null-audio` sink never closes a device, so CI cannot see this.

**RC4. When the engine refuses a load, Studio still shows the new patch.**
`apps/studio/src/app/App.ts` `loadPatch()` replaces the document, marks it clean and autosaves
*before* awaiting `engine.loadGraph()`. On refusal it only shows a toast. The engine is still on
patch A (RC3 rollback), and MIDI is still routed to A, while the canvas shows B. This is
the owner's report that "the previous MIDI patch keeps sounding". After a *successful* D5,
MIDI is rebuilt from the new patch (`midi_->stop()` + `start_midi()`), so stale routing is a
consequence of RC3, not a separate leak. Each later edit resubmits the full document and
fails again in the same way.

**Downstream: "Event queue full".** Per-module rings hold 256 events (`engine2.h`), and only a
rendering callback drains them. In `reconnecting` (RC1) and during the RC3 gaps, nothing
renders, so knob drags (`setParam`) and computer-keyboard notes fill the ring within seconds.
Fixing RC1–RC3 fixes this. It is not a capacity problem: **do not enlarge the ring**.

## Keel rulings (T2)
1. **Identity (supersedes the #293 "id = name" assumption, keeps "id-only identity").**
   The audio device `id` is `"<host API name>|<device name>"`. When the same name occurs more than once
   within one host API, add `#<n>` (its 1-based order among those entries in PortAudio order),
   for example `"Windows WDM-KS|Headphones ()#2"`. Do **not** use the raw PortAudio index: indices
   shift whenever any device (inputs included) is added or removed, which would break
   the saved `audioDeviceId`. `name` stays the human name. Add `hostApi` as an optional additive field.
   The UI shows `name — hostApi` when two entries share a name. A stored legacy bare-name
   `audioDeviceId` is resolved once: if exactly one device has that name, upgrade it; otherwise
   use the default. Never stay in a permanent reconnect because of an old name.
   Hiding host APIs from the list (e.g. WDM-KS) is a product question for Compass, not part of this task.
2. **Default device:** resolve `Pa_GetDefaultOutputDevice()` to that device's composite id. Never
   resolve it to a name.
3. **One PortAudio initialisation per session.** Hold a single process-lifetime `Backend` in the
   sidecar. Stream open/close must not call `Pa_Initialize`/`Pa_Terminate`. Re-enumerate devices
   explicitly (terminate + initialise while no stream is open) at most every 2 s, and only
   while no stream is open. While a stream runs, keep the last list. (PortAudio cannot
   hot-plug enumerate under an open stream anyway.)
4. **`loadGraph` must not reopen the device.** Add an in-stream engine exchange:
   `Stream::exchange(EngineOwner next, timeout)` publishes `next` through an atomic pending
   slot. The callback adopts it at the top of a block and acknowledges; the control thread
   waits a bounded number of periods and then takes the old engine back for off-thread destroy.
   The callback does no allocation, locks or frees, and the old engine is destroyed on the control
   thread (same rule as D6). D5 then becomes: mute A → exchange → fade in B. It is held to
   the existing budgets and **without any change to `kMuteBudget`/`kGapBudget`**. Use the
   close/reopen path only when the stream is not running. Apply the same exchange to the null
   sink (`active_` swap with acknowledgement) so CI exercises the same code.
5. **Studio keeps the confirmed patch on refusal.** `loadPatch()` keeps the previous confirmed
   document until `loadGraph` resolves. On refusal it restores that document (and its
   view, dirty state and history), shows the error, and keeps the refused patch available as an
   "open anyway (not running)" draft. It must never show B as clean while A sounds. When
   the engine is not ready (`reconnecting`, `no-device`), loading still succeeds and the
   status bar says the engine is silent.

**Rejected alternative (VS Code agent, 2026-11-09):** start the gap timer after the old stream
stops. The budget measures *audible* silence. Moving the start point loosens a threshold
(an AGENTS.md stop), and the ~1 s silence on every patch switch would still be there.
The agent's other option, live replan, is the right direction, but `app_engine_replan` carries
module state across matching IDs. A `loadGraph` is a new ID namespace, so it needs a
"fresh namespace" replan with no state carried over. That variant is an acceptable
way to implement ruling 4 instead of a separate `Stream::exchange`.

**RC5 (open). The engine process exited at least once.** Studio shows `ENGINE: RESTARTING`
only after `app.engineExited`, at startup before the first hello (`RealEngine.ts`), or after a
manual restart. A refusal never causes it, so the owner's second screenshot shows the
sidecar exiting, or being restarted, around a device selection. Desktop `main.ts` did not log
the exit code; it now logs `engine-exit code=… signal=…` on stderr. A Windows code of
`3221225477` (0xC0000005) means a crash; investigate that before closing this task.

## Scope
`native/io/audio/portaudio_service.{h,cpp}`, `native/sidecar/{session.cpp,session.h,devices.cpp}`,
their tests, `apps/studio/src/app/App.ts`, `apps/studio/src/engine/RealEngine.ts`, Settings device
list labels, `docs/specs/ENGINE_PROTOCOL.md` (additive `hostApi`, composite id note; protected,
named below), and `docs/PATCH_DESIGNER_GUIDE.md` impact line (none expected).
Split it into two PRs if that helps review: **A** = RC1+RC2 (identity, single init), **B** = RC3+RC4
(exchange and Studio rollback). A is useful on its own: it fixes first launch, device selection and
"Event queue full" while reconnecting.

## Tests (devices off)
- Fake PortAudio enumeration with duplicate names across host APIs: ids are unique, selection
  works, a legacy bare name is upgraded or falls back to the default, and the default resolves to a composite id.
- Counter on fake init/terminate: a D5 `loadGraph` on a running fake stream performs **zero**
  initialise/open/close calls. The exchange is acknowledged within 3 periods. The old engine is
  destroyed off the callback (allocation watch on the callback path).
- Sidecar protocol: 20 back-to-back `loadGraph`s on the null sink with paced rendering all
  succeed with `commit path=d5` and monotonic `gen`.
- A refused `loadGraph` leaves `patch_`, `gen` and MIDI routing on A (existing). Studio Playwright with
  `FakeEngine` refusing a load: the canvas returns to A, a toast is shown, the document is not
  clean B, and the autosave still holds A.
- No test opens a real device. Real-device confirmation is an owner run (below).

## Owner confirmation run (after merge, Windows)
Launch with `--confirm-audio --confirm-midi` and do the following. Pick "Headphones (Realtek(R) Audio)": it is
accepted. Cold start with no settings file: audio is running in under 2 s on the default device.
Switch through all starter patches twice: there is no "gap budget" toast, and the sounding patch
matches the canvas. Load a MIDI-In patch, then a patch without MIDI In: the Launchkey makes no
sound. Drag knobs for 30 s: there is no "Event queue full". Repeat once at `--block 256`.

```acceptance
tier: T2
size: <= 900 changed lines, <= 16 files (or two PRs A/B within it)
must_pass:
  - ctest --test-dir build/native --output-on-failure --no-tests=error -R '^(abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests|sidecar_golden|abi2_sidecar_audio_recovery_tests)$'
  - cd apps/studio && npm ci && npm test
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi: sidecar device-free protocol + offline play
stops: [changed kMuteBudget/kGapBudget or ring capacity, allocation/lock/free in callback, protocol field removal or rename, device access in tests, new dependency]
```

Signed: Keel (Claude), 2026-11-09
