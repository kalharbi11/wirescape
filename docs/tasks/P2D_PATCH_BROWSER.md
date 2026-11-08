<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2D-c: patch browser (#294)

Read #294 and its engineering ruling, P2_REMAINING, App, starters, main/preload
file boundaries. List at most 256 JSON basenames from the user's patches folder;
read only validated basename tokens, reject symlinks/out-of-folder files, bound
reads to 2 MiB and decode strict UTF-8. Keep native paths private. The existing
openPatch return shape carries list metadata as JSON text; optional library
list/read/reset requests stay local to Electron. No new bridge member or engine
protocol. Bundled six Qt starters and three demos are read-only; selecting a
bundle detaches the native save target, Save writes a copy via Save As in the
user patch folder. Confirm selection only for unsaved edits; loadGraph remains
the only graph switch seam. No filesystem/dependency/real-device expansion.

The tenth file is the existing mock bridge, necessary to keep reset/list from
consuming scripted file-dialog results. This uses the workflow +50% file allowance;
the 650-line budget is unchanged.

```acceptance
tier: T1
size: <= 650 changed lines, <= 10 files
must_pass:
  - ci: full Web/desktop tests cover user list, bundled read-only starters, live selection, Save As, path containment, bounded reads and unknown-data roundtrip
stops: [ABI header edit, ID/format change, new dependency, real devices in tests, threshold loosening]
```

