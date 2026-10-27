# Planner handoff: for Claude Code (planner/reviewer role)

Written 2026-10-26 by the Claude session that reviewed the original build, made the architecture decisions with the user, and wrote `docs/PLAN.md`, `docs/specs/*` and `docs/tasks/*`. Read this once at the start of your first session. After that, `AGENTS.md` and `docs/PLAN.md` are enough for routine check-ins.

## 1. Your role
You are the **planner and reviewer**, not the main builder.
- **Codex** builds the large tasks. **Copilot** takes small issues labelled `agent/copilot`. You write and maintain `docs/PLAN.md` and `docs/tasks/*`, open and shape issues, review at milestone gates, and resolve conflicts.
- **Be usage-conscious.** Read narrowly: `AGENTS.md`, `docs/PLAN.md`, the issue or PR in question, the handoff comments and `git log`. Open source files only to verify a specific claim. Never sweep `legacy/` or `docs/history/` unless the user asks.
- Batch your work. Draft all of a milestone's issues in one pass, and review once per milestone gate, not per commit.
- Don't write feature code unless the user asks. Small doc or task-file fixes are fine.

## 2. The user (how to work with them)
- A solo creator: music and sound design, interface and experience design, technically fluent. They make the decisions and do their own research. AI agents implement.
- They want **decisions explained in plain language**, including trade-offs and what each choice limits creatively or for UX. They trust you on wrapper and code-level choices.
- **They prefer one consolidated request.** "Ask for everything at once so we don't do this in many steps."
- They're cost-conscious, and every agent runs on a limited plan. Keep usage low, and prefer giving cheaper agents well-specified small tasks.
- Fully open source. They're fine with GPL, and the SDK is MIT so other people can build modules freely. The trademark is planned at public beta (`TRADEMARKS.md`).
- Target setups: Windows with a DAW alongside, Raspberry Pi 5, Raspberry Pi Pico boards sending OSC/UDP (possibly through a Python mapper), and later Daisy Seed and Eurorack hardware.
- **Future ideas** they've raised: DMX/Arduino integration, a Python script module (Grasshopper-style auto-generated ports), patch → Daisy export, VST conversion, a CHAOS rebuild, and a downloadable AI-friendly build guide.

## 3. How we got here (short history)
1. **Legacy prototype** (now `legacy/`): a Python + browser app with a Surge-based CHAOS synth through `surgepy`. Its musical ideas and the CHAOS feature set are the reference. The code isn't continued.
2. **Native foundation** (the current app, built by Codex): a C++17 headless engine, C ABI 1 modules loaded as DLLs, a Qt Quick UI, PortAudio, 13 "Essentials" modules, scripted checks (`tools/dev.py`), and a short real-device session. See `docs/ARCHITECTURE.md` and `docs/STATUS.md`.
3. **Review and decisions** (2026-10-26): ABI 1 lacked polyphony, cable modulation of parameters, event outputs, feedback, stereo, state and a memory model. Adding these after many modules exist would force rewriting every module. So we designed **ABI 2** (`docs/specs/ABI2_PLAN.md`) and a **communication layer** (`docs/specs/COMMUNICATION_MODULES_PLAN.md`), and moved to GitHub with a clean start.

## 4. Decisions and why (the reasoning the specs don't spell out)
| Decision | Why | Alternatives rejected |
|---|---|---|
| Poly = channels on the cable (1–16, VCV model) | Proven; mono is just 1 channel; SIMD across voices; maps cleanly to CLAP note IDs and per-voice modulation; Daisy gets a compile-time maximum | A voice-container (Reaktor/Bitwig Poly Grid) can be added later as a "poly subpatch"; separate mono instances waste CPU |
| Stereo = two ports with a `stereo_pair` hint | Keeps "channel = voice" unambiguous | Stereo carried as 2 channels (conflicts with polyphony) |
| Units: audio ±1, pitch 1.0/oct with 0 = C4, CV ±1 or 0..1 | No rescaling at plugin or codec boundaries; only hardware jacks scale, using the port hint; 1/oct matches Eurorack | Volts internally (VCV style) would need conversion at every digital boundary |
| Hints never block connections | Audio-rate FM and "creative abuse" are core to modular | ABI 1's kind check (`native/src/engine.cpp:250–251`) |
| Host-side non-destructive parameter modulation with an amount on the cable | Every module gets CV for free (AI-built modules too); macros become plain modulation sources, which removes CHAOS's ownership/precedence rules; maps directly to CLAP param-mod and to Daisy knob + CV jack | Hand-written CV inputs per module |
| Parameters arrive as signals with a CONSTANT flag | One reading model for authors; constant fast path skips coefficient math | Separate scalar and buffer APIs |
| Exact pitch stays an explicit port | Normalized modulation can't track 1 V/oct exactly | — |
| CLAP-style per-block event lists + event outputs | ABI 1 slices the whole graph per event with a 64-span budget and silences the block on overload (`engine.cpp:430–447`, `436`), which fails under live MIDI/OSC | Graph-wide span splitting |
| Overflow degrades (coalesce/drop with counters), never drops note-off | Live robustness. **This deliberately changes a settled ABI 1 rule** | Silence the block |
| Feedback: SCC sub-blocks (default 32) | Feedback patching is essential (ABI 1 rejects all cycles, `engine.cpp:284–285`); only the loop pays the cost | Global 1-sample processing (too slow); one-block delay (too long) |
| Host allocator with FAST/LARGE hints + declared requirements | Makes reverbs and loopers portable to Daisy SDRAM; lets an exporter budget a patch before building | Modules call `new` |
| Ingress = N SPSC rings merged in the audio thread | Several producers (UI, MIDI callbacks, network, scripts) with no locks; fixes ABI 1's single-enqueue-owner limit | A lock-based MPSC queue |
| C ABI + header-only C++ SDK; DSP is a plain class | The DSP class is the portable unit, adapters are thin (Wirescape, CLAP→VST3 via clap-wrapper, Daisy); AI authors fill in a class | Hand-written C glue (ABI 1's modules are inconsistent: the LFO lacks the checks the VCF has) |
| C++17 for DSP and SDK; host services may use C++20 | Embedded toolchains; libremidi needs C++20 | — |
| Own `ws::simd::f4` (SSE2/NEON/scalar) | Zero dependencies; the Cortex-M7 has no float SIMD; swappable later for xsimd, Highway or `std::simd` | An external library now |
| No global fast-math; FTZ/DAZ on the audio thread | Fast-math breaks the `isfinite` guards | `-ffast-math` |
| Licence: SDK/ABI/voice MIT; app and Essentials GPL-3.0; Surge-derived modules GPL | Module authors can choose; the VST3 SDK (MIT since 3.8) and CLAP (MIT) are compatible | All GPL |
| Communication: services (non-RT, Qt-free) / modules (portable) / host mappings | DMX, serial and scripting become new services plus ordinary modules, with no core change | Device code inside modules |
| MIDI: libremidi (Windows MIDI Services: multi-client, loopback, timestamps) | Wirescape and Ableton can share a port; built-in loopback for tests | RtMidi (fallback) |
| OSC: our own bounded codec | Exact bounds, no allocation, no LGPL | liblo, oscpack |
| One Monitor (Log + Table modes) + an Activity panel; CC Map with per-row Learn; Mappings with pickup/relative modes | The user wants explicit, editable routing and no automatic channel assignment; Table mode suits high-rate Pico data | Separate MIDI Monitor; auto-assign |
| Localhost bind by default, LAN toggle | Safe default; one firewall prompt | — |
| CHAOS rebuilt from modules (sst-* blocks) as a subpatch with a panel | Reusable pieces, poly/modulation-native, portable piecewise | Wrapping all of Surge as one module (a second engine inside, heavy, not Daisy-capable) |
| Script modules: Python at control rate in a worker; Faust for audio-rate | Python can't be realtime-safe; Faust code also exports to Daisy, plugins and VCV | PyPy or Numba on the audio thread |
| Windows primary + Pi 5 as the CPU target, CI on both from day one | Catch cross-platform breaks immediately. A Pi pass predicts desktop CPU headroom, but **not** Daisy (much weaker) and not Windows driver issues | Pi-first, then port |
| GitHub Issues as the board; handoffs as issue comments | Visible across branches with no merge conflicts; Copilot works from issues | A TASKS.md file; a handoffs folder |

## 5. Current state (check `git log` and the issues; this may be stale)
- M0-1 is done (report: `docs/history/MIGRATION_REPORT_2026-10-26.md`). M0-2 pushes and creates the issues M0-3, M1-R and M1-2. M0-3 sets up CI.
- **The M1-1 foundation slice already exists.** It was built before the migration: `native/include/wirescape/module2.h`, `native/sdk/`, `native/engine2/`, `native/voice/`, `native/modules2/`, `native/tests2/`. The review package is `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`.
  - Functional tests A–M pass offline on Windows, and the ABI 1 shim is bit-identical.
  - **Open gate:** the mono ABI 2 graph is about 1.89× ABI 1 time (target ≤ 1.05×). Eight poly voices cost about 0.91× eight mono graphs. ARM64 is unverified.
  - It lists experimental semantics that need review: overflow, timestamps, latency, and complex mixed feedback cycles.
- The ABI 1 app is the working product until M2. engine2 stays offline-only until M1-R says go.

## 6. The M1 review gate (your next big job: issue M1-R)
Read `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md` first, then verify selectively. **The performance miss is the main finding to diagnose.** Likely suspects: per-sample parameter-signal materialization when the value is constant, channel and broadcast copies, per-span bookkeeping, and the SDK validation cost in hot loops. Ask for a profile; don't guess.
1. **The contract** (`module2.h`, `docs/specs/ABI2_SPEC.md`):
   - every D1–D15 item is present
   - `struct_size` on every struct
   - nothing locks in a choice the plan left open
   - it compiles as C99
2. **The SDK** (`native/sdk/`):
   - the author ergonomics (compare the VCF in ABI 1 vs ABI 2, and the "new module from the guide" test)
   - validation in one place
   - no STL containers or allocation reachable from process
   - the state helpers
3. **The engine2 scheduler:**
   - SCC and back-edge determinism
   - sub-block execution
   - channel propagation
   - parameter pipeline order (smooth → add modulation → clamp → taper)
   - the constant-flag correctness
4. **Concurrency** (the highest risk):
   - SPSC ring memory ordering, and the producer registry lifetime
   - the shutdown order
   - `applied_generation` semantics
   - overflow reserves for note-offs
   - the message arena bounds

   Ask for ASan or Debug-CRT evidence.
5. **Equivalence:**
   - the ABI 1 shim is bit-identical
   - port deltas are explained (smoothing moved into the SDK is the expected one)
   - the benchmarks are within 5% on mono, and the poly ratio is reported for **both** Windows and ARM64
6. **Portability:** the no-exceptions/no-RTTI build, scalar vs SIMD equivalence, and the banned-include check.
7. **Readiness for later milestones:**
   - can `rows`/`query_ports` support CC Map and OSC rows?
   - can event outputs and egress support MIDI Out?
   - can LARGE memory and state support the CHAOS tape, loop and space modules?

Output: one review comment with findings by severity (file/line, trigger, consequence, fix), what can stay, and the go/no-go for M2. Then update `docs/PLAN.md` and draft the M2 issues.

## 7. Known watch-items and open questions
- **Windows CI dependencies:** PortAudio and Qt in CI (M0-3). The M0-1 report says how to get them.
- **Windows MIDI Services rollout issues** (dynamic virtual ports, Korg drivers): treat them as environment problems.
- **The ADSR gate:** event gate vs cable gate precedence must be documented (M1-1).
- **Patch v2:** drafted in M1, implemented in M2. Lossless v1 migration; string parameter IDs come from each `module.json`.
- **Pi 5 self-hosted runner:** only for push events on our own branches, never fork PRs (M2, optional).
- **Daisy:** an M8 build-only proof. Never claim "Daisy support" without hardware results.
- **Pending decisions from the user:**
  - the MIT copyright name (currently "Wirescape contributors")
  - when to file the trademark (plan: at beta 0.7)
  - whether to change the agent subscription setup (some GitHub agent features need higher plans)
- **History:** the full pre-migration history is in a local `.bundle` and the `wirescape_archive_local` folder on the user's laptop. The tag `baseline/0.5.2-local` exists only there.

## 8. Reference sources (already cited in the specs)
VCV Rack polyphony and voltage standards; CLAP (u-he); the VST3 3.8 MIT announcement; Windows MIDI Services GA and its known issues; libremidi; the OSC 1.0 spec; sst-basic-blocks licensing; GCC 16 `std::simd` audio benchmarks; Python 3.14 performance; VCV Prototype; Daisy memory docs. Links are in each spec's Sources section.
