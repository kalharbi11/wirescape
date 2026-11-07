<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Module lane (Compass board)

Owner decision, 2026-11-07: a module-building lane runs now, beside Phase 2, until Monday 2026-11-09 06:00 and then as normal Track S work. **Compass plans and reviews; Codex builds most modules; Compass/subagents take small or stuck items.** It never preempts a #271 owner failure or a Phase 2 critical-path fix. Keep this file short; history lives in git.

## Queue (pick from the top; one issue = one branch `task/<issue#>-<slug>` = one PR)
| # | Module ID | Name | Task file | Why now | Depends on |
|---|---|---|---|---|---|
| 1 #314 | `fx.delay` | Delay | `docs/tasks/S2_1_DELAY.md` | Biggest jam value per line of code; the Pi patches are dry | none (CPU probe line waits for #304) |
| 2 #315 | `essentials.quantizer` | Quantizer | `docs/tasks/MOD_QUANTIZER.md` | Small; turns LFO/Random/Chaos into melodies | none |
| 3 #316 | `fx.reverb` | Reverb | `docs/tasks/S2_2_REVERB.md` | Second effect; shares the stereo-in/out pattern with #1 | none; reuse #1's patterns if merged |
| 4 #317 | `essentials.resonator` | Resonator | `docs/tasks/CHAOS_R1_RESONATOR.md` | **CHAOS rebuild step 1**: the body as a playable module | none |
| 5 #328 | `sound.blend` | Blend | `docs/tasks/S1_2_BLEND.md` | Next S1 process | PR #329 approved |
| 6 #332 | `sound.cross_mod` | Cross Mod | `docs/tasks/S1_CONJUNCTIONS_MODULES.md` (#3) | Next S1 process | none (#329 conventions) |
| — | `sound.spectral` | Spectral | #302/#304, #305/#310 | in flight | — |

Not queued yet (and why): S3 sample buffer + Sampler (T2, needs a Keel ruling first); MIDI Out/Clock (engine output path, T2); CHAOS-lite custom panel (waits for the owner's redesign); `ports.*` third-party ports (licence/dependency work, Phase 5); GUTTUR (on hold).

## CHAOS rebuild path (owner asked; Compass, 2026-11-07)
CHAOS-lite (`essentials.chaos_lite`) stays as is. The rebuild goes **component first**, from `docs/design/ports/CHAOS_DECOMPOSITION.md` §3, all original code (no Surge/sst, no tape):
R1 **Resonator** (the three-comb body, now) → R2 **Waveshaper** (after R1 is heard by the owner) → R3 CHAOS as a starter patch of Essentials (VCO ×2 → Mixer → Resonator → Waveshaper → VCF → Delay/Reverb) → R4 a CHAOS subpatch/macro once Phase 5 subpatches exist. The owner judges the sound at each step; no CHAOS-identical claim.

## Common rules for every module in this lane
- Follow `docs/SDK_AUTHOR_GUIDE.md` (§1 folder, §8 realtime, §12 tests + definition of done). Claude Code: `new-module` skill. Original code unless the task says otherwise; record "original code" and the textbook source of each algorithm in `DESIGN.md`.
- **Standard wiring** (copy what #304 touches; doesn't count toward the size budget): `native/modules2/<name>/CMakeLists.txt`, `native/engine2/CMakeLists.txt`, `native/control/module_catalog.h` + `.inc`, the module count in `native/control/app_engine_tests.cpp` / `native/document/app_patch_tests.cpp`, `apps/studio/public/catalogue-presentation.json`, an optional `apps/studio/public/panels/<id>.json`, and the regenerated `apps/studio/public/wirescape_modules.json` (generator, never hand-edited).
- **Effects are one voice:** effect inputs are `WS2_SUM` (a poly cable is summed), outputs are one channel. Stereo = `left`/`right` ports linked with `stereo_pair`; an unconnected `right` input follows `left` (check `connected()`).
- Knobs are modulatable by the host; don't add CV inputs for them. Stepped switches use `WS2_STEPPED`.
- All buffers sized in `prepare` for 96 kHz and the declared maximums (`WS2_LARGE` for delay memory). No allocation in process/reset. State: params only unless the task says otherwise; delay lines clear on load.
- IDs in each task are **final once the PR merges**. Questions about them go in the issue as `Compass: …` before merge.
- Tier T1 (one review from a different session; Compass reviews this lane). Pi evidence: the `pi-run:` CPU-probe line once `tools/module_cpu_probe.py` exists (#304); label `ci/pi-accept`.

## Status (update in the PR that changes it)
- 2026-11-07: lane opened; issues #314–#318 filed for rows 1–5 (Compass).
- 2026-11-07 16:00 UTC (Compass check-in): #315 Quantizer → PR #323, CI green on the required jobs, needs a different-session T1 review. #314 Delay is being built by a Compass subagent (nobody had claimed it). #316 Reverb and #317 Resonator are unclaimed (asked Codex Lead). #304 Spectral is fixed after the audit and waits for `pi5/accept`. #318 Blend waits on #304.
- 2026-11-07 19:00 UTC (Compass): #329 Blend reviewed (no High/Medium) and approved; #318 closed as a duplicate of #328. #314 Delay is built locally (`ab154ad`), not pushed: this session has one push branch, held by #323. #323 asked the sound-kit builder for a T1 review. #316 and #317 still unclaimed. #332 Cross Mod filed.

Signed: Compass (Claude), 2026-11-07
