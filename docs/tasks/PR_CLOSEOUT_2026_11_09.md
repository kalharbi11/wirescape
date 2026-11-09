<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Open-PR closeout queue: 2026-11-09 (Keel)

D18 says finish before start: merge or close what's open before any new module or feature work. This file replaces the READY list in `CLAIMABLE_2026_11_09.md`. The Qt retirement and cleanup packets (`docs/planner/CLEANUP.md`, R1–R7) start **after** this queue drains, except where Compass says otherwise. Each lane below names its PRs in merge order, the exact obstacle, and the ruling that clears it. Claim on the issue, continue the existing branch (no duplicate PRs), and sign exact-head evidence.

## Done by Keel today
- Merged: #456 (lead docs), #438 (Macro baseline, merge commit), #441 (OSC inventory), #390 (MIDI Learn seam, T2 final review → #381 done).
- Closed: #82 (Qt-era OSC, superseded; branch kept for porting), #443 (empty net diff), #406 and #423 (Chord/Notes→CV parked per D18; branches kept).
- #427: merged main → head `486d6b9`, approval posted; the gate merges on green.
- Rulings posted: Groove playhead seam (#432), #386 case 4, device routes/rate/block (#293), OSC host/consent/Monitor (#72), #400 T2 review (conditional approval).

## Rules that apply to every lane
- **Static** isn't a required check. Clang-format diagnostics on lines you changed must be fixed: run the pinned formatter on changed hunks only. Cppcheck diagnostics on untouched lines, or in files whose blob equals main's, are inherited. List them by file:line in the handoff, and Codex Lead merges past them (delegation c). Never add suppressions to get green, except a reviewed narrow inline one (see #438).
- **Windows** runs on non-draft PRs or with `ci/windows`. Mark ready, or add the label and push, before asking for merge. Required checks: `guard`, `linux-arm64`, `windows-x64`, `sanitizers`.
- **Stacks:** merge the base first, then retarget the child to `main` and merge main into it normally.
- **Spec edits** that a ruling authorises go in the implementing PR, and the issue carries `Protected path named:`.

## Lane A: merge captain (Codex Lead)
1. #427: the gate merges on green at `486d6b9`. If a check fails, fix it on the branch; don't take a new copy. After the merge, verify the Pages deploy and the 8 live patch names (#426).
2. #323 Quantizer: Keel's helper is merging main (catalogue union). When the new head is green, the T1 review stands (the merge is mechanical). Classify the inherited `host_keyboard/dsp.h:38` static diagnostic and merge.
3. #400: apply Keel's conditional approval once the formatting-only head is green (delegation d).
4. Then work the order below: #442 → #310 → #210 → #405 → #410 → #451. Each needs a different-session T1 review on its exact head (Lane H).

## Lane B: 16-step sequencer (#405 → #410), existing sequencer builder
- #405 (base main): fix authored clang-format and the cppcheck emit-macro finding, then run fresh exact-head native/Studio acceptance plus `ci/pi` evidence. Pending NOTE_OFFs are retried on the next block and never dropped. v1 8-step state loads with length 8 and steps 9–16 off. Keep #412's curated starters. T1.
- After #405 merges, retarget #410 to `main`, merge main, rerun Studio/Playwright, and post a touch-layout screenshot for Compass. T1.

## Lane C: Groove S1 (#451, then playhead seam, then #458)
- #451: the nominal 102-cycle fixture must drain egress between blocks; don't raise caps. Add the deliberate-saturation fixture: NOTE_OFF retry, reload while a release is pending, and event identity. Then format, run accept454 and Pi. You have 6 lines of budget left, so put tests over budget in a split, not a cap raise.
- Then the new small T2 PR `task/432-groove-playhead`, exactly as Keel's ruling on #432: `t1_pos`/`t2_pos` outputs, a `playhead` telemetry map, the `telemetry.playhead` capability, WASM parity, a spec §7 paragraph and the Studio Apply gate. Claude does the cross-family review.
- #458 (design study, T1): needs one different-session review and a visual QA screenshot. Its `codeql (c-cpp)` red on an HTML-only diff gets one rerun; if it repeats, Codex Lead classifies it. It merges as reference only. The functional S1(b) editor comes after the playhead PR.

## Lane D: MIDI (#210, then #287)
- #210: required checks and Pi pass, and the T1 review has no finding. Format the authored files the review listed and classify cppcheck by file, then merge main, rerun and get a changed-head re-review.
- #287 Learn UI is **unblocked** (#390 merged). Build it on a new branch: arm `midiLearn`, read `status.midi.learn`, write persisted `midi.cc_to_cv` `config.rows` per D8 (add a module when 8 rows are full), and keep unknown fields. T1, mock-bridge + FakeEngine tests.

## Lane E: Spectral Interleave (#310)
- Fix authored static formatting. Run the three task-specific Pi probes (`ci/pi-accept`) on the exact head, then get an independent T1 review and update the patch-guide limitation. Interleave first. If Vocoder holds up the release, split it out with stable slots, as in the queue rules.

## Lane F: real-app evidence (#442, then #296)
- #442: add case 4 per Keel's ruling on #386. That's a Studio mock-bridge spec driving `status.audio.state` running → device-lost → reconnecting → running through `RealEngine`, plus a citation of the native `audio_recovery_tests` CTest. No new hooks. Add `docs/tasks/T386_ELECTRON_E2E.md` (now named), mark the PR ready and get a T1 review.
- #296 private 3600 s fake-device edit soak: unchanged, still owned by the private-runner lane.

## Lane G: engine T2 (#400, #399/#293)
- #400: formatting-only commit on changed hunks, merge main, then Lane A applies the conditional approval.
- #399/#293: implement Keel's ruling: optional `backend`/`outputChannels`/`supported`/`unsupportedReason`, mono listed but disabled, `setSampleRate`/`setBufferFrames` stream-only restart with rollback, id-only identity and the old route kept on refusal. Spec §4/§6 goes in the same PR. Claude does the T2 review.

## Lane H: reviewer (ChatGPT, a different session from each author)
T1 COMMENT reviews on exact heads, in this order: #442, #310, #210 (changed head), #405, #458, #451. Check the contract the change touches (spec or native reader), not style. Post severity with file:line.

## Lane I: Pi preparation (#450, #452, then #453)
- #450: fix the CMake-cache identity false positive (Keel ruling 6732522458) and make `--check-only` truly read-only (no `__pycache__`: use `PYTHONDONTWRITEBYTECODE`/`-B`). Node stays strict, and the owner installs Node 22 on the runner.
- #452: commit the short-header ELF regression tests. The display smoke uses `xvfb-run` when it's present and reports **blocked** (never pass) when it isn't. The owner installs `xvfb` on the runner once.
- #453: after both merge, update the guide to one accepted SHA, then merge as T0. It's renamed `docs/PI_SETUP.md` under CLEANUP R5.

## Lane J: OSC slice A (#72)
- Use a fresh main-based branch `task/72-osc-slice-a`. Port codec, service, `osc.in` and `monitor` from `task/72-osc-monitor` with their offline tests (keep the nonzero-timestamp graph test) and regenerate the catalogue. No sidecar or Studio wiring yet (that's slices B/C, T2). Stable IDs, localhost-only tests. T1.

```acceptance
tier: T0
size: <= 160 changed lines, <= 3 files
must_pass:
  - python -c "from pathlib import Path; t=Path('docs/tasks/PR_CLOSEOUT_2026_11_09.md').read_text(); assert sum(l.startswith('## Lane ') for l in t.splitlines()) == 10; assert len(Path('docs/planner/STATE.md').read_text().splitlines()) <= 60"
stops: [runtime or protocol change, threshold weakening]
```

Signed: Keel (Claude), 2026-11-09
