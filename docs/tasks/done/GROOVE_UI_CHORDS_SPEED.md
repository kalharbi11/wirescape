<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove editor chords, scale and speed (#527)

Owner: existing Claude Groove UI builder, branch claude/adoring-shannon-viv6lm.
Continue #509 task/498-groove-editor and #516 task/510-groove-g2b with normal
non-force commits; merge newer remote commits before pushing. Do not duplicate PRs.
Read first: AGENTS.md, apps/studio/AGENTS.md, issues #527/#514/#523, merged
GROOVE_CHORDS_G4.md and GROOVE_SPEED_G5.md, #521 design and parent handoffs.

Order:
1. #509: guard malformed tracks/steps before rendering; no recursion/throw on
   empty arrays/null tracks. Preserve raw JSON/unknown keys; invalid config stays
   read-only. Regress load/save/undo and no unintended loadGraph. Integrate G1.
2. #516: integrate #509; persist root/scale in patch ui.groove[String(id)],
   preserving unrelated UI extras. Regress reload/history. UI-only; no config scale.
3. Existing Claude branch: stack on #516 and implement G4b/G5b together.
   keys only for 2–5 distinct MIDI keys, root first; omit for one note. No stored q.
   Derived chord names, picker/inversions, pad ladder, roll per-key edits, fake
   held-key entry; match #521. div=1/2/4/8, omit1; loop preview length*div.
   Keep draft/applied/refusal/transport rules, unknown data and History.
Native #526 owns config.h/dsp.h/runtime adapter/native tests. Do not edit those.
Current G1 lacks keys/div until native526 lands; never advertise unsupported live
playback. UI may stack now; accepted parent/native feature required before merge.
Update PATCH_DESIGNER_GUIDE in the UI PR. Different-session T1 review by Codex Lead.

```acceptance
tier: T1
size: <= 1600 changed lines, <= 20 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc "cd apps/studio && npm run typecheck"
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [native DSP or adapter edit, protocol/SDK/ABI change, new dependency, test weakening]
```

Pi not required for pure UI; fake engine/keyboard/mock bridge, no devices.
Signed: Codex Lead (Codex), 2026-11-10
