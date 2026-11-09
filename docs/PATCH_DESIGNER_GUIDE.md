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
- Macro requires48k. No external sample files/services. Do not assume pending
  Chord, Notes→CV, Quantizer or16steps exist: inspect actual catalogue.
- Start poly keyboard patches at3–4voices. MIDI→CV supports polyphony in desktop,
  but raw MIDI input and generated note events are different kinds.
- Pitch CV0=C4, +1=one octave; VCO base frequency matters. Signals and events
  are different cables. Modulation targets parameter IDs with amounts[-1,1].
- Every audible chain reaches explicit output routes. An autoplay patch needs
  a running clock/sequence or continuous source; keyboard patches need notes.
- Use modest levels and feedback. Test note release/Panic and multiple held
  notes; check long tails, stereo routes, legible spacing and fit at phone width.
  Structural validation proves neither good sound nor Pi performance.
- Keep unknown fields when editing an export. Share only your own/licensed
  original patches; include GPL-3.0-only SPDX metadata for repository patches.

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

