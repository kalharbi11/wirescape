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

The eight cinematic patches are now editable JSON in that folder. Historical
curated.ts/STARTERS/native ui_starters fixtures stay as regression snapshots,
not the public editable library. Do not edit those snapshots for ordinary
public patch changes. Keyboard voice and Qt/class-demo regressions stay built-in.

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
- Macro requires48k. No external sample files/services. Check the installed
  catalogue/deployed SHA before using new Chord or Notes→CV modules.
- **Chord (builds containing #403):** `essentials.chord` accepts and outputs
  `notes` event cables. Route `host.keyboard.notes` or a sequencer's
  `notes` output into `essentials.chord.notes`. The root is always present;
  four optional enabled semitone shifts (-24..+24) default to +4/+7 enabled,
  +12/-12 disabled. Each note activation owns distinct output note IDs;
  same-pitch duplicates are distinct voices, and held intervals don't retune.
  Chord is not a sequencer or a CV source. **A production polyphonic audio
  chain requires `essentials.notes_to_cv` (#421)**: Chord `notes` →
  Notes→CV `notes` → poly `pitch`/`gate`/`velocity` → instrument/Output.
  Until #421 is actually merged into the target build, don't promise that
  route; `midi.to_cv.midi` is raw-MIDI-only and cannot accept Chord's note
  events. No hardware/MIDI or listening validation is implied.

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
  `host.keyboard` in browser patches. MIDI Learn stays separate UI-owned work.
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
