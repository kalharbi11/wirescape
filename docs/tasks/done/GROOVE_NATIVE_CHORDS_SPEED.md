<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove native chords and speed (#526)

Native G4a then G5a in one PR. Read first: `GROOVE_CHORDS_G4.md`,
`GROOVE_SPEED_G5.md`, `GROOVE_PLAYABLE_V1.md`, `CHORD_MODULE_403.md`
(identity/capacity rulings), and `native/modules2/groove/`. Start runtime work
after #505 merges. The existing Claude Studio builder owns G4b/G5b; this task
does not edit Studio source or close #514/#523.

Accept note-only `keys`: 1–5 distinct integer MIDI keys, root first and equal
to `key`. Omission means the existing one-key step. Accept track `div` of
1/2/4/8, default 1. Keys after the root are distinct and ascending.
Keep config version 2, published IDs and telemetry unchanged.
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

Keel 2026-11-11: G1 single-key/div1 note IDs stay byte-identical. Chord IDs
need no instance tag; scope is source instance/output. An ID cannot be reused
while live or pending; matching NOTE_OFF must precede reuse. Add overlapping
chord steps plus retrigger coverage of this invariant. This supersedes the old
G4 task's instance-tag wording; no new ID design or golden rebaseline.

## Closeout pickup (existing PR #528, no replacement PR)

Use task/526-groove-native-chords-speed; last inspected implementation c6697a4.
Claim a scoped closeout on #526/#528 before edits; preserve any newer pushes.
1. Normally merge current main (includes #567) and keep every feature/main fix.
   No force-push, contents-only fake merge or restored Qt test probe.
2. In native/document/groove_observation_tests.cpp rename only PolyRig's
   shadowing refuse_off and render members plus their PolyRig call sites.
   Preserve Rig behavior and every partial-refusal assertion.
3. Format changed lines with clang-format 18 in groove_observation_tests.cpp,
   groove_runtime_tests.cpp, runtime_module_config.h,
   runtime_module_config_tests.cpp and native/modules2/groove/config.h.
   Do not churn untouched formatting or suppress analyzer findings.
4. Add/verify Keel's overlap/retrigger identity test above; update G4 task
   wording under senior task-path authorization. Keep mono/div1 goldens exact.
5. Close the source review's transient POD Medium with Coda: Config changed
   6212 -> 12372 bytes; JSON is persistence, old v1 POD still decodes.
   Document the internal-only contract or preserve old v2 decoding if required;
   do not call an unanswered review item resolved or change persisted JSON.
6. Push normally without skip-ci, run accept 526 and every existing gate.
   Prior Windows failed apps/desktop/tests/windows-setup.spec.cjs:34 waiting
   for body data-ready, before native calls. Recheck after main integration;
   one targeted cold-start retry is allowed by review. If it repeats, post
   renderer/preload/engine logs and trace; do not widen the five-second limit.
7. Verify actual final-source private Pi Groove CTests. Historical c6697a4
   passes do not certify a later source. Obtain Coda final-head T2 COMMENT.
8. Only then ready/approve/merge through gate. Notify #534 to integrate main,
   flip guide availability and run final task/Web plus Coda T1. Parent
   #514/#523 stay open until native and UI acceptance both pass.

The Studio contract check on #528 is already compatible; do not redo it or
build a capability gate. The UI agent can prepare fixes/CI while this proceeds.

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
