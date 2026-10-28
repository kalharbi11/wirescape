<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M2: port the Essentials, patch v2 and engine2 as the app engine

The M2 milestone, split into seven Codex tasks and a Claude review. Each task has its own issue, branch `task/<issue#>-<slug>` and draft PR (`Closes #<issue#>`), following AGENTS.md. Agent: Codex, Sol Medium, unless a task says otherwise.

**Read first (all tasks):**
1. `docs/specs/ABI2_PLAN.md` sections 5 (B2–B4) and 6 (rules every module follows).
2. `docs/reviews/M1-R_PLANNER_REVIEW.md`, the "Decisions" list and the L5/L6 findings.
3. `docs/reviews/M2_PORT_INVENTORY.md` (ChatGPT's source inventory; PR #17). Use it as a map and verify each claim you rely on. Its file:line pointers are against `main` at `6cc87f9`.
4. `docs/SDK_AUTHOR_GUIDE.md` and `native/sdk/template/`.

## Gate and order
- **M2-0** may start now. It must not touch `.github/` until PR #22 (M1-3) is merged.
- **M2-1 to M2-5** start only when PR #22 is merged into `main`. They are independent of each other and each branches from `main`. When two of them touch the same CMake list or test registry, the later one merges `main` in (merge commit, no rebase) after the earlier one lands.
- **M1-3b** (#32, Windows mono gap) starts when PR #22 is merged. It touches the engine and SDK hot path, so it runs before or after the ports, not interleaved with a port that edits the same file.
- **M2-6** starts only when M1-3b, M2-1, M2-2, M2-3 and M2-5 are merged. If they aren't, post a handoff on M2-6 and stop. (M1-3b may instead be closed by Claude accepting a written explanation; the issue will say so.)
- **M2-R** (Claude) reviews the milestone once M2-6 is in `status/review`.

## Ground rules (every M2 task)
- **Identity is sacred.** Keep module IDs (`essentials.<name>`), port IDs and parameter IDs from each `module.json`. New ports or parameters are *added*, never renamed or renumbered.
- **Equivalence.** A port must match its ABI 1 module at **max abs error 0** for default parameters, mono, unmodulated, at 44.1/48/96 kHz and blocks 1/17/256, using the existing equivalence harness (`build/native/abi2_equivalence.json`). Any non-zero delta needs a written reason in the PR and stops the task as `status/blocked` until Claude accepts it.
- **Unpatched-input behaviour** must match ABI 1 exactly (the inventory lists the cases: slew moves toward zero, scale_offset outputs the smoothed offset, mixer/output read zero, noise's PRNG advances while muted).
- **Module rules** from `ABI2_PLAN.md` section 6, including the test list in rule 9 (impulse/sine/sweep references, rate/block/channel matrix 44.1/48/96 × 1/17/max × 1/4/16, reset determinism, state round-trip, extreme-input guard, zero-allocation check).
- **No DSP math changes** while porting. Double state and exact transcendental calls stay where ABI 1 has them. Optimisation is a separate, later task.
- The existing ABI 1 module folders stay buildable and tested until M2-6 says otherwise. The ABI 1 shim stays bit-identical.
- No ABI header edits (`native/include/wirescape/*.h`). If one seems needed, stop with `status/blocked`.
- **Provenance:** do not invent provenance. Where `DESIGN.md` is missing, write what the source shows and mark the rest "Provenance: to be confirmed by the user". Claude collects these for the user.

## Machines: who runs what
Codex reaches both the **Pi 5** (`ssh pi`) and the user's **Windows laptop** (connection set up by the user on 2026-10-27; use whatever access the user configured, and don't guess hostnames or ask for credentials in comments). It runs all Pi and laptop work itself. If a machine can't be reached, say so in the handoff and keep going with work that doesn't need it.

Codex never hands builds, tests or benchmarks to the user. The only device actions reserved for the user are real audio or MIDI runs (anything with a `--confirm-*` flag) and the M2-L live session.

**Before setting any M2 PR to `status/review`,** post one "Machine evidence" comment on the PR with:
- **Pi 5 (Linux ARM64, Release, headless):** `python tools/dev.py build --headless` and `python tools/dev.py test` (or `ctest`) on the PR head, with pass counts and the toolchain line. For M2-6, also the soak table.
- **Windows:** the `windows-x64` CI job on the same head is enough for builds and tests. Laptop runs are needed only where a task asks for them (M1-3b, M2-6).
- The exact commit SHA. If the Pi is unreachable, say so, retry once later in the session, and only then hand off without it.

## Laptop rules (every task that uses it)
On the laptop, Codex works only inside the repository checkout and the build folders, plus `.tools/` for pinned toolchains. Never open audio or MIDI devices and never run a `--confirm-*` command. Never use admin rights, install system-wide software, change drivers, power plans or OS settings, or touch the user's files outside the checkout. Before benchmarking, record the power source (AC or battery), the power mode and the CPU model, and close nothing the user has open. If something heavy is already running, note it and measure anyway. Results go into PR or issue comments, never into tracked files. Don't commit machine-specific paths.

## Pi 5 rules (every task that uses it)
Reach it with `ssh pi` (user `codex`). On the Pi you may only build, run tests, run benchmarks and run valgrind/callgrind. Never open audio or MIDI devices, never use `sudo`, never install system packages, never change kernel or CPU-governor settings. Toolchains you need go under your home directory or `/tmp`. Record the compiler, CMake, kernel (`uname -r`) and the governor you observed (read-only) with every Pi number. Results go into PR or issue comments only, never into tracked files. If the Pi is unreachable, skip the Pi parts, say so in the handoff and continue.

---

## M2-0: benchmark tooling and Pi 5 realtime evidence (size/m, may start now)
Why Codex: needs the Pi and a native build.

**Deliverables:**
1. `tools/bench/compare_bench.py`. It takes one or two ABI 2 benchmark JSON files (`--base`, `--head`) and prints a Markdown table of the median ns per row plus the `mono_ratio`, `poly8_vs_eight_mono_ratio` and `modulated_ratio`, with head/base deltas. `--gate` checks mono ≤ 1.05 and poly ≤ 1.00 and exits 1 when either fails. Python 3.9+, stdlib only. Unit tests in `tools/foundation/tests/`.
2. **A paced soak tool** `wirescape_soak` (in `native/engine2/tests/` or `native/cli/`). It loads a built-in graph by name (`mono`, `poly8`, `six_essentials_shim`, `voices:<n>` = n-voice VCO→VCF→ADSR→VCA poly synth), then renders in a loop paced by `steady_clock` at the block period. This simulates an audio callback without opening any device. It writes JSON: render-time min/median/p99/p99.9/max per block, the count of blocks over 50% / 100% of the period, and the configuration. Arguments: rate, block, seconds, graph. The render thread stays allocation-free (reuse the existing hook). CTest runs a 1-second smoke at 48k/128 with **no timing assertions**.
3. **A voice budget mode:** `--budget` raises `voices:<n>` from 1 to 16 and reports the largest n whose p99 stays below 50% of the block period.
4. A new guide, BENCHMARKING.md in `docs/`: how to run `abi2_cost.py`, `compare_bench.py` and `wirescape_soak`, locally and on a Linux ARM64 board. No machine-specific paths or host names.
5. **After PR #22 is merged** (merge `main` in first): change `.github/workflows/ci.yml` so both `linux-arm64` and `windows-x64` print `build_reports/abi2/benchmark.json` into the job log and the step summary. Today Windows writes no summary and the numbers are only in artifacts, which some agent environments cannot download. Change only that summary step. Allowed protected path: `.github/workflows/ci.yml`, this step only.
6. **After PR #22 is merged:** re-run the Pi evidence below on the merged `main` (the first run measured `main` at `6cc87f9`, before the M1-3 speed-ups) and post it as a second table next to the first. Same toolchain line.

**Pi evidence (PR comment):** Release build on the Pi, then `wirescape_soak` for 10 minutes each at 48k with blocks 64, 128 and 256 on the `poly8` and `six_essentials_shim` graphs, plus `--budget` at 48k/128. Report the table, overruns and the toolchain line.

**Acceptance:** CI green; the new tests pass; soak smoke in CTest; Pi table posted. Expected size: 350–500 lines, 8–10 files.

---

## M2-1: per-bus latency and staged reconfigure (size/m, after #22)
Why Codex: engine2 scheduling code.

Implements M1-R decision 5 and findings L5/L6.
1. **Latency:** count only *signal* edges that reach an output bus, reported per bus (`engine2.h` gains a per-bus query; the ABI header does not change). Event and modulation edges do not add latency. Feedback delay is a property of the loop and is reported per SCC, never added to forward latency.
2. **Staged reconfigure keeps untouched instances:** `reconfigure()` re-creates and re-prepares only nodes whose module, config or prepare inputs changed. Untouched instances keep their DSP state (phase, filter memory). Stable-port cable remapping and dropped-connection reporting behave as the spec says (ABI2_SPEC D12).

**Tests:**
- Latency: chains with module latency on signal, event and modulation edges; two buses with different paths; a feedback loop.
- Continuity: render N blocks, reconfigure an unrelated node, render M blocks. The untouched VCO→VCF output is bit-identical to a run without the reconfigure.
- Zero allocations in render after reconfigure.

**Acceptance:** CI and the `sanitizers` job green; no change to existing test expectations. Expected size: 250–400 lines.

---

## M2-2: port the utility Essentials (size/m, after #22)
Why Codex: native ports with an equivalence harness.

Port **mixer, noise, output, scale_offset and slew** to `native/modules2/<name>/` (`dsp.h`, `module.cpp`, `module.json`, `README.md`, `DESIGN.md`, `tests.cpp`, `LICENSE` as in the existing ports). **One commit per module.** Add each to the equivalence harness and the ABI 2 test registry.

Watch for:
- `noise`: the PRNG sequence and its advance-while-muted behaviour must match bit for bit. **Channel 0 uses the ABI 1 seed `0x12345678` exactly** (equivalence is mono). Channel `c > 0` uses a fixed, documented derivation from that seed and `c` (for example a splitmix32 step), so voices differ deterministically. There is no engine seed today; don't add one.
- **Banned-include test (M1-3 review L-c):** `tools/foundation/tests/test_banned_includes.py` scans every `.cpp` under `native/modules2/`. Exempt files named `tests.cpp` (host-side test code) and add a test that a `tests.cpp` with `<vector>` passes while a `dsp.h` with it fails. `dsp.h` and `module.cpp` stay strict.
- `slew` and `scale_offset`: unpatched behaviour (see the ground rules).
- `mixer` and `output`: missing inputs read as zero; `output` is the bus sink; check how engine2 binds buses before choosing its shape and document it in `DESIGN.md`.

**Acceptance:** max error 0 on defaults for all five; the section 6 test matrix per module; zero allocations; CI and sanitizers green. Expected size: 600–900 lines, about 30 files (mostly small module files). That is pre-approved.

---

## M2-3: port clock, function_envelope and ladder (size/m, after #22)
Why Codex: stateful DSP and event semantics.

Same layout and rules as M2-2, one commit per module.
- `clock`: rounded tick lengths, the threshold run and reset-edge phase (inventory: high risk). Transport sync is **not** in scope (M6); keep free-running behaviour identical.
- `function_envelope`: gate-edge latch, curve function, and falling from the held level.
- `ladder`: private pole state, nonlinear drive and non-finite input handling. Keep double precision where ABI 1 has it.

**Acceptance:** as M2-2. For `ladder`, also run the existing `returned_filter_stress` cases against the ABI 2 port. Expected size: 500–800 lines.

---

## M2-4: behaviour regressions for the five existing ports, and per-module CV (size/m, after #22)
Why Codex: needs builds and signal tests.

1. **Regression tests** for the differences the inventory found (open question 3): ADSR note ownership and gate-cable precedence over note events; LFO default `sync` with and without transport; VCA ramp versus constant fast path; VCF NaN/Inf inputs and coefficient smoothing; VCO note-off, choke and negative frequency. Where ABI 2 behaviour intentionally differs from ABI 1, the test pins the ABI 2 behaviour and the module's `DESIGN.md` states the difference.
2. **Per-module CV (B4):** every continuous parameter of all thirteen Essentials is modulatable through D7, and each module has the explicit musical ports the plan names (VCO pitch/FM, ADSR gate). Write the ADSR event-versus-cable gate precedence into `docs/SDK_AUTHOR_GUIDE.md`.
3. Check that each of the five `native/modules2/<name>/DESIGN.md` files has a provenance section (sources, pinned revisions, licence). The ABI 1 folders of adsr, lfo, vca and vco have no `DESIGN.md`, so fill gaps from the source only and follow the provenance rule.

**Acceptance:** tests pass on both runners and under sanitizers. Expected size: 300–500 lines. If item 2 needs changes to modules still being ported in M2-2/M2-3, do it for the five existing ports only and list the rest as a follow-up.

---

## M2-5: patch document v2 and lossless v1 migration (size/l, after #22)
Why Codex: document code, fixtures and render comparisons.

**Allowed protected paths:** `docs/specs/patch_v2_draft.schema.json` and `docs/specs/patch_v2_draft.example.json` (finalise them; you may rename them to `patch_v2.schema.json` / `patch_v2.example.json` and update links), and `docs/specs/ABI2_SPEC.md` (the "Patch schema" paragraph only).

**Planner decisions (settle open questions 1 and 4 from the inventory):**
1. **Parameters:** v2 keys parameters by the string IDs in each module's `module.json`. v1 keys are stringified numeric descriptor IDs; migration maps them through `module.json`. A v1 key with no mapping goes into that module's `preserved` object, unchanged.
2. **Unknown modules** (a type the catalog doesn't know): keep the whole v1 module object verbatim in the v2 module entry under `preserved`, keep its cables, and mark it inactive. Saving writes it back unchanged.
3. **Unknown fields** at any level are kept and re-emitted on save. Byte-identical output is not required; value-identical JSON is.
4. **DSP state:** v1 has none. Migrated modules get no `state` blob and start from reset. Do not invent a state-restore path.
5. **Duplicate keys, unknown future versions (e.g. `future_patch_v99.json`) and malformed files:** refuse to load with a clear error and never overwrite the file. This keeps today's behaviour for the legacy fixtures.
6. Load accepts v1 and v2; **save always writes v2**. The first save of a v1 file writes a sibling backup `<name>.v1.json` if one doesn't exist.

**Deliverables:** reader and writer in `native/document/`, the migration, the schema and example finalised, and a relational validator (unique IDs, cable endpoints exist, `feedback_block ≤ max_block`).

**Tests:**
- `Six_Essentials_Demo.json`: load v1 → save v2 → reload → offline render identical (same WAV hash) to the v1 render.
- Every file in `native/tests/fixtures/legacy_documents/` behaves exactly as before.
- Unknown fields, unknown modules and unmapped parameter keys round-trip v1 → v2 → v2 with value-identical JSON.
- Validator negative cases.

**Acceptance:** CI and sanitizers green. Expected size: 700–1,000 lines. Stop with a handoff above 1,200.

---

## M2-6: engine2 becomes the app engine (size/l, after M2-1, M2-2, M2-3 and M2-5 are merged)
Why Codex: app integration, Windows UI tests and the Pi.

1. `StudioController`, the CLI render path and the offline render use engine2. All thirteen Essentials load as ABI 2. ABI 1 modules (third-party or not yet ported) still load through the shim.
2. Documents load v1/v2 and save v2 through M2-5's code.
3. The audio callback path calls engine2 `render` with no allocation, locks or logging. Tests check this without opening a device.
4. The ABI 1 engine (`native/src/engine.cpp`) stays in the tree and keeps its tests, for the shim and for comparison. Removing it is a later decision.
5. Update `docs/ARCHITECTURE.md` (allowed: it describes what is built).

**Tests:**
- Each example and legacy fixture renders offline. The output either matches the ABI 1 render exactly or has a listed, explained delta (for example ADSR gate precedence). Any listed delta stops the task as `status/blocked` until Claude accepts it.
- UI offscreen tests at both sizes; package check; zero allocation in the callback path.
- **Pi:** a headless build of the app engine and CLI, then `wirescape_soak` on the Six Essentials demo patch through the real document loader, 10 minutes at 48k/128. Post the table.

**Never** run a real audio device. The user does the first live session on Windows.

**Acceptance:** all four CI jobs green; Pi table posted; `docs/ARCHITECTURE.md` updated. Expected size: 600–900 lines.

---

## M1-3b: close the Windows (MSVC) mono gap and gate Ir in CI (issue #32, size/m, after #22)
Why Codex: needs MSVC on the user's Windows laptop, profiling and CI runs. Runs on the laptop (see Machines). Background: `docs/reviews/M1-3_REVIEW.md`.

The `windows-x64` runner measured ABI 2/ABI 1 mono at 1.097×. Every other measure passes (arm64 0.865×, Pi 0.711×, Ir 0.860×), so the likely cause is MSVC codegen, not framework design. Don't assume that; measure it.

1. **Baseline first.** Build `main` at `6cc87f9` (before M1-3) and current `main` with the same interleaved benchmark driver, the way the Pi baseline was done for #22. Run both on the laptop (MSVC Release, plugged in, nothing else running), 5 runs each. Report the medians and the run-to-run spread. Then get three `windows-x64` CI runs of current `main` (re-run the job; don't push empty commits) and report all three ratios.
2. **Find the cause.** Use `/Qvec-report:2` on `engine2.cpp`, `wirescape_sdk.h` users and the five `modules2` DSPs to see which hot loops MSVC didn't vectorize: `ws::finite_samples`, the VCA row loop, the parameter fill loop and the input copy loop. Optionally build the same code with `clang-cl` to separate compiler from code. Report per-probe medians for `abi2_mono`, `vcf_constant` and `vcf_audio_rate_cutoff`.
3. **Fix within scope.** Engine/SDK/module-port code only, no ABI header change, no DSP math change, and no `/fp:fast` or other global flags that change results. Typical fixes: `__restrict` or local copies so MSVC can prove there's no aliasing; replacing `std::max` in the exponent reduction with an OR-reduce; hoisting loop-invariant loads.
4. **CI gate on Ir (review L-b).** Add `--gate` to `tools/bench/abi2_cost.py` (exit 1 when `mono_ratio > 1.05`), with unit tests, and use it in the `sanitizers` job's callgrind step. Allowed protected path: `.github/workflows/ci.yml`, that step only.

**Acceptance:** `windows-x64` mono ≤ 1.05× in at least 2 of 3 CI runs, and on the laptop; arm64 and Ir ratios not worse than #22 (0.865× / 0.860×) by more than 0.01; equivalence max error 0, shim bit-identical, zero allocations and the poly WAV hash unchanged; all four CI jobs green. Expected size: 100–250 lines. **Stop rule:** if two reasonable attempts don't get there, set `status/blocked` with the vectorizer report, medians and spreads. Claude then decides whether to accept the gap with a written explanation.

---

## Codex continuous queue (run in this order; don't wait for review between tasks)
Codex runs this list top to bottom in one long session. Open one draft PR per issue, post a handoff on each issue when it's done, set `status/review` and move straight on to the next item. **Never merge.** When an item's prerequisite isn't merged yet, skip it (post a one-line "waiting on #N" handoff) and take the next one; come back when it's merged.

Updated 2026-10-27 (late check-in). Merged: M2-1 (#38) and the portability probe (#55). Accepted and waiting for the user to merge, in this order: #39 (M2-2), #37 (M1-3b, accepted with the gap moved to #64), then #40 and #41.

| # | Item | Prerequisite | Machine |
|---|---|---|---|
| 1 | **Merge upkeep:** after #39 merges, merge `main` into #40, then (after #40) into #41. Keep both sides' test registrations, run `python tools/dev.py test`, push, and confirm CI is green | the PR ahead of it merged | cloud |
| 2 | #28 M2-5 (draft PR #48, CI green): finish it, add Pi evidence, handoff, `status/review` | none | Pi |
| 3 | #50 M2-0c (draft PR #63, CI green): post the 600 s Pi run with the preempted-block count, handoff, `status/review` | no other Pi run at the same time | Pi |
| 4 | #64 M1-3c clang-cl comparison (measurement only, no source changes) | #37 merged; laptop on | **laptop** |
| 5 | #29 M2-6 engine2 becomes the app engine. **#32 no longer blocks it** (see the decision on #32) | #39, #40, #48 merged (#38 is in) | Pi + laptop |

Copilot owns #49 and #56 (after the reset on Oct 31); Codex skips them. ChatGPT owns the docs studies.

Rules for the queue:
- Every PR ends with the "Machine evidence" comment from the Machines section. Run the Pi work while CI runs, so it doesn't add wall time. Never run two Pi benchmarks or soaks at once.
- Never benchmark on the laptop while a build or test runs there; the laptop and the Pi may work at the same time.
- Items 1–3 can run at the same time on different machines. Item 2 runs only on the laptop.
- Each item branches from the latest `main`. If an earlier item touches the same CMake list or test registry and merges first, merge `main` into your branch (merge commit, no rebase).
- Run `python tools/dev.py test` before every final push. The final push of each PR runs CI; WIP pushes may use `[skip ci]`.
- If a PR gets review comments from Claude while you're on a later item, finish your current commit, fix the comments on the earlier branch, push and return.
- Hard stops: any ABI header edit, any non-zero equivalence delta, any test removal or tolerance change, a diff past its size budget, or two failed attempts at the same failure. Post a `status/blocked` handoff for that item and take the next one.
- Collect every "Provenance: to be confirmed by the user" in one comment on #29 so the user can answer them in one go.

---

## M2-P: portability compile probe (issue #44, Copilot, size/s, report only)
Why now: the plan keeps CLAP/Daisy adapters for M8, but every module written before then inherits the SDK's constructs. A cheap compile probe now shows which ones would block a bare-metal build.

Add `.github/workflows/portability.yml` (triggers: `workflow_dispatch` and `pull_request`; **not** a required check; `continue-on-error: true` on the compile step). On `ubuntu-latest`, install `gcc-arm-none-eabi`, then for each folder in `native/modules2/*/` compile `module.cpp` with `arm-none-eabi-g++ -std=c++17 -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -O2 -fno-exceptions -fno-rtti -ffunction-sections -c -I native/sdk -I native/include -DWS_SIMD_SCALAR`, once with `-fno-exceptions -fno-rtti` and once without. Write a Markdown table to the step summary: module × {with exceptions, without} → pass/fail plus the first error line. Don't change anything under `native/`. Claude turns the findings into SDK tasks.

---

## M2-L: user live session (issue #45, the user, after M2-6 merges; gate for M2-R)
On the laptop, in the real app with a real audio device, 30–60 minutes:
1. Build a patch from scratch: VCO → VCF → VCA → Output with an ADSR, plus an LFO modulating the cutoff through a modulation cable.
2. Play it from the on-screen keyboard: single notes, held chords, fast repeats. Listen for stuck notes, clicks on note-on/off and voices cutting out.
3. Turn every knob on the patch while it plays. Listen for zipper noise, jumps and crackles.
4. Save, close the app, reopen, load. Does it sound the same?
5. Load `Six_Essentials_Demo.json` and an older patch from before M2. Do they sound as you remember?
6. Leave a patch running for 20+ minutes while doing something else on the laptop. Any dropouts?
7. Note anything confusing in the UI.

Post the list with ✅/❌ and one line per problem on #45. Each ❌ becomes an issue before M2-R closes the milestone.

---

## M2-R: Claude milestone review (agent/claude, milestone-review)
Starts when M2-6 is in `status/review` **and** the user has posted the M2-L checklist (#45). Claude reviews M2-0 to M2-6 against this file and `ABI2_PLAN.md` B2–B4 acceptance, builds and runs sanitizers, turns each M2-L ❌ into an issue, and decides whether M3 (UI for the contract) can start. Before reviewing, Claude asks the user what ChatGPT should prepare.
