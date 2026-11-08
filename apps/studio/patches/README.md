<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Public patch library

Upload, replace or delete a .json file here, then commit.
A successful main **Web demo** deployment publishes this folder to the web app:
**Starter patches** and **PATCHES → SHARED PATCHES**. Refresh after deployment.
No source registration, generated manifest or per-patch build-code edit is needed.

The existing eight cinematic patches are here as editable JSON.
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

