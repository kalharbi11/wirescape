<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Patch designer guide and owner-authorized fast lane

Owner instruction: #426, 2026-11-08. The owner wants rapid design and publication
without Codex Lead approval. This is the standing authorization for PATCH DATA
ONLY; ordinary application/engine changes retain the repository's normal review.

## Assigning a patch designer

Paste this into the agent's chat:

> You are my Wirescape patch designer. Read AGENTS.md,
> docs/PATCH_DESIGNER_GUIDE.md, apps/studio/patches/README.md and the current
> generated module catalogue. Design the requested sound using supported
> capabilities, add or edit JSON in apps/studio/patches, validate it, commit
> for me and merge the passing patch-only change under my standing authority.
> Do not wait for Codex Lead approval. Follow existing branch protection/checks;
> report the actual deployment link/SHA and any untested listening limitations.
> My sound brief: [describe the sound, autoplay or keyboard, and desired controls].

## Keeping limitations current — integration author owns this

The permanent rule is in AGENTS.md → Docs upkeep, so every agent sees it.
A module/engine/UI/platform/publishing integration that changes what a patch
can do updates THIS guide in the same PR; the reviewer verifies that update.
This is not a later Codex Lead chore and does not need a separate docs ticket.

Update the affected limitations, native-versus-web availability, exact
module/port/parameter IDs and units, polyphony/asset restrictions, persistence
or publication steps, and a small wiring example when a new route is enabled.
Read the current catalogue/spec/source and actual evidence, not an old roadmap.
For example: a production Notes→CV route becoming available must replace the
current chord-routing limitation with its real module ID, connections and voice
limit. A sequencer gaining16steps must update the documented step limit and
saved-patch compatibility, only when that integration is actually available.
Keep pending capability proposals separate from working features.

If nothing patch-facing changes, the PR self-review says
`Patch guide impact: none — <reason>`. Missing/stale guide changes are part
of integration review, not an additional approval step for a patch designer.

## Where the public patches live

Edit/add/delete individual JSON files in **apps/studio/patches/**.
The Vite build packages this folder into patches/index.json; the existing
web-demo workflow watches apps/studio/**, builds and deploys main to Pages.
No per-patch TypeScript registration, manual manifest or native fixture update.
Visitors find compatible patches in Starter patches and PATCHES → SHARED PATCHES.
Changes appear after a successful deployment and refresh, not at the instant of commit.
An open PR is a preview/check, not a public deployment.

**Shipped set (2026-11-11).** Main ships 18 public library patches: Basics 1–7, Scene 1–8 and merged CHAOS Lab 1–3, plus a bundled desktop-only MIDI example. PR #555 adds two more public candidates (Lab 4 sample-and-hold FX and Lab 5 smooth-random keyboard pad) only once it merges and deploys. The bundled MIDI example is
`apps/studio/src/app/bundled/midi-keys-and-knobs.json`; it lives in the
app because this folder and the web host exclude `midi.*` modules; the web
Starter list hides it automatically. The six old `qt-*` starters were retired;
their IDs stay reserved. Historical curated.ts/STARTERS/native ui_starters
fixtures stay as regression snapshots, not the public editable library. Do not
edit those snapshots for ordinary public patch changes. Keyboard voice (the web
smoke-test default) and the three class demos (owner M8 sessions) stay built-in.

## Standing agent authority (no lead approval round trip)

A patch designer explicitly assigned this guide may:
- Create, edit, rename or delete apps/studio/patches/*.json and patch descriptions
  in its README. The owner has approved trying different sound designs.
- Self-review and enable auto-merge/merge a PATCH-ONLY PR after the checks below
  and the repository's required checks pass on its exact head. No Codex Lead,
  Compass, Keel or separate-session patch-design approval is needed.
- Use the signed gate line `Approved for merge <sha7>` and the usual ready label.
  This is the owner's exception to the normal T1 independent-review/lead gate.
  Do not claim a separate reviewer. Use normal PRs for agent changes; the owner
  may upload/edit directly on main through GitHub if repository settings allow.
- Batch a whole design session in one PR instead of one PR per knob tweak.
  No request to a lead is needed for a new patch title, musical choice or deletion.

Limits: this does NOT authorize changing application code, build plugins,
engine/DSP, SDK/ABI, format, module IDs, dependencies, CI, tests/thresholds,
protected docs or repository settings. Such changes use ordinary workflow.
An unavailable module is a limitation, not permission to implement it.
No force-push, device access, secrets, private endpoint names or copied third-party
presets. Branch protection still applies; do not bypass required checks.
If GitHub does not permit a self-merge, enable auto-merge and report the actual
setting blocker rather than pretending to merge.

## Current design profile

- Native patch v2, 48 kHz; stereo output buses 0/1. UTF-8 JSON <=2 MiB each.
  At most128 library entries /8MiB total; <=32modules /128cables per patch.
- Lowercase filename with hyphens, e.g. `slow-glass-pad.json`.
  Filename stem is public ID. Keep it stable for edits; change `name` for display.
  `keyboard-voice` and `qt-*` are reserved built-in IDs.
- Use only modules/ports/parameters in current
  apps/studio/public/wirescape_modules.json. No hardware MIDI modules in the
  web library; use host.keyboard. Browser audio needs its user start gesture.
- Reverb fx.reverb is merged on main (c39af7b): stereo SUM inputs, one wet/dry output per side, not one reverb per note. Unconnected right follows left; size/decay/damping/predelay/width/motion/freeze/mix are available. Use two output-bus routes to preserve stereo; saved state clears the tail.
- Macro requires48k. No external sample files/services. Inspect the actual
  catalogue and deployed SHA before assuming a module is available.
- **Chord (builds containing #403):** `essentials.chord` accepts and outputs
  `notes` event cables. Route `host.keyboard.notes` or a sequencer's
  `notes` output into `essentials.chord.notes`. The root is always present;
  four optional enabled semitone shifts (-24..+24) default to +4/+7 enabled,
  +12/-12 disabled. Each note activation owns distinct output note IDs;
  same-pitch duplicates are distinct voices, and held intervals don't retune.
  Reusing a live explicit `(source_id, channel, note_id)` is a producer error:
  last wins, releasing the old chord; the next matching off releases the current
  chord, and another off is ignored. The event has no generation discriminator.
  Capacity is 16 input activations and 80 emitted/pending voices; new notes are
  declined when full. Chord instances sharing a source need distinct graph IDs
  modulo 2048 to keep their output note identities separate at fan-in.
  Chord is not a sequencer or a CV source. **A production polyphonic audio
  chain requires `essentials.notes_to_cv` (#421)**: Chord `notes` →
  Notes→CV `notes` → poly `pitch`/`gate`/`velocity` → instrument/Output.
  Notes→CV is in builds containing #421; inspect the catalogue. `midi.to_cv.midi` is raw-MIDI-only and cannot accept Chord's note
  events. No hardware/MIDI or listening validation is implied.
- **Notes→CV (builds containing #421):** `essentials.notes_to_cv` accepts
  a `notes` event input and produces three **poly signal** outputs:
  `pitch` (V/oct; C4=0), `gate` and `velocity`. It allocates fresh poly
  voices independently of incoming MIDI channels (stepped `voices` 1–16,
  default 4, rotating/stealing when full). Example: `host.keyboard.notes`
  → `essentials.notes_to_cv.notes` → `pitch` to `essentials.vco.pitch`,
  `gate` to `essentials.adsr.gate`, and `velocity` to a sound-level
  control; then VCO.audio + ADSR.envelope → VCA → Output. The same note-event
  input also accepts a sequencer's `notes` or `essentials.chord.notes`
  (Chord is on main via #403). The raw-MIDI `midi.to_cv.midi`
  port does *not* accept those notes. The WASM host excludes `midi.*`;
  use `host.keyboard` in browser patches. Recorded native tests are
  device-off, not evidence of physical MIDI or speaker sound.

### Other shipped module IDs (catalogue cross-reference)

This guide's wiring examples are **not** the complete module inventory. The
generated `apps/studio/public/wirescape_modules.json` is the authoritative list
for each deployed build's module IDs, port directions, parameter ranges and defaults.
At this guide revision, it also contains these modules not named in the examples
above or below:

- Control and shaping: `essentials.clock`, `essentials.lfo`, `essentials.function_envelope`,
  `essentials.adsr`, `essentials.slew`, `essentials.scale_offset`, `essentials.quantizer`
- Audio and filtering: `essentials.vca`, `essentials.vcf`, `essentials.ladder`,
  `essentials.resonator`, `essentials.mixer`, `fx.delay`, `sound.blend`
- Generators: `example.random_source`, `example.step_sequencer`

Treat the list as discoverability, **not** a substitute for the generated
descriptor or a promise that pending modules are available on a deployed SHA.

### Macro Oscillator model names (48 kHz only)

These **0-based** model indices follow the exact `plaits::Voice::Init` registration order
at pinned `pichenettes/eurorack` commit `f4cfa7697095a28d292bff758beb51deb49b5a86`
(`plaits/dsp/voice.cc`). The index is stored in `ports.macro_oscillator.model`;
UI names are presentation-only. Do not substitute the older 16-model VCV order.

| Index | Engine | Index | Engine |
|---|---|---|---|
| 00 | Virtual analog filter | 12 | Additive synthesis |
| 01 | Phase distortion | 13 | Wavetable synthesis |
| 02 | Six-operator FM preset 1 | 14 | Chord synthesis |
| 03 | Six-operator FM preset 2 | 15 | Speech synthesis |
| 04 | Six-operator FM preset 3 | 16 | Swarm synthesis |
| 05 | Wave terrain | 17 | Noise synthesis |
| 06 | String machine | 18 | Particle synthesis |
| 07 | Chiptune | 19 | String physical model |
| 08 | Virtual analog | 20 | Modal resonator |
| 09 | Waveshaping | 21 | Bass drum |
| 10 | FM synthesis | 22 | Snare drum |
| 11 | Granular synthesis | 23 | Hi-hat |

For example, model **07** is Chiptune, **12** is Additive synthesis,
**15** is Speech synthesis, and **20** is Modal resonator. Three six-operator
FM presets occupy indices 02–04. The selector does not alter the patch's port
or parameter IDs.

- Macro `level` input: when connected it overrides `gate`, so keyboard velocity
  patched into `level` keeps the internal low-pass gate open after note-off and the
  note never releases. Drive the Macro from `pitch` + `gate` (or `trigger`). If you
  want velocity on `level`, put an ADSR → VCA after the Macro for the release.
  Speech synthesis (model 15) with `harmonics` above about 0.35 plays a short word
  and then stops; keep `harmonics` ≤0.35 for sustained vowels.
- Start poly keyboard patches at3–4voices. MIDI→CV supports polyphony in desktop,
  but raw MIDI input and generated note events are different kinds.
- Desktop/native MIDI: wire `midi.in.midi` to `midi.to_cv.midi`; append-only
  `last_note` output (index 8) can drive `essentials.vco.pitch` as a mono held,
  unbent V/oct value (0=C4). It follows accepted NoteOn even when mono priority
  keeps another note sounding, survives release/choke,
  and resets to 0 on reset/restore; poly pitch/gate outputs keep their behavior.
- CC range wiring: `midi.in.midi` → `midi.cc_to_cv.midi`, then `cc1` → a CV input.
  For row 1 use `config.rows[0]` with `controller:21`, `min:0`, `max:1`.
  Finite optional endpoints override the matching bipolar default; inverted and
  constant ranges are valid, existing smoothing still applies, and JSON persists
  the mapping. MIDI modules remain unavailable in the WebAssembly host; use
  `host.keyboard` in browser patches.
- MIDI Learn (Studio on the desktop engine, #287): right-click a modulatable knob →
  **MIDI Learn…**, move a controller, **Confirm** (Enter; Esc cancels; the engine
  stops waiting after 30 s). Studio writes a free `midi.cc_to_cv` row
  (`channel` = the captured one, or `0` with "any channel"; `controller`), cables
  `ccN` → that knob as a modulation with amount `1`, and sends the edited patch as
  one live replan. Until it settles, Studio shows "Applying MIDI Learn…" and
  blocks patch edits, undo, cables and CC labels; rejection restores the old
  patch and history before editing resumes. The row goes on a converter whose `midi.in` feed hears the
  device that moved (`all_enabled`, or that exact `endpoint` + `system_name`); a
  converter with no feed gets one in the same edit. Otherwise it adds a
  `midi.cc_to_cv` module fed by a matching `midi.in` (a new one naming the device
  when none hears it); unrelated routes are untouched. A device unplugged or
  disabled before Confirm is refused. Wired but unconfigured outputs keep their
  default CC n (explicit default rows fill the gap, never a hole). Re-learning a
  mapped knob updates its row, or moves it to a new row when another knob shares
  that row or its converter does not hear the new device. **Forget MIDI mapping**
  removes only that knob's CC cable (and a now-unused last row). One undo step
  each. Learned rows are ordinary `config.rows`, so a saved patch needs no
  learn state. Modulation is added to the knob's own value, so a full sweep needs
  the knob near its minimum (0..1 CC onto its range). The `CC n` label on the
  module follows the learned controller (`ui.ccMap`, display only). Hand-edit
  JSON is still fine; unavailable in the browser demo (no MIDI).
- `essentials.vco` is a pure sine. Keep `frequency` 220 for concert tuning when pitch
  is connected (it is a ratio to 220; 261.63 plays three semitones sharp). Its `fm` input is additive Hz from a ±1 signal,
  so it only gives a few Hz of deviation. For audible FM, connect a modulator
  VCO's `audio` with a **modulation** cable to the carrier's `frequency`
  parameter (amount ≈0.05–0.2). With pitch connected, `frequency` is a tuning
  ratio, so the FM index tracks the keyboard. Put a VCA on the modulator audio
  (before the cable) to envelope the depth. Basics 2 shows this wiring.
- Modulation amounts span the target's whole range: 0.02–0.05 on a 20–20000 Hz
  cutoff is already a large sweep.
- Pitch CV0=C4, +1=one octave; VCO base frequency matters. Signals and events
  are different cables. Modulation targets parameter IDs with amounts[-1,1].
- Every audible chain reaches explicit output routes. An autoplay patch needs
  a running clock/sequence or continuous source; keyboard patches need notes.
- Use modest levels and feedback. Test note release/Panic and multiple held
  notes; check long tails, stereo routes, legible spacing and fit at phone width.
  Structural validation proves neither good sound nor Pi performance.
- Keep unknown fields when editing an export. Share only your own/licensed
  original patches; include GPL-3.0-only SPDX metadata for repository patches.

## Groove S1(a) build compatibility (PR #451)

`sequencing.groove` S1(a) adds two synchronized tracks of up to32steps while
retaining JSON config `version:1`, existing `run` and `t1_*` IDs. Existing S0
patterns remain readable; reserved4×64 transient storage is not a four-track UI.
Treat S1-only patches as requiring a build containing #451; check the actual
native/WASM catalogue and deployed SHA before claiming availability on another host.
Each track exposes `t1_*`/`t2_*` pitch, gate, mod1, mod2 and notes outputs.
Mod1/mod2 hold authored0–1 values until the next note; each note output is mono.
For notes wiring, connect `t1_notes` → one `essentials.vco.notes` event input and
`t2_notes` → a second VCO's notes, then mix their audio into `essentials.output`.
For CV wiring, use `t1_pitch` → VCO.pitch, `t1_gate` → VCA.gain_cv and
VCO.audio → VCA.audio → Output.audio; T2 uses the corresponding `t2_*` ports.
Route Output.audio explicitly to buses0/1 if a duplicated mono output is wanted.
Controls are rising-edge requests: global `run`/`stop`/`reset`/`mute`, plus
`t1_run`/`t1_stop`/`t1_mute` and T2 equivalents. STOP/track entry are queued to
the shared32-step boundary; mute releases immediately while phase continues.
Reload starts stopped even with saved high run. RESET does not invent a new start.
The stopped editor/Apply workflow and authoritative completed-STOP/playhead
observation are separate work: no live config editing or inferred STOP from a
queued ACK, mute, silence or browser timer. Native/Pi tests do not certify owner
listening, arbitrary whole-patch load or a deployed browser/editor implementation.

## Spectral mode compatibility (PR #310)

`sound.spectral` on older builds supports modes0–5. Builds including PR #310 append
**6 Interleave** and **7 Vocoder**, retaining existing IDs and slots0–8.
Patches using either mode or slots9–15 require PR #310 or a newer build;
do not advertise them as compatible with older native or web deployments. Check
the actual native/WASM catalogue and deployed SHA before publishing.
The implementation shares native DSP with the web build, but a native/Pi probe
does not certify a particular deployed browser build or subjective sound quality.

Concrete mono wiring: `essentials.vco.audio` → `sound.spectral.a`,
`essentials.noise.audio` → `sound.spectral.b`, then `sound.spectral.audio` →
`essentials.output.audio`; route that Output's audio to buses0/1 to duplicate mono.
Both Spectral inputs SUM polyphonic cables; its `audio` output is one voice,
not a separate spectral process per note. For mode6, use two contrasting sounds
in A/B; B is scaled by existing `b_level` (slot7,0–2,default1). For mode7,
A is the articulated modulator and B the sustained rich carrier (saw/pad/noise).
No external sample file or service is loaded; use source modules already supported
by the chosen host. Start with conservative carrier/output levels and monitor peaks.

| Slot | Parameter | Range and default | Use |
|---|---|---|---|
| 9 | `chunk_a` | 1–64,default5 | Interleave A hops; stepped, modulatable |
| 10 | `chunk_b` | 1–64,default5 | Interleave B hops; stepped, modulatable |
| 11 | `phase` | 0 Phase vocoder (default),1 Source phase | Interleave phase choice |
| 12 | `bands` | 6–64,default24 | Vocoder bands; stepped, modulatable |
| 13 | `flatten` | 0–1,default0.85 | Remove carrier spectral tilt |
| 14 | `release` | 0.005–0.4 seconds,default0.05; log | Vocoder envelope release |
| 15 | `noise` | 0–1,default0.2 | Modulator-following upper-band hiss |

Interleave alternates whole STFT frames on a hop of N/4 samples, including silent
frames, and its schedule advances in every mode. At48k/default N=2048, each hop
is10.67ms, so default5+5 chunks repeat every106.67ms. Controls except `level`
are sampled on hops; mode changes crossfade over four frames. Phase vocoder
continues output phase through switches (glide/smear); Source phase switches
more directly. Larger FFT sizes increase latency; default N=2048 is42.7ms at48k.
Vocoder bands cover70Hz to min(12kHz,0.45×sample rate); attack is4ms and carrier
smoothing approximately11.4ms. Statev2 restores the chunk schedule and accepts
statev1 at interleave position0; input/overlap-add audio history is not persisted,
so a loaded sound must be reanalyzed rather than resuming its exact waveform.

Device-free Pi evidence exists at `f6d4dd8`: three60s probes, two instances,
mono48k/128, all with0 allocations/0 faults. Worst blocks were0.315ms baseline,
0.699ms with size3/mode7/bands6↔64 each hop, and0.918ms with size3/pitch1/mode0↔7
every four hops, against a2.667ms period. These bounds describe those probes,
not a whole patch, arbitrary instance counts, live audio, or physical display load.
Final integration evidence and owner listening remain separate; larger sizes and
mode crossfades need their measured budget. Keep older-build availability and
browser deployment limits visible when sharing patches.

## Designer procedure

1. Read this guide, patches/README and the actual module catalogue.
2. Design in Studio or start from a JSON in apps/studio/patches.
3. Export/save v2. Give it a unique slug filename and readable `name`.
4. Run:
   ```sh
   python tools/patch_library.py check apps/studio/patches --shared
   python -m unittest discover -s tools/foundation/tests -p test_patch_library.py
   node --test apps/studio/tests-node/patch-library.test.mjs
   cd apps/studio && npm ci && npm run build && npm test
   ```
5. Confirm the uploaded/updated patch loads from the actual SHARED PATCHES UI.
   Test the real WASM build; FakeEngine motion does not prove sound. Agents use
   device-free native/WASM tests only. Record what was and was not auditioned.
6. Patch-only PR: self-review, green exact-head checks, signed approval, merge.
   Verify main Web-demo engine/site/deploy/live. Publish the deployment link/SHA.
   Failed deploy means old library remains; never report it as published.

## Owner: add/update/delete on GitHub

- Open apps/studio/patches on GitHub, Add file → Upload files; upload saved JSON.
- For an update replace the SAME filename. For deletion open file → Delete file.
- Commit to main when offered/allowed. Otherwise GitHub needs a PR; your patch
  agent can handle its checks and merge under the standing authority above.
- Watch Actions → Web demo; after successful deployment refresh the site.
  No native rebuild is needed just to test on the website.
- Removing a public patch does not delete visitors' downloaded or saved copies.

## Owner: personal patches, no Git at all

Design with modules/cables/knobs. SAVE downloads JSON in browser; OPEN imports it.
Desktop File → Save As writes JSON; File → Open reads it. Edits need only Save.
For a bundled/shared patch, Save creates a personal copy. Save inside the folder
shown by desktop Save As to list it under PATCHES → USER PATCHES. A file saved
elsewhere still opens normally but is not automatically in that list.
The real folder is Electron userData/patches, not repository user_data/patches;
its OS/app-name location can differ, so use the dialog's actual location.

**Proposed in #471 (not available on main until merged):** if the real engine
refuses a patch switch because it is unavailable or busy, the previous patch
keeps playing. OPEN REFUSED DRAFT opens the rejected document on the canvas,
marked **DRAFT · NOT RUNNING**; edits there do not alter the playing graph.
APPLY DRAFT retries the edited document, while BACK TO PLAYING returns to the
sound currently running. A further refusal keeps the draft. Save/export may
store a draft, but saving it does **not** make it run. Unknown patch fields are
preserved. Switching between draft and playing views makes the next Save ask
for its target, so one document cannot inherit the other's file. Apply waits for
prior playing edits to settle and holds an immutable submitted document; menus,
shortcuts and earlier reads cannot change it during acknowledgement. This is not
a workaround for unavailable modules or devices.

To bulk copy from a downloaded checkout into that actual local directory:
```sh
python tools/patch_library.py install apps/studio/patches --dest "/your/actual/patch-folder"
```
Existing names are refused. Add --replace only intentionally; a .bak is kept.
No devices, Git operations, upload, rebuild or running-app reload is performed.
Reopen PATCHES to refresh local listing. Laptop↔Pi transfer is just copying JSON.

Future: optional live/shared-library refresh and an owner upload/publish UI,
desktop synchronization and editable descriptions/audition tools. No automatic
GitHub credentials or private local-file uploads in this change.


## Step Sequencer 16-step editor (PR #410)

Builds including native #405 and UI #410 expose 16 pitch/gate/velocity triples
as two rows of eight. Length still defaults to8; steps9–16 default to Rest.
For a longer pattern set `length:16`, author `pitch16` (octaves), `gate16`
(0 Rest/1 Note/2 Tie), and `vel16` (0..1); wire Clock to `clock`, then
`pitch`/`gate`/`velocity` to an instrument. Shortened patterns retain inactive
values through edit/undo/save; native/WASM version compatibility remains #405's
contract. UI/FakeEngine preview alone does not prove DSP or audible playback.

## Groove native v2 patterns (G1 #496; available in builds containing this integration)

The original `sequencing.groove` authored config `version:1` remains valid and
keeps its two synchronized **32-step** tracks, old sample/note ordering and
implicit full gate. The new `version:2` config allows each track to set
`length` to **8, 16, 32 or 64** sixteenth notes (default 16 for a newly authored
v2 track, 32 for v1). A track pads its omitted steps with rests. Active tracks
follow their own lengths; the longest active track determines the global
STOP/Apply master-cycle boundary. V2 track START enters at the next
16-step bar; an authored v1 track retains its original 32-step entry boundary.

On each v2 step, `gate` selects 0.25, 0.5, 0.75 or 1 times one step, default
1. A tie extends the sounding note through the tied cells, with the *last*
tie's gate fraction determining the release. Only note trigs accept `cond`:
`{"type":"chance","p":50}` (integer percent 1..99),
`{"type":"ab","a":1,"b":4}` (loop 1 of every 4; b 2..8),
`{"type":"first"}`, or `{"type":"not_first"}`. Omit `cond`
for always. A failed condition skips note and ties, but still applies that
step's Mod1/Mod2 values. A:B counts the **track's** loops, not necessarily
16-step bars. A chance sequence is deterministic from the native graph
instance ID and track; it never stores a random seed in authored patch JSON.

Example of one track's v2 config, excluding unrelated module settings:

```json
{"version":2,"bpm":120,"tracks":[{"length":16,"steps":[
  {"kind":"note","key":60,"velocity":0.9,"gate":0.5,
   "mod1":0.4,"mod2":0.2,"cond":{"type":"chance","p":50}},
  {"kind":"tie","gate":0.75},
  {"kind":"note","key":64,"velocity":0.8,"cond":{"type":"ab","a":1,"b":4}}
]}]}
```

### Groove native chords and per-track speed (G4a/G5a, #526)

**Chord steps and speed play on engines with #526.** Config remains
`version:2`; there are no new event ports, parameters or telemetry fields.
On a `note` step, optional `keys` contains **1–5 distinct integer MIDI
keys (0–127)**: `keys[0]` equals the existing root `key`, and the remaining
keys are ascending. Studio omits `keys` for a single note. Chord voices
attack together in this order, each with a separate note ID. A tie extends
all voices; gate-off, STOP and RESET release them. A failed condition
skips the entire chord. The strict engine refuses `keys` on a rest or tie,
empty/oversized/invalid/duplicate keys, or a root mismatch.

Each track optionally sets `div` to **1, 2, 4 or 8** (1×, ½×, ¼×, ⅛×
speed); omission means 1. No other `div` values are supported. Each step
takes `div` sixteenths; gates, ties and A:B loop counters follow the
slowed track. The global STOP and Apply-at-loop-end master cycle is the
**maximum `length × div` over active/armed tracks**, at most
**512 sixteenths**. Track STOP still ends at its own
`length × div` boundary; `t1_pos`/`t2_pos` report each track's own
step index. For example, `length:16, div:4` is a four-bar progression
using sixteen pads.

A v2 step and slowed track can be authored as:

```json
{"version":2,"bpm":120,"tracks":[{"length":16,"div":4,"steps":[
  {"kind":"note","key":60,"keys":[60,64,67],"velocity":0.8,"gate":0.5},
  {"kind":"tie","gate":0.75}
]}]}
```

**Wiring:** `sequencing.groove:t1_notes` → `essentials.notes_to_cv:notes`
→ Notes→CV `pitch` and `gate` to a polyphonic voice chain
(VCO → VCA → Output, gate through ADSR to VCA level). Connect
`t2_notes` to another Notes→CV for an independent track. The native
#526 engine test proves three voices with its device-free test
`NotesToCv`; production `essentials.notes_to_cv` is contingent on its
separate #512 integration and must not be assumed to exist on an older
deployed engine.

The output and transport IDs remain unchanged; G3's appended measurement
ports appear below. Engines without #526 refuse `keys` and `div` rather
than silently misplay them. Studio authoring for both is described in
"Groove chord steps and per-track speed in Studio" below; physical listening
is a separate owner check. Check deployed SHAs for native and WebAssembly
support.

## Groove observation outputs (PR #501)

Groove observation outputs append to its existing pitch/gate/mod/notes ports:
`t1_pos` and `t2_pos` report 0-based steps while active and -1 only after the global
transport, the track and all owed note releases finish. Mute and STOP request acknowledgement
do not mean fully stopped. `t1_fired`/`t2_fired` are 0/1 condition results
(including a passed trig's continued ties). Exact per-track uint32 loop counts
use paired `t1_loop_lo`/`t1_loop_hi` and `t2_loop_lo`/`t2_loop_hi` signals,
each 0..65535; reconstruct as `lo + 65536 * hi`. These ports are patchable,
but primarily support the engine's measured editor telemetry; no cable is needed
for the editor to observe them. For example, a `t1_fired` signal may drive a gate
input to visualize a passed note condition. It is an observation held for the
step, not an extra note-event route. All original port IDs/order remain stable.

## OSC In and Monitor (host slice B of #72, once its PR merges)

`osc.in` has eight CV signal outputs (`row1`…`row8`) and a `messages` event
output. Each row defaults to `/pico1/knob/1`…`/pico1/knob/8`, maps numeric
0–1 to CV 0–1, smooths over 5 ms and retains the last value when stale.
The control-thread adapter accepts a patch `config` with `port` (1–65535,
default 9000), `lan` (default false), and up to eight row maps with
`pattern`, `kind` (`cv|gate|trigger`), `argument`, input/output ranges,
`smoothing_ms`, `timeout_ms` and `stale` (`hold|zero`). Invalid data
refuses activation without rewriting the saved patch.

**Sidecar availability, only after slice B merges:** the native Electron engine
listens on `127.0.0.1` by default. The patch's `lan:true` alone **never**
authorizes a LAN socket: a real owner launch must also pass `--confirm-lan`.
CI uses localhost only. One worker shares each listening UDP port across
module instances, up to 32 routes. Busy/failed bindings do not refuse a patch;
affected OSC inputs remain inactive and `status.osc.routes` reports
`port-busy`. An unconfirmed LAN toggle still works on localhost and reports
`lan-not-confirmed`. No OSC Out, browser/WASM networking or native OSC
listener exists in builds lacking B.

**Example wiring (native Electron after B):** add an `osc.in` with
`config: {"port":9000,"lan":false}`, patch `osc.in.row1` to the
`essentials.vca.gain_cv` signal input, and send an OSC 1.0 float message to
`127.0.0.1:9000` at address `/pico1/knob/1` with a value in 0–1.
The `monitor` module's `events`, `in1` and `in2` ports exist, but the
user-facing Monitor log is a **separate slice C**; do not claim it is visible
until C merges. No existing module/port/parameter IDs change.

## Groove editor in Studio (G2a #498; builds containing this integration)

Studio now edits `sequencing.groove` patterns on its card (Pads view, two lanes: Trig and
Note). Authoring rules for patch designers:
- **Format:** Studio reads `config.version` 1 and 2. Opening a v1 pattern changes nothing;
  the *first edit* rewrites it as v2 and gives each v1 track an explicit `"length":32`
  (its old meaning), adding an empty second track (same length) if the pattern had only one. A newly added Groove has
  no `config` until edited; its first edit writes two 16-step tracks of rests at 120 BPM.
  Unknown keys in `config`, tracks or steps are saved back untouched in the document;
  the engine projection omits unrelated extras but forwards the known `keys` and `div`
  fields unchanged (including values an old engine refuses). A newer `version` is shown read-only.
- **What it writes:** `length` 8/16/32/64 (shortening a track trims its saved steps),
  `gate` 0.25/0.5/0.75 (1 is written by omitting `gate`), `cond` from the picker
  (chance 10/25/50/75/90 %, A:B 1:2, 2:2, 1:4, 2:4, 3:4, 4:4, 1:8, first, not first; any
  valid hand-authored chance 1–99 or A:B up to b = 8 is kept), `bpm` 20–300, per-track
  `armed` and `mute`. Rests and ties never carry `key`, `keys`, `velocity` or `cond`.
- **Draft and Apply:** edits are a draft; the engine keeps playing the applied pattern.
  **Apply** is enabled only on a measured, current-generation stop (both tracks report
  step -1) or while audio is not running; it reloads the whole graph, so the Groove (and
  other modules) start stopped, and Play starts from step 1. A refused Apply keeps the
  draft and starts nothing. **Stop & apply** queues the global STOP to the master-cycle
  end and applies once the stop is measured. **Revert** restores the applied pattern.
  Play applies a pending draft first. Transport buttons send momentary rising edges to
  `run`/`stop`/`reset` and `t1_run`/`t1_stop` (`t2_*`); they are not saved.
- **Live view:** the playhead, track positions and the "skipped this pass" dim (a failed
  condition) come only from engine telemetry; on an engine without the playhead
  capability they stay hidden. Chance results are never predicted in the editor.
- **Roll:** Pads/Roll are two projections of the same MIDI-key pattern. Roll
  rows are indexed by MIDI note, including every note of a chord. Clicking an empty
  note cell on a note step adds a voice, up to five notes per step; dragging a note
  moves only that voice. Drag right to tie; double-click, right-click or press
  Backspace on a focused note cell to delete that voice. Under the
  note grid, lollipop lanes edit velocity, gate, Mod 1 and Mod 2. The inspector
  also accepts typed Mod 1/2 percentages. The Roll scrolls within its card,
  rather than widening the web page. Roll note buttons are keyboard-focusable:
  arrow keys move by step or pitch; Enter/Space enters a note, and Backspace
  removes it. Focus a Roll value slider and use arrow keys or Home/End to edit
  its value.
- **Scale and bar tools:** root and Chromatic/Major/Minor/Pentatonic scale selections
  mark the pitch guide and affect subsequent pitch gestures, not saved note data.
  The choice is saved with the patch in `ui.groove["<moduleId>"] = {"root": 0..11,
  "scale": "<name>"}` (one undo step per change); the engine ignores it and the
  module config never holds it.
  Copy snapshots the selected (up to 16-step) bar. Paste and Clear each create a
  single undoable edit; existing unknown step keys are preserved. Each bar of
  16 steps becomes two rows of eight on narrow screens.
- **Keyboard:** focus the Groove first. Space Play/Stop, arrows move the selected
  step or pitch (Shift = octave), A/W/S/E/D/F/T/G/Y/H/U/J/K enter chromatic
  notes at the selected step (hold multiple piano keys to write one chord when
  they are all released; the multi-key cursor advances), Z/X change keyboard octave, hyphen makes a tie,
  Backspace clears a trig, 1/2 select track, Ctrl/Cmd+Z undo (Shift = redo).
- **Looks:** Graphite (default), Oxblood and LED only change the appearance;
  no style choice or scale setting is saved to the module config.
  No live per-step editing during playback.
Example (a 16-step bass, one 50 % trig and one 1:4 trig with a short tie):
`{"version":2,"bpm":120,"tracks":[{"length":16,"steps":[{"kind":"note","key":36,"velocity":0.9},`
`{"kind":"tie","gate":0.5},{"kind":"rest"},{"kind":"note","key":43,"velocity":0.7,"cond":{"type":"chance","p":50}},`
`{"kind":"rest"},{"kind":"rest"},{"kind":"rest"},{"kind":"note","key":48,"velocity":0.8,"cond":{"type":"ab","a":1,"b":4}}]}]}`.
Hardware listening and Pi display checks of the editor are not claimed.

## Groove chord steps and per-track speed in Studio (G4b/G5b, #527)

**Available with native #526.** The stored format, limits, master-cycle rule and a
Groove → Notes→CV wiring are in "Groove native chords and per-track speed" above;
Studio writes exactly that format: `keys` only for 2–5 distinct notes (root first,
the rest ascending), no `keys` for a single note, and `div` 2/4/8 with 1× omitted.
No chord-quality field is stored; names are derived from the notes.

- **Notes Single/Chord** in the inspector. Choosing Chord on one note builds the
  scale chord (or a major triad on Chromatic); the picker lists the scale's triad and
  seventh first, then triads, sevenths and extended chords, greying types that leave
  the scale. ◁ ▷ invert. The Note field is labelled Root on a chord step.
- **Moving a chord:** drag, wheel or ↑↓. A chord equal to the scale stack on its root
  moves within the key (Am → B° → C in A minor); any other chord moves as a block.
  Shift moves by octaves.
- **Pads** show the chord name (slash bass dropped) and a small pitch ladder.
- **Roll:** click an empty cell in a note's column to add a note (up to 5); drag one
  note on its own; double-click, right-click or Backspace removes one note, and the
  last note removes the step's note. All notes share the step's start, gate, ties,
  velocity, mods and condition.
- **Held keys:** hold several piano keys on the computer keyboard; the chord is
  written to the selected step when all are released, then the selection advances.
  Leaving the window mid-chord drops it without writing.
- **Speed `1× ½ ¼ ⅛`** next to Steps, per track; the track tab shows ½×/¼×/⅛×, and the
  loop/position display uses `length × div`. Changing Steps keeps the speed.
- The scale helper stays UI-only (`ui.groove`), as in G2b.

**Four bars in sixteen pads:** Steps **16**, Speed **¼** on T1. Each pad is one beat,
so pads 1–16 span four 4/4 bars. Chords on pads 1, 5, 9 and 13 (C, F, G, C), each
followed by three ties, hold one chord per bar.

## CHAOS sound presets (#489; available after Studio PR merges)

The `essentials.chaos_lite` card's **Presets** dropdown supplies eight
repeatable starting sounds. The first four are from its native README:
**Warm fifth** (smooth pair/fifth), **Bent wire** (delay bending), **Broken
bell** (ring-mod strike), and **Slow weather** (slow body motion). The four
Studio originals are **Basalt drone** (dark sustained comb), **Glass pluck**
(bright short resonant note), **Iron rain** (metallic high partials), and
**Tide choir** (slow stereo tonal movement).

A choice sets **all** descriptor parameters to its values or their defaults,
not merely the named subset, via the existing engine parameter path. Undo
restores the previous parameter state in one step, and save/reload retains
those numeric values without writing preset names or changing cables,
module IDs, or unknown fields. A changed knob shows **Custom**. These are
sound-design candidates, not separately approved/audio-auditioned patches:
verify actual sound against the offline render/WAV evidence before claiming
a preset's audio acceptance, and never infer DSP playback from FakeEngine
control values alone.

## CHAOS atmospheric lab (merged in #531)

Three original scenes use the current `essentials.chaos_lite` v1:
`chaos-01-ember-veil-pad` (keyboard pad, clocked smooth random),
`chaos-02-wireglass-lead` (ring/AM lead with envelope-driven coupling),
and `chaos-03-orbiting-iron-fx` (autoplay curved metallic pulses).
`basics-06-chaos-keys` and `scene-03-black-ice-choir` were also revised.

**Critical CHAOS gate rule:** `essentials.chaos_lite.gate` is an immediate
audio multiplier, not an ADSR generator. To get keyboard release, wire
`host.keyboard.gate` to `essentials.adsr.gate`, then
`adsr.envelope` to the two stereo `essentials.vca.gain_cv` ports.
Leave CHAOS gate **unpatched (default 1)**; if the keyboard gate goes
into CHAOS, a zero at note-off defeats the ADSR release. `fall` works with
the gate unpatched: each new note (a pitch-input step of at least half a
semitone) restarts the drop, the drop is capped at 4 octaves, and turning
`fall` back to 0 returns the pitch at 16 oct/s. Repeating the same note
with an unpatched gate does not restart it; use the gate or external CV
for that. Each patch description names useful performance
parameters; no new CHAOS DSP/control is assumed.

Four native CHAOS LFO waveforms **merged in #533**. The original sine
shape remains the default; triangle, sample-and-hold and smooth random
are additive. The #555 public patch candidates use shapes 2 and 3 without
new modules or port changes. The compatibility/test plan is in
`docs/tasks/CHAOS_LFO_SHAPES_532.md`. Eight module presets are available
from merged #522; patch JSON stores parameter values rather than preset
names. Real device listening remains owner-only.

## CHAOS internal LFO waveforms (merged #533)

`essentials.chaos_lite` appends **`lfo_shape`** at parameter slot 26:
0=Sine (default, fully compatible with older patches), 1=Triangle,
2=Sample & hold, 3=Smooth random. S&H and smooth random use their
own per-voice seeded random stream; the existing drift/jitter and
roughness RNG sequence is unaffected. Each cycle picks a new held
value or interpolation target. A 10 ms switch ramp is for shape
changes, not a new long-cycle envelope. No new input/output port,
no second LFO, and no sample looper. `lfo_rate` (0.02–12 Hz),
`lfo_depth` (0–1), `lfo_self` (0–1), and target (A pitch,
body tuning or cutoff) remain unchanged. Studio's eight CHAOS
module presets resolve the new selector to default 0 unless
explicitly specified.

Backward-compatible wiring: keyboard pitch → CHAOS.pitch; keyboard gate
→ ADSR.gate → VCA.gain_cv; CHAOS left/right → one VCA each → outputs
0/1. Leave CHAOS gate unpatched for actual ADSR release. A Lab 1 pad
can replace its external smooth-random CV with internal shape 3
targeting body tuning **only if its other modulation routes are
preserved separately**; the internal LFO has one target, so it is
not a patch-wide modulation matrix. Lab 3's percussive envelope
remains external. **Downgrade warning:** `lfo_shape=0` retains the earlier
sine algorithm, but this build saves internal CHAOS state as **version 2**.
An older CHAOS binary accepts only state version 1 and returns
`WS2_UNSUPPORTED` for version 2. Do not promise that a newer saved
session can be opened with an older binary merely by selecting Sine.
Preserve a backup before downgrading; test the actual host's response
to unsupported module state and, if necessary, reconstruct from
parameter values in a matching older build.

The native state reader supports version-1 imports. When importing version-2
state across sample rates, the 10 ms live-shape fade counter is bounded
to the receiving sample rate; an old build cannot perform this conversion.

Test native WAV, real WASM catalogue, old patch/state migration
and CPU before calling this feature released. The 27th parameter
must appear in generated catalogue and grouped panel. Device/Pi
listening remains an owner step.
