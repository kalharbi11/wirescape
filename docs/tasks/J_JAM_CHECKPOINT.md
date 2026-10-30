<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J: Jam checkpoint (playable pause point)

Adopted 2026-10-28 from the fresh-eyes review. **This file overrides the M2 queue and the order of later milestones until J9 passes.** M2-R, M3 and the communication tracks resume afterwards.

**Goal:** a stage where everything that exists works, so the user can stop for about a week, play, and build their own modules (a sequencer, a random source) with an agent. Not needed: distribution, long soaks, Daisy/CLAP, peak performance, polish.

At the checkpoint:
1. The Essentials play in the real app on engine2.
2. Polyphony and chords work from the on-screen/computer keyboard and from MIDI.
3. MIDI In is tight. OSC over UDP works for Pico 2 W / ESP32 controllers.
4. CHAOS-lite works, two instances at once.
5. Macro Oscillator (Plaits engine) works.
6. All of the above runs on the Pi 5 with real audio (headless runner at minimum).
7. A module-authoring guide an agent can follow (done by Claude: `docs/SDK_AUTHOR_GUIDE.md` + `native/sdk/examples/`).

## Planner decisions (2026-10-28)
1. **Order:** J pulls MIDI In, an OSC In subset, Plaits and CHAOS-lite ahead of M3. M3's full UI, Track C's Learn/Mappings/MIDI Out/Clock/UDP Send/OSC Out, MPE, Quantizer and Basic Sequencer wait until after J9.
2. **CHAOS-lite is one native module now** (J7). Decomposition into primitives stays the M7 plan. It is original DSP and **will not sound identical** to the legacy Surge-based CHAOS.
3. **ABI soft freeze from J-R2 onwards.** No breaking change to `module2.h` or `native/sdk/` without a migration note in the PR and in `docs/SDK_AUTHOR_GUIDE.md`. Additive changes are fine. Source compatibility for modules written during the pause is promised.
4. **Pi:** the headless runner (`wirescape play`) is required. The Qt UI on the Pi is a time-boxed stretch inside J3.
5. **Gates:** the Windows 1.05× mono microbenchmark is now **tracked, not a gate**. #64 is deferred.
6. **MIDI decisions** (answers to `docs/design/BETA08_MUSICAL_CORE_BRIEF.md` §6, P0):
   - IDs, final once merged: `midi.in`, `midi.to_cv`, `midi.cc_to_cv`. Port IDs as in the brief §1–2. The J2 keyboard source is `host.keyboard`.
   - Minimum C1 subset: one backend (libremidi; RtMidi only if libremidi fails twice), endpoints identified by saved name with system-name fallback, explicit enable, hot-plug safe, a fake backend for tests, counters, disconnect releases all voices. No Learn, Mappings panel, MIDI Out, MIDI Clock.
   - Allocator semantics: `native/voice/` is the oracle. Repeated Note On of a held key: FIFO (the oldest activation releases first). Stealing emits NOTE_CHOKE before the replacement NOTE_ON at the same offset.
   - MPE postponed. RPN 0 is ignored (counted in the monitor counters); the bend range is the manual parameter, default ±2 st.
   - The sequencer question is moot: the user writes their own sequencer (Template B in `native/sdk/examples/step_sequencer/` is the starting point).
   - CC rows: 7-bit by default, plus a per-row 14-bit toggle (MSB CC 0–31 paired with LSB CC+32).
7. **Process, lighter for J:** see "Evidence" below. Planner queue changes are issue comments, not PRs.

## Read first (every J task)
1. `AGENTS.md` and this file (only your section plus "Rules", "Evidence" and "Queue").
2. Your issue and its latest handoff comment.
3. `docs/SDK_AUTHOR_GUIDE.md` for any module work.
4. Files your section names. Don't read whole folders.

## Rules (every J task)
- Engineering rules from `AGENTS.md` stand: no allocation/locks/I/O/logging in process, event or reset; no devices in CI or agent runs; stable IDs; never weaken CI.
- **Devices:** agents never open audio or MIDI devices or LAN sockets. Localhost UDP in tests is fine. Every real-device path is behind an explicit `--confirm-audio` / `--confirm-midi` / `--confirm-lan` flag that only the user runs.
- **Services stay out of DSP.** MIDI, OSC and audio backends live under `native/io/` (Qt-free, so the headless runner reuses them). Modules only see ABI 2 events/signals.
- **New third-party code** is pinned (FetchContent with an exact SHA), never vendored without a reason, and recorded with licence in the module's `DESIGN.md` or `native/io/<service>/DESIGN.md`.
- **ABI headers:** `native/include/wirescape/*.h` stays untouched. If a J task genuinely needs an ABI change, stop that item with `status/blocked` and continue the queue.
- **Size:** each section gives a budget. Past 1.5× the budget, post a handoff asking whether to split, then continue with the next item while waiting.

## Evidence (workflow v2, 2026-10-28)
- Agents run in the cloud and can't reach the Pi or the laptop. **No SSH.** Pi numbers come from the Pi runner (`docs/WORKFLOW.md` §2): `pi:` lines in each task's acceptance block run there. Until the runner exists (#84), `pi:` lines are marked "pending Pi runner" and don't block review.
- CI green on the final head is required (Linux ARM64, Windows, sanitizers, guard).
- No evidence re-runs after a merge from `main` that touches no compiled code.
- Each J item now has its own task file with an acceptance block: `J2_POLY_APP.md` … `J8_AUTHOR_GUIDE.md`. J1 stays in `M2_PORT_AND_APP_ENGINE.md` § M2-6 plus the decisions below.

## Planner decisions 2026-10-28b (unblocking the open PRs)
1. **J1 (#29, PR #78) render delta.** The app-path render of Six_Essentials_Demo differs from the ABI 1 graph by at most 3.7e-9 (about −169 dBFS). Accepted for the **app integration** comparison only, with a bound of max abs error ≤ 1e-7 in `app_patch_tests`, on two conditions: (a) the PR states the cause in one paragraph (e.g. summation order or float/double at a host stage) with the line that introduces it; (b) per-module ABI 1 ↔ ABI 2 equivalence tests stay exact (0). This is a planner-authorized threshold, not a loosening by the builder.
2. **J6 (#73, PR #77) upstream signed-shift UB:** a bit-preserving patch applied at fetch time is authorized (details in `J6_MACRO_OSC.md`). All 24 models stay.
3. **J4 (#71, PR #81) `retrigger`:** J ships `on_new_note` only; `off` is post-J9 (details in `J4_MIDI.md`).
4. **Windows red on #77, #81, #82:** build logs are readable through the GitHub REST API; #83 makes them readable from Codex cloud. Fix them in each PR; they're not planner decisions.

## Queue
Two Codex runs. Run A is the critical path. Run B is independent module work and may run as a second worker in parallel (file ownership below), or after Run A in the same session. **Never wait for a review: open the PR, set `status/review`, post the handoff, move on.** If an item depends on an unmerged PR, branch from that PR's branch ("stacked"), say "stacked on #N" at the top of the PR, and merge `main` into the stack after each merge (merge commits only).

| Order | Item | Issue | Size | Branches from | Run |
|---|---|---|---|---|---|
| A0 | Merge upkeep: after the user merges #40, merge `main` into #41; apply SPDX headers per the provenance answer on #29 | #27, #29 | s | — | A |
| A1 | **J1 = M2-6** engine2 is the app engine | #29 | l | main (after #40) | A |
| A2 | **J2** poly in the app (`host.keyboard`, voice count, poly badge) | #69 | m | J1 | A |
| A3 | **J3** Pi audio: headless `wirescape play` + Linux audio + Pi UI stretch | #70 | m | J2 | A |
| B1 | **J7** CHAOS-lite module | #74 | l | main | B |
| B2 | **J6** Macro Oscillator spike (X-3) | #73 | m | main | B |
| B3 | **J4** MIDI In + MIDI→CV + CC→CV | #71 | l | J2 (core can start from main) | B |
| B4 | **J5** OSC In + Monitor (log) | #72 | m–l | J4 (shares `native/io/` + ingress plumbing) | B |

**File ownership when both runs are active:** Run A owns `native/ui/`, `native/engine2/engine2.*`, `native/control/`, `native/document/`, `native/cli/`, `tools/dev.py`, `docs/ARCHITECTURE.md`. Run B owns `native/modules2/chaos_lite/`, `native/modules2/macro_oscillator/`, `native/modules2/midi_*`, `native/modules2/osc_in/`, `native/modules2/monitor/`, `native/io/`. Shared files (`native/engine2/CMakeLists.txt`, the module catalogue list) get one-line additions only; the later branch merges `main` and keeps both. Run B needs a Run A file changed → ask in the issue, or make it after Run A's PR is up, on a branch stacked on it.

**Hard stops per item** (post `status/blocked`, then take the next item): ABI header edit needed; non-zero equivalence delta in J1 without a listed reason; test removal or tolerance change; two failed attempts at the same failure.

**Review points (Claude):** only two.
- **J-R1** when A1–A3 are in `status/review` (plus whatever of Run B is ready).
- **J-R2** when everything is in `status/review`. After it, the user merges in order and runs J9.

---

## A0: merge upkeep (size/s)
After #40 merges: merge `main` into `task/27-port-regressions` (#41) with a merge commit, keep both test registrations, run tests, push, CI green. Merge-only evidence rule applies unless compiled code conflicted. Add `SPDX-License-Identifier: GPL-3.0-only` headers where missing and replace "Provenance: to be confirmed by the user" in the DESIGN.md of mixer, output, scale_offset, ADSR, LFO, VCA and VCO with "Original Wirescape code, GPL-3.0-only, no upstream source" (the user's answer on #29). Do the provenance edit in #41 or as the first commit of J1, whichever is open first.

## A1 / J1: engine2 becomes the app engine (#29, size/l)
Spec unchanged: `docs/tasks/M2_PORT_AND_APP_ENGINE.md`, section "M2-6". #32 no longer blocks it. Additions for J:
- Keep the ABI 1 host-note behaviour for v1 patches (keyboard notes reach VCO/ADSR as today, mono) so old patches still play. J2 adds the poly path.
- The module catalogue must list ABI 2 modules from a single registration point that later modules add one line to (J4–J7 depend on it). Document it in `docs/ARCHITECTURE.md` and in one sentence in `docs/SDK_AUTHOR_GUIDE.md` §11 (allowed for this item only).
- Pi: the 10-minute soak in the M2-6 spec stays (engine change).

## A2 / J2: polyphony in the app (#69, size/m, ~500–800 lines)
Read first: `native/voice/allocator.h`, `native/tests2/test_modules.h` (`NotesToCv`), `native/ui/studio_controller.*` (`note()`), `docs/design/BETA08_MUSICAL_CORE_BRIEF.md` §2 (behaviour of MIDI→CV, which J4 implements).
1. **Shared note core:** factor the event→voice logic into one reusable header (for example `native/voice/note_to_cv.h`, MIT) used by `host.keyboard` now and `midi.to_cv` in J4: shared allocator, NOTE_CHOKE before replacement NOTE_ON, FIFO for repeated keys, sustain hold, release-all, pitch 1.0/oct with 0 = C4, gate, velocity, `notes` event output with the assigned voice channel.
2. **`host.keyboard` module** (ABI 2, GPL): outputs `pitch`, `gate`, `velocity` (poly signals) and `notes` (events). Params: `voices` 1–16 (default 8, STEPPED), `mode` rotate/reuse/reset/mono (STEPPED), `octave` −3..+3. It receives notes from the on-screen keyboard and the computer keyboard through the D13 ingress path, with no lock on the audio thread. One instance per patch receives host keys (the first; document it).
3. **App:** a new patch starts with Keyboard → VCO pitch, Keyboard gate → ADSR. The computer keyboard plays chords (several keys at once, no key-repeat retriggers). Poly cables sum to the stereo output through the Output module's SUM policy. Cables show a small channel-count badge when channels > 1. Nothing else from M3.
4. Panic (existing control or a new one) chokes all voices.
**Tests:** shared-core unit tests (CV-01, CV-04, CV-05, CV-07, CV-09 from the brief, adapted to keyboard input); a graph test Keyboard → VCO → ADSR → VCA → Output with a 4-note chord, voice count 1/4/16, no stuck note after release-all; UI offscreen test that a chord produces 4 active channels; no allocation in process.
**Evidence:** Pi (engine/DSP change).

## A3 / J3: Pi audio and the headless runner (#70, size/m, ~400–700 lines)
1. **`wirescape play <patch.json>`** (CLI, Qt-free): loads a v1/v2 patch through the document loader, runs engine2 on a real audio device **only with `--confirm-audio`**. Options: `--device <name|index>`, `--rate 48000`, `--block 128`, `--list-devices` (enumeration only, allowed without the flag), `--midi <name>` and `--osc-port <n>` once J4/J5 land (leave clean hooks). Prints CPU load and xrun count once per second to stderr; Ctrl+C stops cleanly (stop producers → drain → stop render → destroy).
2. **Linux audio:** PortAudio via ALSA (PipeWire/Pulse through their ALSA plugins is fine). Load `libportaudio.so.2` as the Windows path loads the DLL. Document `apt` packages the user installs (the agent does not run sudo).
3. **Without `--confirm-audio`**, `play` renders 10 s offline to a WAV under `build/` and prints peak/RMS, so CI and agents can test it.
4. **Stretch (time-box 2 hours of attempts):** build the Qt UI on the Pi with the existing Qt sysroot and run its offscreen tests there. If it fails, record why in the handoff; it doesn't block J3.
5. **Docs:** `docs/PI_SETUP.md` (new): packages, building headless, the exact commands the user runs for the first audio test (`--list-devices`, then `play --confirm-audio`), how to pick a USB interface, how to read the xrun line, and a troubleshooting list (device busy, wrong rate, permissions/`audio` group).
**Tests:** offline `play` in CTest on both platforms; device enumeration code compiled but not run in CI; clean shutdown under ASan.
**Evidence:** Pi build + `play` offline render + 10-minute soak of the Six Essentials demo through `play`'s loader path (no device).

## B1 / J7: CHAOS-lite (#74, size/l, ~900–1,400 lines)
Read first: `docs/design/ports/CHAOS_DECOMPOSITION.md` §1 (block inventory, behaviour reference only) and §5 (Pi budget), `docs/SDK_AUTHOR_GUIDE.md`. Don't read `legacy/` source except the inventory's cited parameter list if a range is unclear.
- One ABI 2 module `essentials.chaos_lite`, display **CHAOS-lite**, GPL-3.0, **original DSP** (no Surge code).
- Voice: two internal oscillators (A, B: saw/square/sine morph each, detune, octave), A/B level and pan; **route** mode STEPPED: parallel / ring-AM / delay-bend (B modulates a short delay on A); three-comb **body** (tuned to pitch ratios, feedback, damping, mix); **roughness** AM (noise-modulated amplitude); **drift / jitter / fall** (slow seeded instability, per-block pitch jitter, pitch fall on gate); a **self-mod LFO** (rate, depth, target A pitch / body tune / cutoff of a simple post filter); output **safety stage** (DC block, soft clip, hard ceiling).
- Inputs: `pitch`, `gate`, `velocity` (poly), `fm` (CV_BI). Outputs: `left`, `right` (stereo pair). Poly up to 8 (`prepare` sizes for `min(max_channels, 8)`; channels beyond 8 output silence and that is documented).
- Write the comb body, the A/B coupler and the drift generator as **separate classes in their own headers inside the module folder**, so M7 can lift them into standalone modules.
- Seeded randomness from `config.seed` + channel; deterministic reset; versioned state (drift phases and seeds, not DSP buffers).
- Comb buffers in `prepare` only (LARGE hint), sized for the lowest supported pitch at the rate; declare `memory_requirements`.
- Sensible default patch sound (it should sound like an instrument on first load). Provide 4 named starting points in `README.md` as parameter tables (no preset system needed).
**Tests:** frame/rate/channel matrix, reset determinism, state round-trip and unknown version, finite at extreme params (max feedback, max drift), no allocation, stereo pair metadata, CONSTANT never set on running audio.
**Acceptance:** two instances × 4 voices in one graph at 48k/128 on the Pi: post median/p95/worst block µs from `wirescape_soak` (or an offline probe) — the worst block must stay under 50% of the period (1,333 µs). If it doesn't, reduce cost (cheaper interpolation, fewer comb taps) before asking.

## B2 / J6: Macro Oscillator spike, X-3 (#73, size/m, ~500–1,000 lines)
Spec: `docs/design/ports/PLAITS_STUDY.md` §3 (mapping), §8 (smallest spike) and the "Decisions" section. Module `ports.macro_oscillator`, display **Macro Oscillator** (no "Plaits" or Mutable branding in the UI; attribution in `DESIGN.md`/`README.md`). Pinned FetchContent of the upstream revision named in the study, no DSP edits. 48 kHz only (prepare refuses other rates; README says so). Per-channel 12-frame FIFO with `latency_frames`. **Poly up to 8** (study said 1 then 16; for J, 8). All 24 models selectable; tests exercise at least models 0, 8, 10 and 13 against the upstream sources directly. State = patch state + deterministic reset (study decision 2). `UserData`/flash and global-RNG findings go in `DESIGN.md`; exclude an engine only if it can't be made instance-safe, and list it.
**Acceptance:** as the study's §6 plus: 8 voices at 48k/128 on the Pi with worst block under 25% of the period. This is also the X-3 proof that outside DSP fits ABI 2 unchanged.

## B3 / J4: MIDI In, MIDI→CV and CC→CV (#71, size/l, ~1,200–1,800 lines)
Spec: `docs/design/BETA08_MUSICAL_CORE_BRIEF.md` §1 (MIDI In) and §2 (MIDI→CV) with the decisions above, and `docs/specs/COMMUNICATION_MODULES_PLAN.md` §3.5–3.6 (bounds, libraries). Ingress through ABI2 D13 (`docs/specs/ABI2_PLAN.md`).
1. **Service** `native/io/midi/`: `midi_backend.h` interface, libremidi implementation, fake backend for tests. Endpoint registry by saved name; explicit enable list; hot-plug: a vanished endpoint marks offline and emits release for its notes, a returning one rebinds by name. One open per endpoint however many modules use it. Bounded SPSC ingress ring per callback context; counters (received, dropped, malformed).
2. **`midi.in`**: brief §1 (endpoint config, channel filter, `midi` event output). IN-01 to IN-07 as fake-backend tests.
3. **`midi.to_cv`**: brief §2 using J2's shared note core (stacked on J2 if not merged). Parameters as the brief, MPE excluded. CV-01 to CV-12 as tests.
4. **`midi.cc_to_cv`**: 8 rows; each row: channel (omni/1–16), CC number, 14-bit toggle, output range (0..1 or −1..1), smoothing ms (default 5). Outputs `cc1`…`cc8` (signals). The user patches a row onto any knob with an ordinary modulation cable; no Learn or Mappings UI. Tests: 7-bit and 14-bit values, MSB-then-LSB ordering, channel filter, smoothing reaches target.
5. **App + runner:** a MIDI settings dialog listing endpoints with enable checkboxes (minimal); `wirescape play --midi <name>` (with `--confirm-midi`). Fixtures 1, 2 and 4 from brief §5 as offline graph tests.
**Never** open a real MIDI port in tests. Real-port code is reachable only with `--confirm-midi` in the CLI or by the user enabling an endpoint in the app.
**Evidence:** Pi (build + tests). The user does the first real controller test in J9.

## B4 / J5: OSC In (UDP) and Monitor (#72, size/m–l, ~900–1,300 lines)
Spec: `docs/specs/COMMUNICATION_MODULES_PLAN.md` §4.4 (OSC In) and §4.7 (Monitor), cut down as follows.
1. **Codec** `native/io/osc/`: own bounded OSC 1.0 parser (types i, f, s, b, T, F, N, I; arrays rejected safely), bundles parsed and their messages applied immediately (time tags ignored), address glob matching (`*`, `?`, `[]`, `{}`). Fuzz-style corpus test with truncated/garbage packets; no allocation on receive.
2. **UDP service** `native/io/net/`: one network thread, nonblocking socket, poll 10 ms, one socket per port shared by all OSC In modules on that port. Bind **127.0.0.1 by default**; LAN only when the module's `lan` toggle is on **and** the app/runner was started with `--confirm-lan` (app: a confirmation dialog the first time). Tests bind localhost only.
3. **`osc.in`**: config: port (default 9000), lan toggle. 8 rows; each: address pattern, argument index, kind cv/gate/trigger, input range → output range, smoothing ms, timeout ms (stale → hold or zero, per row). Wildcard rows (`/pico1/knob/*`) output a poly signal, one channel per matched index, sorted numerically. Outputs `row1`…`row8` (signals) and `messages` (events).
4. **`monitor`** (log mode only): one event input (fan-in) and 2 signal inputs; a fixed SPSC tap ring (1,024 × 128-byte entries) to the UI; formatting only in the UI; per-key rate limit 30 lines/s; pause/clear; drop counter. The runner prints the same log to stderr with `--monitor`.
5. `docs/CONTROLLERS.md` (new, short): Pico 2 W / ESP32 sending OSC (address scheme `/pico1/knob/3`, floats 0..1, send on change with a deadband, ≤200 Hz per control, 1 Hz heartbeat), and USB-MIDI controllers (Pico TinyUSB, ESP32-S2/S3, 32u4) which J4 covers. Include a 20-line Python OSC sender for testing from the laptop.
**Tests:** codec unit + corpus; a localhost loopback test sending to 127.0.0.1 and reading rows through a graph; wildcard→poly; stale timeout; LAN refused without the confirm flag.

---

## J8: module author guide v2 — done by Claude (#68)
`docs/SDK_AUTHOR_GUIDE.md` (rewritten), templates in `native/sdk/examples/random_source/` and `native/sdk/examples/step_sequencer/` with CTest coverage, and the `.claude/skills/new-module/` skill. Verified by a fresh agent building a module from the guide alone. Codex: nothing to do, except keep the guide's §11 catalogue sentence current in J1.

## J9: user checkpoint session (#45, the user)
The checklist lives on #45. Each ❌ becomes an issue. J9 replaces M2-L for now.
