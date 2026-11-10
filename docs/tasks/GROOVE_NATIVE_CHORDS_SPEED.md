<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove native chords and speed (#526)

Native G4a then G5a in one PR. Read first: `GROOVE_CHORDS_G4.md`,
`GROOVE_SPEED_G5.md`, `GROOVE_PLAYABLE_V1.md`, `CHORD_MODULE_403.md`
(identity/capacity rulings), and `native/modules2/groove/`. Start runtime work
after #505 merges. The existing Claude Studio builder owns G4b/G5b; this task
does not edit Studio source or close #514/#523.

Accept note-only `keys`: 1–5 distinct integer MIDI keys, root first and equal
to `key`. Omission means the existing one-key step. Accept track `div` of
1/2/4/8, default 1. Keep config version 2, published IDs and telemetry unchanged.
The absolute sixteenth clock wraps at 512; each track advances at its divisor,
and STOP/Apply uses max(length × div). Ties, shortened gates, conditions and
track loops follow the slowed track. No new dependency, protocol or ABI/SDK edit.

Use fixed five-voice sounding/pending state per track, release accepted voices
only after partial sink refusal, and keep releases ahead of attacks. Tests must
cover every malformed keys/div rule, 3/5-voice emission and unique live IDs,
tie/bar/gate/condition/stop/reset, refused attacks and releases with recovery,
slowed onsets/gates/ties/master STOP/A:B, Groove → NotesToCv three-voice graph,
1/17/128/max frames, allocation watch, and unchanged v1/G1 one-key/div1 golden.
Update the patch guide with availability, limits and usable wiring.

The combined budget below applies only to this bundled native implementation.
Separate G4/G5 PRs retain their individual task limits. Add `ci/windows` before
the source push and `ci/pi`/`ci/pi-accept` for actual private device-off Pi tests;
queued runs do not satisfy acceptance. Coda or another Claude-family reviewer
must provide a T2 COMMENT review on the final green head.

```acceptance
tier: T2
size: <= 1800 changed lines, <= 20 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'groove|ui_patch_conformance|sidecar' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - pi-run: ctest --test-dir build/native -R groove --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [ABI/SDK header change, Groove parameter/port ID change, one-key/div1 rendering change, more than 5 keys, speeds other than 1/2/4/8, protocol change, new dependency, changed threshold]
```

Signed: Codex (native builder), 2026-11-10
