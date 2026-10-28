<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Fresh-eyes review and "Jam checkpoint" proposal (2026-10-27)

For: the planner (main Claude Code). From: a separate read-only Claude session.
Scope: read `AGENTS.md`, `docs/PLAN.md`, `PLANNER_HANDOFF.md`, `ARCHITECTURE.md`, `STATUS.md`,
the M2 task file, the CHAOS / Plaits / Beta 0.8 / communication studies, open issues, all PRs,
recent diffs (#48, #63, #67) and CI on `main`. **Nothing was built or run.** `main` = `b47b107`, CI green.

---

## 1. The user's goal (new, overrides the order of PLAN.md for now)

The user wants a **pause point**: a stage where everything that exists works, so they can stop
for about a week, play and test, and build their own modules (a sequencer, a random source) with
an agent. They do **not** need distribution, long soaks, Daisy/CLAP, peak performance or polish.

Must have at the checkpoint:
1. Basic Essentials working in the real app on engine2.
2. **Polyphony and chords** working well (keyboard and MIDI).
3. **MIDI in tight**, and **OSC/UDP in** good enough for Pico 2 W / ESP32 controllers.
4. A **CHAOS** module working, ideally **two instances at once** smoothly.
5. Maybe **Plaits** (Macro Oscillator).
6. All of the above **working on the Pi 5** with real audio.
7. A **good module-authoring guide** an agent can follow for a sequencer / random source.

Proposed name: **Jam checkpoint (J)**, between M2 and Preview 0.7.

## 2. Where the project actually is (vs. that goal)

- The playable app is still the **ABI1 mono engine**. engine2 (poly, modulation cables, events,
  feedback) is offline-only until **M2-6 (#29)**, which hasn't started.
- No MIDI, OSC or UDP service exists in code. In PLAN, MIDI is Beta 0.8 (after M3), OSC/UDP is
  M5, CHAOS is M7 (needs subpatches + Channel Expander), Plaits is X-3 after M2-R.
  **Under the current plan, the goal is roughly 5 milestones away.** Reordered, it is close.
- **The Pi has never produced sound.** Every Pi result is device-free (soaks, CTest). The Qt UI has
  never been built on Linux (CI linux-arm64 is headless). PortAudio's loader already looks for
  `libportaudio.so.2`, and the UI sources have no `_WIN32` code, so both are plausible but unproven.
  Agent rules forbid devices, so the **first Pi audio run must be the user's**.
- Headroom is not a concern: a 16-voice poly graph used ~12% of a 128-frame period at p99 on the Pi.
  Two CHAOS-lite instances should fit easily; measure, don't assume.

## 3. Proposed checkpoint path (in order)

Each line is one issue / one PR. Sizes are guesses.

| # | Item | Agent | Notes |
|---|---|---|---|
| J0 | Merge #40, then #41 (main merged in) | user + Codex | #40 is integrated and green but now 2 commits behind (#63/#67 touched only soak code). #41 is `dirty`, 50 behind |
| J1 | **M2-6 (#29)** engine2 is the app engine | Codex, l | Unchanged spec. Highest priority |
| J2 | **Poly in the app**: on-screen keyboard and any note source go through the shared allocator; voice count (1–16) setting; poly output summed to stereo bus; a minimal channel-count badge on cables. No M3 mod-ring polish | Codex, m | Chords from the computer keyboard must work before MIDI |
| J3 | **Pi audio + Pi app**: build the Qt UI on the Pi (it has a Qt sysroot) and/or a headless `wirescape play <patch.json> --confirm-audio` runner; PortAudio via ALSA/PipeWire; document the setup. Codex builds and tests device-free; **the user** does the first real audio run | Codex m + user | Recommend the headless runner as the floor: design on the laptop, play on the Pi, control the Pi from MIDI/OSC. UI on the Pi is a stretch |
| J4 | **MIDI In + MIDI→CV (poly)** + **CC→CV rows**: one backend (libremidi), endpoints by name, hot-plug safe, disconnect releases all voices, sustain, pitch bend ±2 st. CC rows output CV that the user patches onto any knob with an ordinary modulation cable, so **no Mappings panel or Learn UI is needed yet** | Codex, l | Minimal C1 subset only. Follow `BETA08_MUSICAL_CORE_BRIEF.md` §1–2 and its fixtures 1, 2, 4. Answer its P0 questions first (IDs, allocator semantics, MPE postponed) |
| J5 | **OSC In (UDP)**: own bounded codec, one port per module, address rows → CV / gate / trigger outputs, float/int args, localhost default + LAN toggle, stale timeout, 127.0.0.1 loopback test in CTest. Plus a minimal **Monitor** (log mode) so the user can see what a Pico sends | Codex, m–l | C4/C5 subset: no OSC Out, no UDP Send, no bundles/time tags beyond parsing-and-ignoring safely. Fuzz corpus stays (cheap, important for anything on a LAN) |
| J6 | **Macro Oscillator (Plaits) spike X-3** per `PLAITS_STUDY.md` §8: pinned FetchContent, 48 kHz only, 12-frame FIFO, poly up to 8 | Codex, m | Also serves as the X-3 proof that outside DSP fits the ABI. MIT upstream; no "Plaits" branding |
| J7 | **CHAOS-lite** as **one** native ABI2 module (GPL, original DSP): two internal oscillators, A/B levels/pan, route modes (parallel / ring-AM / delay-bend), three-comb body, roughness AM, drift/jitter/fall, self-mod LFO, output safety stage. Poly up to 8. No tape, looper, reverb, Surge | Codex, l | **Deliberate deviation** from the X-2 study, which wants CHAOS decomposed into primitives inside an M7 subpatch. Subpatches don't exist yet, and the user wants a playable CHAOS now. Write the comb body, coupler and drift as separate reusable classes inside the module so they can become standalone modules in M7. Acceptance: two instances, 4 voices each, on the Pi at 48k/128 with headroom |
| J8 | **Module author guide v2** + templates (see §5) | Claude writes, Codex verifies by building the two templates | Must land before the pause |
| J9 | **User checkpoint session** (replaces M2-L for now): laptop 30 min + Pi 30 min, checklist in §6 | user | Each ❌ becomes an issue |

**Explicitly deferred until after the pause** (don't spend usage on them now): #64 clang-cl
comparison, M1-3c / Windows 1.05× mono gate, M3 full UI, Track C Learn / Activity panel / Mappings
panel / MIDI Out / MIDI Clock / UDP Send / OSC Out, MPE, Quantizer + Basic Sequencer (the user is
writing their own sequencer), X-4 Surge/sst primitives, long soaks, formal ABI freeze.
#56 and #66 can stay with Copilot (cheap, off the critical path).

**J2 + J3 can run in parallel with J4** (different files). J6 and J7 can run in parallel after J1.

## 4. Decisions the planner should make (or put to the user in one message)

1. **Adopt the Jam checkpoint** and reorder PLAN.md: J pulls MIDI In, a slice of OSC In, Plaits
   and CHAOS-lite ahead of M3.
2. **CHAOS-lite as one module now** (§3 J7), decomposition later. Say clearly it won't sound
   identical to the legacy Surge-based CHAOS.
3. **ABI soft freeze at the checkpoint.** The user's agent will write modules during the pause.
   No `module2.h` / SDK breaking change without a migration note; source compatibility promised
   across the pause. (PLAN says no freeze until X-3 + X-4; J6 delivers X-3, so the risk is lower.)
4. **Pi: headless runner, UI, or both?** Ask the user.
5. **Gates:** demote the Windows 1.05× mono microbenchmark from "gate" to "tracked". The Pi
   numbers show huge headroom; a 5% gate on a CI VM is noise-prone and is consuming laptop time.

## 5. Module author guide v2 (what to write)

Today's `docs/SDK_AUTHOR_GUIDE.md` is ~20 dense lines of compressed rules. It's accurate but hard
for a cheaper agent to act on. Replace it with a step-by-step guide plus two worked templates:

- **Template A — "random source"** (signal outputs): clock/trigger input with `ws::Schmitt`,
  seeded `ws::Random` per channel, sample & hold + smooth outputs, CONSTANT fast path, poly output
  sized by a parameter, state save/load of the seed.
- **Template B — "step sequencer"** (event + signal outputs): clock and reset inputs, run param,
  N steps of pitch/gate/velocity as parameters, outputs both **pitch + gate CV** and **note events**
  via `b.emit`, same-frame reset-wins rule, tie handling, never strand a note-off on stop/reset,
  pattern stored with versioned `save_state`.
- Sections: folder layout and `module.json`; IDs and why they never change; ports vs params vs
  events; units (1.0/oct, 0 = C4; gates 0/1); polyphony (who sets the channel count); CONSTANT;
  memory in `prepare` only; the realtime rules as a checklist; tests to copy (frame/rate/channel
  matrix, reset, state round-trip, no-alloc); how it shows up in the app catalogue; how to build
  and try it on the Pi; a "definition of done" checklist; common mistakes.
- Ship it as a repo skill too (e.g. `.claude/skills/new-module/SKILL.md` + an `AGENTS.md` pointer),
  so any agent asked "build a module" follows the same path.
- Verify the guide by having a cheap agent build the Ring Mod exercise or Template A from the guide
  alone, as was done for M1.

## 6. User checkpoint checklist (J9)

Laptop: build VCO→VCF→VCA→Output + ADSR from scratch; play held 4-note chords from the keyboard
and a MIDI controller; sustain pedal; unplug the controller mid-chord (no stuck notes); turn every
knob while playing; CC row → cutoff via a cable; Pico/ESP32 OSC → a knob; Monitor shows the
messages; Macro Oscillator through a few models; two CHAOS-lite instances; save/close/reopen.
Pi: same patch file loads; audio at 48k/128 through the chosen interface; MIDI over USB; OSC over
Wi-Fi; two CHAOS-lite instances for 20 minutes without dropouts; note the CPU meter.

## 7. Repo / process observations (fresh eyes)

- **Docs are stale against `main`.** PLAN "Current milestone" still says #39/#40/#41 await merge;
  STATUS says 5 of 13 Essentials are on ABI2 (now 10 merged); the "Codex continuous queue" in
  `M2_PORT_AND_APP_ENGINE.md` lists #48 and #63 as to-do, though both merged. The user's latest
  Codex prompt inherited that staleness (items 2–3 were already done; #67 already fixed the
  GetThreadTimes finding). Suggest one short "Now" section at the top of PLAN.md as the single
  source of truth, updated at each check-in.
- **Labels are stale:** #26 is `status/review` though integrated and green; #27 is `status/blocked`
  though #41 was accepted; #29/#64 are blocked only on the user merging and the laptop.
- **Process overhead is high for a solo project.** ~40 PRs in two days, about a quarter of them
  planner/docs/study PRs, plus exact-head Pi evidence (with toolchain, kernel and governor lines)
  on every PR. Suggest: Pi evidence only for PRs that change engine, DSP or build; no evidence
  re-runs after a merge-from-main that touches no compiled code; planner queue changes as an
  issue comment, not a PR.
- **#67** (Windows soak CPU clock reported as unsupported instead of using `GetThreadTimes`) is a
  reasonable fix; `QueryThreadCycleTime` could restore Windows classification later. Not urgent.
- **#48 patch v2** looks solid per its review; the known 2× size duplication (`preserved` at two
  levels) is fine for now.
- **Engineering rules are good** and worth keeping through the checkpoint: no allocation/locks in
  process, no devices in CI, stable IDs, no weakening CI.

## 8. Notes on controllers (Pico 2 W, ESP32, Arduino) for J4/J5 design

- **USB-MIDI is the most reliable path.** Pico 2 W (Arduino-Pico core + Adafruit TinyUSB, or
  CircuitPython `usb_midi`), ESP32-S2/S3 (native USB) and 32u4 boards (Leonardo/Pro Micro,
  `MIDIUSB`) appear as class-compliant MIDI devices on both Windows and the Pi. Wired, ~1 ms,
  no Wi-Fi jitter. **J4 alone covers these.**
- **OSC over Wi-Fi** suits Pico 2 W and classic ESP32 (no native USB). Expect a few ms of jitter
  and occasional loss: fine for knobs, less ideal for note timing. Use `/pico1/knob/3` addresses,
  floats 0..1, send on change with a small deadband, ≤100–200 Hz per control, and a 1 Hz heartbeat.
- **Arduino Uno** has no native USB device: use serial → a small Python bridge (`pyserial` +
  `mido`/`python-rtmidi`; virtual ports work natively on the Pi/ALSA, but on Windows need loopMIDI
  or Windows MIDI Services loopback), or a 5-pin DIN MIDI out into a USB-MIDI interface.
- High-resolution knobs over MIDI: 14-bit CC pairs or pitch bend. Make sure J4's CC rows support
  14-bit pairing (already in the C2 acceptance) or document 7-bit only for the checkpoint.
