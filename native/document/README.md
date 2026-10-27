<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Native patch documents

`inspect()` keeps its legacy classification and byte-retention behavior. The separate `read_patch()` API accepts native v1/v2, rejects duplicate keys (including escaped aliases), and returns canonical v2 plus the original bytes. It does not load processors or open devices.

Hosts construct `ModuleCatalog` with `add_manifest()` from each module.json and the descriptor's output-zero port ID. Migration maps numeric parameter keys through those manifests. Unmapped values remain unchanged inside `module.preserved.values`. Unknown modules retain their complete v1 module under `preserved`, are inactive, and keep their cables. No DSP state is synthesized.

Every migrated document and module also retains its entire original v1 object under `preserved`. This deliberately preserves even collisions with names newly defined by v2 (including an existing v1 `preserved` field). Ordinary unknown fields remain in place too. V2 reads/saves never unwrap or reinterpret these envelopes; JSON values survive repeated round trips. Native v1 audio/control cables become signal cables; the original kinds remain in the preservation envelope. Newly defined v2 settings/config/state/device fields do not interpret opaque v1 data with colliding names; those originals remain in the envelopes.

`validate_patch()` checks types, positive unique IDs, existing cable/output endpoints and feedback bounds. Port compatibility and processor availability belong to graph construction. Missing v2 settings use host defaults; v1 migration supplies 48 kHz, 256 frames, 16 maximum channels and a one-frame feedback slice.

`save_patch()` always emits v2 using QSaveFile's atomic commit with direct-write fallback disabled. A v1 source gets an exact sibling `<name>.v1.json` backup, including on Save As. Existing backups are never replaced; staged backup publication refuses an existing destination. Failed loads, invalid documents, malformed/future destination files and oversized serialization cannot overwrite the destination. The 2 MiB document limit also applies to serialized v2, including preservation envelopes.

The Six Essentials render test uses separate v1/v2 adapters into the retained ABI1 engine, initializes from their respective parameter/cable/output fields, and compares complete two-second WAV bytes and SHA-256 after save/reload. App engine2 integration follows in M2-6.
