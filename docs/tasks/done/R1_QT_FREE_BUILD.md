<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #464: Qt-free default build and PortAudio header move

Cleanup R1 (`docs/planner/CLEANUP.md`); preserve Qt coverage until R3.
Move the existing PortAudio header and MIT licence byte-for-byte to
`native/third_party/portaudio/`; update both engine and retained Qt includes.
CMake and `tools/dev.py build` default to Qt-free. Qt discovery is opt-in via
`WIRESCAPE_BUILD_UI=ON` / `build --with-qt`; `--headless` stays compatible.
Existing Windows Qt CI explicitly opts in. Static already sets ON; CodeQL is
source-only (`build-mode: none`). No tests or device permissions are removed.

Budget: acceptance counts renames as delete/add: 2,618 byte-identical moved
lines plus at most 200 actual edited lines (Codex Lead ruling, 2026-11-09).

Read first: issue #464, CLEANUP R1, `native/CMakeLists.txt`, `tools/dev.py`,
the three CI configure paths, and the audio-service header/license references.
Report a fresh default configure/build with Qt absent from PATH/prefixes and
its complete CTest result. Headless tests never open real devices.

```acceptance
tier: T1
size: <= 2800 changed lines, <= 14 files
must_pass:
  - python -c "import subprocess,sys; r=subprocess.run(['git','grep','-n','ui/third_party','--','native'],capture_output=True,text=True); sys.exit(1 if r.stdout.strip() else 0)"
  - python tools/dev.py build --headless
  - python tools/dev.py test
stops: [test removal, threshold change, required check rename]
```

Signed: Codex (R1 Qt-free build cleanup), 2026-11-09.
