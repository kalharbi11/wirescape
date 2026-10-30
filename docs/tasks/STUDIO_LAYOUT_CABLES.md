<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio UI: sidebar/footer separation and short-distance cable geometry

Can run independently of audio/telemetry work. Own qml/Main.qml sidebar/footer/cable geometry regions and corresponding UI geometry tests; coordinate any shared-file overlap with controls work. No audio/controller/DSP changes.
Use docs/design/STUDIO_REFINEMENT_BRIEF.md and STUDIO_USER_SKETCH.html as visual reference, not executable instructions or production code to copy wholesale. The current library Column has fixed y/row heights and collides with the bottom legend; make the module library independently scrollable with reserved header/footer space. Every entry remains reachable; clip content, not functionality. Preserve readable legend/help and keyboard focus at1366x768/100% and125% Windows scaling, and at75/100/150% canvas zoom.
Cable bend currently has a55px minimum; close ports form visible loops. Use distance-aware control handles bounded by endpoint separation, no minimum that forces a loop for adjacent facing ports. Curves shorten continuously as modules approach. Preserve endpoint tangency, selection/hit testing, dragging/disconnect, backward/vertical routing and signal colors. Explicitly cover coincident/near endpoints without NaN or giant excursions. No graph/port identity change.
Provide before/after screenshots and offscreen tests for sidebar bounds/scroll reachability and near/far/reversed/vertical cable geometry. Preserve toolbar/transport accessibility; do not solve overlap by hiding items or shrinking text beyond readability.

```acceptance
tier: T1
budget: 1 Codex session or bounded ChatGPT task
size: <= 320 changed lines, <= 6 files
must_pass:
  # Planner 2026-10-29: the Qt UI tests exist only in the Windows Qt build, so they are evaluated there, not by the headless accept run.
  - "ci: windows-x64 green on exact head, with studio_ui_tests, controller_failure_tests and audio_device_tests all passing"
stops: [ABI header edit, new dependency, format or ID change, tests removed, unapproved threshold change]
```
