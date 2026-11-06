# Phase 7 — Raspberry Pi UI pretest runbook

**Purpose:** certify and fix UI/browser defects from Phases 3.5–6 **before** Phase 8 native-engine integration. Dedicated `pi-ui` runner + real Raspberry Pi monitor + **headed system Chromium**; `FakeEngine` supplies simulated values. No native bridge, DSP/audio, real MIDI/OSC, or user device access is necessary.

**Status:** plan and runner configuration exist, but **no completed physical Pi acceptance result is claimed**. See [PLAN.md](PLAN.md), [PI_UI_RUNNER.md](PI_UI_RUNNER.md) and the [pre-Pi review](PRE_PI_READINESS_REVIEW.md). This runbook replaces the former pre-Phase-7 plan.

## A. Before the first accepted hardware run

- [ ] **Merge consistency:** PR #3 was merged into `main`. PRs #4–6 were merged into **intermediate phase branches**, so first bring the cumulative through-Phase-6 code to `main`. Also merge Phase 7's test-harness updates. Verify hosted build/typecheck/Playwright green for the **exact final main SHA**.
- [ ] **Runner connected:** `pi-ui` online, labels `self-hosted`, `Linux`, `ARM64`, `pi`, `ui`, `pi5`; system Chromium installed. The prior [run 42106052630](https://github.com/kalharbi11/Wirescape-UI/actions/runs/42106052630) was cancelled after assignment with **zero setup steps**; inspect runner service/worker diagnostics if that repeats. It is **not** an application test failure.
- [ ] **Real desktop available:** owner connects monitor, logs in, confirms actual resolution/scale/refresh; runner user inherits `DISPLAY` (X11/XWayland) or `WAYLAND_DISPLAY` + `XDG_RUNTIME_DIR` (Wayland). Do not substitute Xvfb, grant broad display access or call a headless run hardware-verified.
- [ ] **Avoid contention:** do not run native repo builds/soaks on the same Pi during baseline; separate repo concurrency groups do not serialize work across repositories. No agent needs to SSH, start a native private bridge, change autologin or touch real audio/MIDI devices.
- [ ] **Evidence:** ensure runner has permission for Actions uploads, can create `test-results/` and `build_reports/pi-ui/`, and failed jobs retain artifacts/traces.

Owner's previously recorded target: Pi 5B rev1.0 / BCM2712 / 8 GB RAM / MicroSD, externally powered 14-inch 16:9 “2K” display. **Measure** pixel dimensions and DPR; do not guess from the marketing description.

## B. Phase 7 test-harness revisions (from 3.5–6 audit)

The following instrumentation is now prepared on the Phase 7 branch. Hosted checks verify the harness; actual hardware measurements and owner acceptance remain NOT RUN.

- [x] **Long-duration timing instrumentation:** `tests/review-artifacts.spec.ts` retains 120-frame hosted samples. Pi conditions use a recorded2-second warm-up and **15 seconds of sample time per condition**, with average rAF FPS, p95/p99 interval, long-frame counts (including >33.3ms and >50ms diagnostics), focus/visibility, telemetry apply cost and exact sample duration. Keep short hosted samples for fast CI if desired. Adjust the existing Pi test's 180-second timeout and runner job timeout if necessary.
- [x] **Realistic signal instrumentation:** retain the 20-module/40-cable **10 LFO + 10 VCF** modulation-heavy fixture; it contains **no oscillator/Output** and cannot verify Mixer/Output scope or meter activity. Add a second deterministic **signal-bearing UI fixture** such as the built-in Two-source drone (VCO + Noise → Mixer → Ladder → Output, LFO cutoff modulation, FakeEngine held note) and assert meter/scope/signal-activity visual updates, without claiming audio.
- [x] **Profile behavior instrumentation:** Normal/Pi × fit/100% at the real monitor's viewport, plus a **separate** 5-second under-40-FPS automatic Pi-profile watchdog test, its one-shot behavior and user opt-out. Stress setup currently forces `autoProfile: false`; it does not test this automatically.
- [x] **Environment capture instrumentation:** log system Chromium version, renderer/backend (including software vs GPU if evident), OS/model/RAM, viewport/screen/DPR/refresh, current focus and page visibility; gather read-only load/free memory/disk/temperature/throttling pre/post when tools exist. Missing values are “unavailable,” never 0. Historical throttle flags alone do not imply an active power failure.
- [x] **Predetermined acceptance criteria:** physical target ≥30 measured UI FPS across the defined profiles/fixtures; inspect p95/p99 and long-frame/stutter rates, not average alone. Define a documented acceptable pacing threshold **before** final sign-off. The Pi profile's 30Hz telemetry/drawing cap is not the same quantity as displayed rAF FPS.

## C. Execute on the dedicated Pi UI runner

1. Merge accepted Phase 7 test changes to `main`. Owner goes to **Actions → Pi UI checks → Run workflow** and selects `main`. The workflow is deliberately **manual, owner-only, main-only**; keep it that way unless the owner requests a separately reviewed change. Do not run or restart the hardware service from this planning task.
2. Verify preflight actually executed on ARM64, identified system Chromium, captured the exact checkout SHA and connected to the existing headed desktop. If assigned but no steps run, diagnose runner/service first.
3. Execute all existing Playwright UI suites: 21 catalogue modules, 50/100/185% patching, cable/parameter modulation gestures, undo/redo/copy/paste/typed values, autosave/reload/unknown-field preservation, keyboard/octave/modal focus, fake audio/MIDI/OSC faults and panic, 12 starter layouts at mouse and touch-profile density.
4. Capture headed screenshots and short video of Step Sequencer, Macro Oscillator, Mixer and Output. Inspect visible module/port geometry and console/page errors.
5. Run sustained stress and signal-bearing UI samples at Normal/Pi and fit/100%; verify focus/visibility and exact fixture counts. Report rAF FPS, interval distributions, telemetry application cost, frame stalls, renderer/backend and hardware load/temperature context.
6. Review failures independently: headless fallback/desktop or renderer missing, wrong revision, service stall or runner offline are **environment blocked**; product crashes/clipped cards/slow frames on valid display are **UI issues**.

## D. Manual owner screen check (not replaceable by CI alone)

- [ ] Create a voice by hand and move/add/remove modules; plug/unplug and remap cables, select cables, edit colour and modulation amount; try stereo connections.
- [ ] Edit hero/trim knobs, numbers, faders and typed values; check precision keys and readout clarity; test undo/redo to empty/back, copy/paste, save/open and recovery.
- [ ] Check keyboard note-off after octave switch; background shortcuts while modal open; Tab focus trapping/restoration; panic and device lost/recovery **simulations**.
- [ ] At the actual display scale, inspect all 12 starters, Sequencer's wide grid and Tie bars, Macro's 24-model bank, Mixer A/B meters, Output L/R meter/scope, rail ports and labels. Repeat representative actions at 50/100/185% zoom and both mouse/touch **layout profiles**.
- [ ] **Q-B:** normal signal-drop **replaces** and Shift-drop **adds** today; PLAN default proposes sum/add. Owner chooses intended behavior; provide a usable touch alternative if multiple input cables are needed without Shift. Regress save/load, undo and stereo/multiple connections after any UI change.
- [ ] “Touch profile geometry tested” is not a claim that actual multitouch/pinch worked. Only claim physical touch after testing on a real touchscreen.
- [ ] Rest/Note/Tie UI should be visually correct; FakeEngine currently treats Tie as gate-on in a simple simulation. If meaningful musical tie behavior is required in this walkthrough, add explicit fake-state tests or label as an engine semantic limitation; real behavior belongs to Phase 8.

## E. Record evidence and decide Phase 7 completion

Download `pi-ui-<SHA>` with preflight, Playwright JSON/frame report, GPU/display evidence, screenshots/recordings and failure traces. Document in [PERFORMANCE.md](PERFORMANCE.md): exact tested SHA, Pi model/OS, system Chromium and renderer, resolution/DPR/refresh, Normal/Pi × fit/100%, **both** workload definitions, duration, mean FPS/p95/p99/stalls, telemetry apply cost, focus/visibility, read-only thermal/load context and Actions run URL. Log design/UX choices in [DECISIONS.md](DECISIONS.md) and [DESIGN.md](DESIGN.md).

Classify every stage as **PASS**, **UI ISSUE**, **RUNNER/DISPLAY BLOCKED**, or **NOT RUN / PHASE 8 DEPENDENCY**; never count missing evidence as green. Fix UI-only defects on Phase 7 with regression tests, then rerun hosted + physical acceptance on a final merged revision. **OPEN-5 remains open** until genuine physical results are reviewed.

**Phase boundary:** a passing Phase 7 result certifies only the browser/Pi interface and FakeEngine visual path. The native v2 codec, async engine refusals, render-safe real telemetry, real audio/MIDI/OSC, DSP parity, audio dropouts/latency, native CPU contention, packaging and migration are Phase 8 ([native handoff](PHASE8_HANDOFF.md)).

## Preparation review before hardware connection

The split is accepted: PR7 integrates the already-reviewed cumulative app; PR8
adds the UI-only acceptance harness. Merge7 first, retarget8 to main, confirm its
final hosted checks, then merge8 and confirm main again before manual dispatch.
Do not duplicate the implementation commits or merge old-look PR1.

Initial pacing gates, fixed before hardware collection: mean rAF FPS ≥30,
p95 ≤50ms, p99 ≤100ms, and no frame interval >250ms for every workload/profile/view.
Report >33.3/>50/>100ms counts as diagnostics as well. A failed gate requires
investigation; do not loosen it after seeing measurements without documenting
and reviewing the reason. These are UI pacing gates, not audio realtime deadlines.

Each Pi condition uses2s warm-up then15s sampling (8 conditions, at least136s
plus functional setup). Hosted sampling stays short. The watchdog opt-out and
one-transition behavior are tested independently using simulated slow frames.
Read-only health snapshots are captured before and after the session; missing
GPU/refresh/thermal data needs explicit qualification and owner desktop checks.

Q-B remains an owner acceptance choice; this preparation changes no cable behavior.
Pi is offline during commuting; no dispatch or service change is part of this work.
