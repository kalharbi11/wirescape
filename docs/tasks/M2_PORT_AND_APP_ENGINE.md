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
- **M2-6** starts only when M2-1, M2-2, M2-3 and M2-5 are merged. If they aren't, post a handoff on M2-6 and stop.
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

## Pi 5 rules (every task that uses it)
Reach it with `ssh pi` (user `codex`). On the Pi you may only build, run tests, run benchmarks and run valgrind/callgrind. Never open audio or MIDI devices, never use `sudo`, never install system packages, never change kernel or CPU-governor settings. Toolchains you need go under your home directory or `/tmp`. Record the compiler, CMake, kernel (`uname -r`) and the governor you observed (read-only) with every Pi number. Results go into PR or issue comments only, never into tracked files. If the Pi is unreachable, skip the Pi parts, say so in the handoff and continue.

---

## M2-0: benchmark tooling and Pi 5 realtime evidence (size/m, may start now)
Why Codex: needs the Pi and a native build.

**Deliverables:**
1. `tools/bench/compare_bench.py`. It takes one or two ABI 2 benchmark JSON files (`--base`, `--head`) and prints a Markdown table of the median ns per row plus the `mono_ratio`, `poly8_vs_eight_mono_ratio` and `modulated_ratio`, with head/base deltas. `--gate` checks mono ≤ 1.05 and poly ≤ 1.00 and exits 1 when either fails. Python 3.9+, stdlib only. Unit tests in `tools/foundation/tests/`.
2. **A paced soak tool** `wirescape_soak` (in `native/engine2/tests/` or `native/cli/`). It loads a built-in graph by name (`mono`, `poly8`, `six_essentials_shim`, `voices:<n>` = n-voice VCO→VCF→ADSR→VCA poly synth), then renders in a loop paced by `steady_clock` at the block period. This simulates an audio callback without opening any device. It writes JSON: render-time min/median/p99/p99.9/max per block, the count of blocks over 50% / 100% of the period, and the configuration. Arguments: rate, block, seconds, graph. The render thread stays allocation-free (reuse the existing hook). CTest runs a 1-second smoke at 48k/128 with **no timing assertions**.
3. **A voice budget mode:** `--budget` raises `voices:<n>` from 1 to 16 and reports the largest n whose p99 stays below 50% of the block period.
4. `docs/BENCHMARKING.md`: how to run `abi2_cost.py`, `compare_bench.py` and `wirescape_soak`, locally and on a Linux ARM64 board. No machine-specific paths or host names.
5. **After PR #22 is merged** (merge `main` in first): change `.github/workflows/ci.yml` so both `linux-arm64` and `windows-x64` print `build_reports/abi2/benchmark.json` into the job log and the step summary. Today Windows writes no summary and the numbers are only in artifacts, which some agent environments cannot download. Change only that summary step. Allowed protected path: `.github/workflows/ci.yml`, this step only.

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
- `noise`: the PRNG sequence and its advance-while-muted behaviour must match bit for bit; seed from the engine seed and channel seed so voices differ deterministically.
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

## M2-R: Claude milestone review (agent/claude, milestone-review)
Claude reviews M2-0 to M2-6 against this file and `ABI2_PLAN.md` B2–B4 acceptance, builds and runs sanitizers, and decides whether M3 (UI for the contract) can start. Before reviewing, Claude asks the user what ChatGPT should prepare.
