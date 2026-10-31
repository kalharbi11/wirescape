<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3-2 stereo-pair port display and atomic cabling (#178)

Read issue #178 and its root ownership release. Depends on #198 / PR #199:
its internal borrowed native descriptor accessor distinguishes event ports.
This is stacked on `task/198-studio-event-cables`, **not** a mutation of
the Codex branch. Retarget only after #199 merges through normal gates.

Export actual native `ws2_port.stereo_pair` metadata to QML. CHAOS-lite
Left/Right are paired; Macro Main/Aux never are. Present a named stereo
source affordance. Selecting a compatible input opens a keyboard/touch
accessible two-input chooser; its one **Connect** action stages both
ordinary port-to-port signal cables in a single candidate commit. Both
target inputs must be chosen explicitly (currently no built-in
module advertises a stereo input pair). Reject an occupied, identical or
invalid second input without committing the first cable. Preserve the
single-cable path and all patch v2 IDs/kinds and existing tests.

```acceptance
tier: T1
size: <= 300 changed lines, <= 6 files
must_pass:
  - ci: windows-x64 studio_ui_tests: stereoPairMetadataAndAtomicConnect and stereoPairGestureOffscreen cover actual CHAOS pair, atomic connect, rejection, save/reload and UI picker
  - ci: linux-arm64, sanitizers, guard
stops: [public ABI change, schema/ID change, test removal, threshold weakening, device access, new dependency]
```
