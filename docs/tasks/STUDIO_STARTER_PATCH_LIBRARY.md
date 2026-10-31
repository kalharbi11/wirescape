<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio starter patches and saved-file dropdown (#187)

Owner Jam report #182 P0-D. Bundle six read-only v2 starter patches:
minimal oscillator, LFO-shaped VCA behind a filter, MIDI pitch-to-VCO,
four-voice keyboard chords, CHAOS-lite and Macro Oscillator. Show an
audible description and prerequisites for each in the selector. The MIDI
preset has a continuous offline reference tone; real MIDI input is user-only.
Macro is 48 kHz only; its Main and Aux ports are not stereo partners.

Read first: native/ui/qml/Main.qml, native/ui/studio_controller.h,
native/examples/chaos_two.json, native/document/app_patch_tests.cpp.
The existing controller remains unchanged. Starter JSON is shipped as
read-only Qt resources, never copied over user patches. QML Settings holds
only recent/favourite file URLs; a failed open never changes history.
Starter resources use the existing `:/starter/` namespace. The picker passes
`file::/starter/...` so QUrl's `toLocalFile()` yields the read-only
`:/starter/...` QFile resource path. No controller extension, installed file
copy, or user-patch write is required.
Existing dirty-save/discard/cancel continuations, fullscreen and Clock
behavior remain intact. Tests isolate history and write WAVs and saved
patches only in temporary test-owned paths.

```acceptance
tier: T1
budget: 1 bounded UI library pass
size: <= 500 changed lines, <= 12 files
must_pass:
  - python native/ui/tests/starter_patch_probe.py
  - ci: windows-x64 studio_ui_tests starterPatchLibraryAndOfflineRender loads all six bundled patches, renders non-silent offline WAVs, exercises dropdown open/recent/favourite and does not write user data
  - ci: windows-x64 studio_ui_tests, linux-arm64, sanitizers, guard
stops: [controller or DSP ownership change, invalid patch, silent offline render, test removal, protected path, threshold change, budget exceeded]
```
