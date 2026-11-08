<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Build lead handoff, 2026-11-08 (#376)

Owner requested exactly three ChatGPT chats with combined tasks. These are assignments to paste, not claims that chats have started. Repository: kalharbi11/wirescape.

Common instructions: read current AGENTS.md, your issue and latest handoff, then its task file and acceptance in full. Claim before editing. One issue/branch/PR per task; continue existing branches. Maximum three active items per chat. Run the issue's must_pass commands (python tools/dev.py accept NUMBER); hosted CI is your build when local builds are unavailable. Report exact head and actual evidence; queued Pi runs are not passes. Never open devices or SSH to the Pi. Sign ChatGPT (<role>), 2026-11-08.

## Agent 1: telemetry (two items)
1. #358 / #369: after #368 merges, integrate current main without rewriting history. Verify native, wasm, deployed-site and telemetry JSON timing evidence; follow the latest Compass/Keel review requirements. Existing A3 is already implemented; do not duplicate it.
2. #359: after a host merges, use task/359-lfo-phase. Read docs/design/ENGINE_TELEMETRY.md and docs/tasks/T350_ENGINE_TELEMETRY.md. Enable measured essentials.lfo phase in available hosts; integrate the second host after it lands. Null-sink/native tests prove real phase in [0,1); Studio Playwright proves dot updates only from snapshots and unavailable state without phase. Keep <=30Hz, no interpolation. Independent T1 review required.

## Agent 2: module integration (three items)
Continue #314/#345 Delay, #316/#348 Reverb, #317/#344 Resonator on their existing branches. Read each issue's latest handoff and task brief before touching it. Integrate shared probe #367, retaining current main catalogue/module registrations and removing duplicate probe definitions. Run each task's native/Studio acceptance and its device-free Pi CPU command. Resonator eight-channel timing waits for #373 and uses --channels 8. No threshold changes. Independent T1 review and exact-head required checks before merge.

## Agent 3: independent review and soak (three items)
1. Review #373 head 852a1ce main-relative diff (five files, 87 changed lines). Check requested channels reach process context, capacity remains 16, output scanning and evidence validation remain fail-closed. Local merged Python suite passes 8/8; native/CTest/full hosted evidence remains pending. Report severity/file:line and inspect final-head required checks; do not author repairs and then self-review.
2. Review #342/#346 oscillator UI against its task and current Studio rules. Diagnose conflict before edits; if you repair it, obtain another-session review. Run its acceptance including Studio tests.
3. #296: read issue acceptance and latest handoff, prepare/run the supported fake-device Pi soak through ci/pi-soak only. Link completed exact-head evidence. Do not change runner workflows or open devices.

## Codex Lead retained work
- #373 integration pushed as 852a1ce, preserving b883ecb and main 4c8bfe0 as parents. Local Python 8/8 pass; native/full CI unverified.
- #292 implemented in draft #378, head 1ac6d71. Local metrics/allocation regression and changed-unit C++ syntax checks pass; full native/Studio/hosted/Pi evidence pending. The raw 64-tap measurement command is implemented; numeric tolerance/aggregation awaits Keel's ruling. No thresholds changed.
- #293 can proceed after merged audio recovery #301. #287 remains blocked on raw CC identity/value contract; batch with genuine T2 questions for Keel.
- #304/#323/#310 remain in queue; activate them when a module slot opens, without exceeding three active module items.
- Owner Test Session 1 #271 remains Phase 1 sign-off; any reported failure preempts these lanes.

Signed: Codex Lead (Codex), 2026-11-08
