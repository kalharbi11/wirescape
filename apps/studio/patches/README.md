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

