<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Public patch library

Upload, replace or delete a .json file here, then commit.
A successful main **Web demo** deployment publishes this folder to the web app:
**Starter patches** and **PATCHES → SHARED PATCHES**. Refresh after deployment.
No source registration, generated manifest or per-patch build-code edit is needed.

Shipped set (2026-11-10): **Basics 1–7** (`basics-NN-*.json`, introductory: first tone,
FM/filter/LFO, echo and reverb, sequencer and quantizer, random melody, CHAOS keys, Macro keys)
and **Scene 1–8** (`scene-NN-*.json`, the revised cinematic patches). The MIDI example is bundled
in the app (`src/app/bundled/`) because this folder excludes `midi.*` modules. The old
`hollow-chapel`…`neon-elegy` files were replaced by the `scene-*` versions; Qt starters are retired.
Your private saved patches are not uploaded automatically.
Keep filenames lowercase-with-hyphens.json; put the display title in patch.name.
Use Studio's saved/exported native patch-v2 format at48k, stereo0/1.
Files are read-only in the public browser; SAVE makes a personal copy.

Agent instructions and standing patch-only merge authority:
[Patch designer guide](../../../docs/PATCH_DESIGNER_GUIDE.md).
The owner permits assigned patch agents to self-review and merge valid patch-only
changes after required checks, without another lead approval. No code/CI changes.

Validate from repository root:
```sh
python tools/patch_library.py check apps/studio/patches --shared
```

Uploading to this repository makes the JSON PUBLIC. Remove private device bindings.
A compatible Pi/Windows build includes the same library on its next UI rebuild;
personal OPEN/SAVE never requires Git or a rebuild.

## CHAOS atmospheric lab (draft #530)

These use **existing** `essentials.chaos_lite` v1 and current module IDs. They do
not require an unfinished CHAOS control, looper, waveshaper or subpatch. The
new scenes are original rather than copies of the five legacy Python presets.
Descriptions include performance parameters to expose in a future front panel.

| Patch | Musical job / test action | Performance controls to foreground |
|---|---|---|
| `basics-06-chaos-keys` (revised) | First playable stereo CHAOS / test note-off | A/B shape, detune, body mix and feedback, roughness, ADSR attack/release |
| `scene-03-black-ice-choir` (revised) | Cold slow pad / hold and release a chord | body tune/mix, cutoff, route/depth, drift, ADSR attack/release |
| `chaos-01-ember-veil-pad` | Warm evolving keyboard pad / long notes | shape A/B, detune, body mix, feedback, cutoff, drift, slow random range |
| `chaos-02-wireglass-lead` | Expressive metallic keyboard lead / legato notes | route/depth, rough rate/depth, internal vibrato, body tune, envelope and echo |
| `chaos-03-orbiting-iron-fx` | Autoplay irregular-feeling metallic accents / listen for space between hits | clock rate/duty, function rise/fall/curve, coupling, roughness, body, delay/reverb |

**Important release fix:** CHAOS's `gate` multiplies audio by zero at
note-off, so an external ADSR cannot produce a release if the keyboard gate
is also connected to CHAOS. Both revised keyboard patches and the three new
patches deliberately leave CHAOS `gate` **unpatched**, where its documented
default is 1. Keyboard gate controls the ADSR; the envelope controls left
and right VCA gain. This means the internal `fall` reset is unavailable
for those patches; leave `fall=0`. For falling-pitch transients use an
external modulation route or plan a future separate pitch-fall trigger.

All three new patches should be *structurally* validated in native/WASM and
*auditioned by the owner* before anyone claims a particular timbre, stereo
balance, CPU margin, or final preset quality. The slow random source is
clocked externally; the current CHAOS internal LFO remains sine-only.
See `docs/tasks/CHAOS_ATMOSPHERES_530.md` for the proposed layout and
Codex/Coda implementation notes.
