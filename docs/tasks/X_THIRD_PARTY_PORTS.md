<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Track X: porting existing open-source instruments into Wirescape

Exploratory and low priority. Track X never blocks or delays M2. Nothing from it ships as an Essential, and no Track X code merges into `main` before M2-R and a planner review. Its job now is to learn: what porting a real, complex instrument costs, and what the SDK is missing. The answers feed the SDK before M3, when changing it is still cheap.

Follow `AGENTS.md` like any other task: one issue = one branch = one draft PR, handoff comments, and no merging, approving or force-pushing.

## Who can do what
| Phase | What | Who | Proof |
|---|---|---|---|
| A: study | Read the upstream source and our contract, write a design doc | ChatGPT (GitHub-connected chat) is ideal. No build needed | The doc, reviewed by Claude |
| B: spike | Write the module and its tests on a draft PR | An agent that can compile: Codex, the Copilot coding agent, or a ChatGPT agent with a terminal. A chat-only agent may use CI as its compiler (push, read the CI log, fix), which works but is slow | CI green on the draft PR, plus the tests below |
| C: adopt | Decide whether and how the module ships | Claude and the user, after M2-R | A planner decision recorded in `docs/PLAN.md` |

Read first, every Track X task: `AGENTS.md`, `docs/SDK_AUTHOR_GUIDE.md`, `native/sdk/template/`, `docs/specs/ABI2_PLAN.md` sections 2, 3 and 6, and one finished port as a model (`native/modules2/vcf/` for a simple one; `native/modules2/ladder/` for state and double precision).

## Rules for every port
1. **Licence first.** Record the upstream repository, the exact commit SHA and the licence file path. MIT (Mutable Instruments eurorack, stmlib, DaisySP) and GPL-3.0 (Surge XT, the sst-* libraries) are both compatible with our GPL-3.0 modules. Keep upstream copyright notices. If a licence is unclear, stop and ask on the issue.
2. **No vendored trees.** Fetch upstream sources at build time with CMake `FetchContent` pinned to a full commit SHA (not a branch or tag), the way CI pins PortAudio. Copy individual files into the repo only when the study says why, with their notices intact. Third-party sources never live under `native/modules2/` (the banned-include test scans it).
3. **Names and trademarks.** Don't use a vendor's brand as the module's name. For example, call the port "Macro Oscillator", with a README line that says it is a port of Mutable Instruments Plaits at commit X. Module IDs use a `ports.` prefix (for example `ports.macro_oscillator`), never `essentials.`.
4. **Our contract wins.** The module follows `ABI2_PLAN.md` section 6: no allocation, locks, I/O or exceptions in `process`; memory requested through `memory_requirements` and the host allocator; per-voice state for all 16 channels; state save/load; finite outputs. If upstream code needs something the SDK can't give, that's a finding. Write it down rather than working around the contract.
5. **No DSP changes in the first port.** Wrap upstream code; don't rewrite it. Behaviour must match upstream bit for bit wherever the host conditions match.
6. **Digital DSP only.** Port modules whose sound comes from published DSP code (for example Plaits, Rings, Clouds, Braids). Skip modules whose character comes from analogue circuitry (schematics aren't a DSP implementation; those would need analogue modelling, which is a separate project).
7. **Every target, from the first commit.** Upstream code often assumes x86 SIMD (Surge's sst libraries use SSE, with a SIMDe compatibility layer for other CPUs) or a single CPU family. A port must build and pass its tests on the `linux-arm64` and `windows-x64` CI jobs, and the portability probe (`.github/workflows/portability.yml`) must report it. Record any target it can't support (for example "desktop only, too large for Daisy") in its `DESIGN.md`.
8. **Measure the cost.** Report CPU per block on the Pi 5 (with `wirescape_soak` or a dedicated probe) and memory per voice × 16 channels, so we know what polyphony costs.
9. **Tests:** reference equivalence against the upstream code built directly in the test (same sources, same inputs), determinism after reset, state round-trip, zero allocations in `process`, the rate × block × channel matrix the module supports, and a clear refusal (`prepare` fails) for the conditions it doesn't support.

---

## X-1: study, Mutable Instruments Plaits (issue #51, ChatGPT, docs only)
Write `docs/design/ports/PLAITS_STUDY.md` with these sections. Mark every fact with the upstream file and line it came from, and write "to verify" where you can't confirm something.
1. **Source and licence:** repository (`pichenettes/eurorack`), the commit SHA you read, licence files for `plaits/` and `stmlib/`, and the attribution text a port must carry.
2. **What Plaits needs from a host:** the files a port needs (the `plaits/dsp/` tree and the `stmlib` parts it uses), the sample rate it assumes (find `kSampleRate` or its equivalent), its internal block size, how much working memory one voice needs (the shared buffer handed to its allocator), its inputs (patch values, modulations, trigger and level), its outputs (main and aux), and anything that's STM32-specific (fixed-point, lookup tables, `CONSTRAIN` macros, the hardware-only `ui`/`cv_scaler`/`settings` folders that a port drops).
3. **Mapping onto Wirescape:** proposed module ID, display name, port and parameter IDs with ranges and tapers, which inputs become signal ports vs parameters vs note events, how engines are selected (a stepped parameter?), how polyphony works (one upstream voice object per channel), and how memory is requested (FAST/LARGE, bytes per channel).
4. **The sample-rate question.** Our engine runs at 44.1/48/96 kHz, and Plaits is designed for one fixed rate. Compare three options and recommend one: (a) support only 48 kHz and refuse other rates in `prepare`; (b) resample around the voice; (c) compile per-rate variants. Weigh cost, quality and Daisy impact.
5. **SDK gaps:** anything the port needs that our SDK, `module2.h` or the engine doesn't provide. Candidates include fixed internal block sizes that don't divide the host block, trigger semantics, and large per-voice memory times 16 channels.
6. **Test plan** following "Rules for every port", item 6.
7. **Effort and risks:** size estimate in changed lines and files, the riskiest parts, and what a minimal first spike should include (for example two engines only).

Stop condition: the doc is in a draft PR, with a handoff and `status/review`. No code.

---

## X-2: study, CHAOS from Surge/sst components (issue #52, ChatGPT, docs only)
CHAOS (`legacy/modules/chaos/`, read-only) is a Python prototype built on Surge XT through `surgepy`. Around Surge it adds a generator-bus router, a 4-slot modulation matrix, a three-comb resonant body, tape and space effects, a varispeed looper, macros and presets. `ABI2_PLAN.md` section 3 says the rebuild must be expressible with our contract (D1–D16). This study turns that table into a concrete module list.

Write `docs/design/ports/CHAOS_DECOMPOSITION.md` with:
1. **Inventory:** every CHAOS block and macro from `README.md`, `ARCHITECTURE.md`, `PORTS.md`, `engine.py`, `chaos_dsp.py`, `macros.py`, `voices.py` and `params.json`. One row each: what it does, its inputs and outputs, and where it lives (file:line).
2. **Source of each block:** an existing Essential (name it), a planned module (ROADMAP or M6), Surge XT code, an sst library (sst-basic-blocks, sst-filters, sst-waveshapers, sst-effects; verify which exist and their licences and pin a SHA), or new code.
3. **Contract check:** the D-numbers each block relies on (for example D2 poly channels, D7 modulation, D11 LARGE memory for the looper, D16 macros and subpatches). List anything the contract can't express.
4. **Build order:** the first three primitive modules worth porting on their own because they're useful outside CHAOS too (for example a Surge-quality oscillator, a comb/resonator body, a tape/wow-flutter effect), each with a one-paragraph spec in the same shape as X-1 section 3. Note which pieces must wait for subpatches and macros (M7) and the Channel Expander.
5. **A CHAOS-lite subset** that could run on a Pi 5 within 25% of a 128-frame period at 48 kHz, as a first target.

Stop condition: the doc is in a draft PR, with a handoff and `status/review`. No code.

---

## Later (not yet issued): spikes
These get issues after X-1/X-2 are reviewed, and only when they don't compete with M2 for Codex time.

**Why they matter:** X-3 and X-4 are the **external-DSP compatibility test**. The ABI 2 header and SDK stay experimental until M2's phase B4 finishes (`ABI2_PLAN.md`), and they are **not frozen until both spikes have run**. Each spike reports: adapter size (lines of wrapper vs upstream code), any engine or SDK change it needed (the goal is none), equivalence with upstream, Pi CPU and memory per voice, and the portability-probe result. Friction found here gets fixed in the SDK before the freeze, while that's still cheap.
- **X-3 Plaits spike:** two engines, 48 kHz only (unless X-1 recommends otherwise), every rule above.
- **X-4 first Surge/sst primitive:** the top item from X-2 section 4.
- **X-5 CHAOS-lite:** after M7 subpatches and macros exist.
- **X-6 (optional, desktop) whole-Surge module:** the complete Surge XT engine inside one module, with its own voices and patches. Its internal signal paths would be hidden from the patch graph, and it's a much bigger integration (notes, automation, presets, resources, state). It's a separate desktop feature, not part of the CHAOS rebuild, and not before Beta 0.8.

**Rough effort guide** (an external estimate, not a promise): a small standalone oscillator or filter takes days to two weeks to integrate properly; one sst filter or effect, one to three weeks; Plaits, two to four weeks; a whole-Surge module, one to three months or more. Getting first sound is much faster than making a module reliable and pleasant to use.
