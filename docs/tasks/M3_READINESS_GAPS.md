<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3 preparation: source-grounded readiness gaps (#158)

Create `docs/design/M3_READINESS_GAPS.md` and this task file in one documentation-only PR. Compare current main with the M3 checklist in `docs/PLAN.md`: poly cables, modulation drag-to-knob and amount, base/modulation rings, feedback/back-edge markers, stereo pairs. Distinguish current-main behavior from open PRs and cite exact audited SHA, paths, and PR heads. Do not treat PR branches as merged evidence or approve production design.

Propose at most three bounded future work items with candidate acceptance ideas and ownership. Coordinate with existing #141/#142 UI ownership. State that modulation feedback needs a verified source of effective values and bounded snapshots; no fake animation or mutable audio/DSP reads from UI code. No DSP, ABI, telemetry, runtime, device or generated-file changes. This is preparation only; no scheduling claim.

```acceptance
tier: T0
size: <= 180 changed lines, <= 2 files
must_pass:
  - python -c "from pathlib import Path; import subprocess; sha='b7586874604de53736b72daacc87240619784685'; guide=Path('docs/design/M3_READINESS_GAPS.md').read_text(); assert sha in guide; subprocess.run(['git','cat-file','-e',sha+'^{commit}'],check=True); paths=['docs/PLAN.md','docs/tasks/STUDIO_CONTROL_HIERARCHY.md','docs/tasks/STUDIO_LAYOUT_CABLES.md','docs/tasks/STUDIO_MODULATION_FEEDBACK.md','native/ui/studio_controller.cpp','native/ui/qml/Main.qml','native/ui/qml/ParameterControl.qml','native/engine2/engine2.h','native/include/wirescape/module2.h','native/modules2/chaos_lite/dsp.h']; assert all(Path(p).is_file() for p in paths); print('audited SHA and cited paths exist')"
  - python -c "import sys; sys.path.insert(0,'tools'); from pathlib import Path; from foundation.acceptance import parse; parse(Path('docs/tasks/M3_READINESS_GAPS.md').read_text()); print('acceptance block parses')"
stops: [runtime change, protected path, new dependency, telemetry design decision]
```
