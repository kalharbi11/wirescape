<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Historical pre-Pi review — phases 3.5–6

**Reviewed:** 2026-11-05

**Subsequent roadmap update:** the owner made **Phase 7** the dedicated Pi **UI-only** pretest and moved real-engine integration to **Phase 8**. This is a historical static review of the cited Phase 6 revision, not live branch or runner status. PRs #4–6 were subsequently merged into their **intermediate phase bases**, not directly to `main`. Use [PLAN.md](PLAN.md) for current merge prerequisites and [PI_UI_TEST_PLAN.md](PI_UI_TEST_PLAN.md) for the actionable Phase 7 checklist.  
**Revision reviewed:** `phase/6-signature-polish` at `e0e5ef1f1fff7ed56b45d154c3f259a23bfb83f1` (stacked PR [#6](https://github.com/kalharbi11/Wirescape-UI/pull/6)).  
**Scope:** Static review of the phase plans, source, Playwright coverage, runner workflow and published CI/run state. **No Raspberry Pi or local interactive test was executed in this review.** The last reviewed head's hosted `browser` check succeeded; this is not hardware sign-off.

**Recommendation:** Preserve the implemented phase 3.5–6 design and keep the native engine (now Phase 8) out of this UI acceptance test. The browser app is a reasonable Pi-test candidate **after P0 prerequisites**; address the **P1 measurement gaps before declaring OPEN-5 passed**, and settle the owner UX questions during Pi acceptance. Items below are review notes/revision recommendations, not a claim that the existing 79 hosted browser tests failed.

## P0 — prerequisites before running the physical Pi job

### 1. Merge and test the exact revision that will run (phases 3.5–6)

- At the time of review the phase stack was open: [#3](https://github.com/kalharbi11/Wirescape-UI/pull/3) → [#4](https://github.com/kalharbi11/Wirescape-UI/pull/4) → [#5](https://github.com/kalharbi11/Wirescape-UI/pull/5) → [#6](https://github.com/kalharbi11/Wirescape-UI/pull/6). At review time `main` still held the design kit; **current follow-up**: PR #3 merged into main and the others merged into intermediate bases. Bring cumulative implementation to main before formal acceptance.
- [`.github/workflows/pi-ui.yml`](../.github/workflows/pi-ui.yml) is intentionally **manual, owner-only, and `main`-only**. It cannot produce an accepted run on the unmerged phase branch without changing that policy. Do not bypass the guard to create a premature green result.
- **Action:** Owner reviews/merges in order, resolves any retarget/conflicts, confirms hosted build/typecheck/Playwright on the **final merged SHA**, then manually dispatches the Pi workflow when the physical setup is available. Record that same exact SHA in the evidence.

### 2. Resolve runner/desktop availability before debugging UI code

- An earlier job ([Actions run 42106052630](https://github.com/kalharbi11/Wirescape-UI/actions/runs/42106052630)) was assigned to `pi-ui`, had **zero reported setup steps**, and was **cancelled**. It is not a completed Pi test and does not establish a browser regression. The current workflow is deliberately paused from auto-execution.
- **Action (owner/operator):** Check runner service/worker logs, labels `self-hosted, Linux, ARM64, pi, ui, pi5`, installed system Chromium, session access for the runner identity, and `DISPLAY` / `WAYLAND_DISPLAY` / `XDG_RUNTIME_DIR` as applicable. Verify the physical monitor and normal compositor; do not use Xvfb/headless as a substitute.
- **Pass gate:** At least one owner-dispatched, headed job reaches and completes preflight and produces evidence. If the job is queued, cancelled, or has zero steps, mark **infrastructure blocked**, not **UI failed**. Follow [PI_UI_RUNNER.md](PI_UI_RUNNER.md) and [PI_UI_TEST_PLAN.md](PI_UI_TEST_PLAN.md). Do not start the job or change the Pi runner without owner action.

## P1 — revise measurement/coverage before accepting Pi performance

### 3. Extend the sustained-performance window and add a frame-pacing gate (phase 5 / OPEN-5)

- [`tests/review-artifacts.spec.ts`](../tests/review-artifacts.spec.ts) samples just **120 rAF intervals** per condition after 30 settle frames: around 2 seconds at 60 Hz, and around 4 seconds at 30 Hz. It records `p95FrameMs` but only asserts mean `frameFps >= 30`. Short runs can miss periodic stalls, thermal/load effects and the 5-second auto-profile watchdog.
- **Recommended revision:** Keep the short hosted regression for fast CI if useful, but add a Pi-only warm-up plus **at least 15–30 seconds of stable, headed measurement per profile × zoom** (Normal/Pi × fit/100%), with actual window/screen/DPR, average FPS, p95 or p99 frame interval, long-frame count, visibility/focus and `ticker.applyMs`. Define an explicit frame-pacing threshold/acceptable exception **before** interpreting the results; passing average FPS alone is insufficient.
- The fixture forces `autoProfile: false`. Test the automatic `<40 FPS for 5 s` fallback **separately**, including one-time switching and user override; do not infer that the FPS benchmark validated it.
- **Pass gate:** Both profiles, both views and the auto-profile check have labelled results at the final SHA. A hidden/unfocused window, missing hardware evidence or an unrun condition is **not a pass**.

### 4. Add a signal-bearing functional workload alongside the current stress patch (phases 3.5 and 6)

- The current synthetic 20-module/40-cable fixture uses **10 LFOs + 10 VCFs**, with VCF outputs connected to other VCF inputs; it has **no oscillator or Output module**. It exercises many moving modulation visuals well, but a held fake-engine note does **not** by itself create an audio signal path, active output meters or a representative mixer/scope load. Source: [`tests/review-artifacts.spec.ts`](../tests/review-artifacts.spec.ts); the fake signal-level path in [`FakeEngine.ts`](../src/engine/FakeEngine.ts) starts from generator modules and propagates through connections.
- **Recommended revision:** **Retain** this worst-case modulation fixture for comparisons. Add a second fixed, realistic patch (e.g. VCO → VCF → VCA/Mixer → Output with a keyed note, an LFO modulation, active stereo metering/scope and cables). Check signal-activity dots, live value markers, meter/scope updating and frame pacing; do not describe fake levels as real DSP or audible output.
- **Pass gate:** Evidence distinguishes `modulation-heavy stress` from `signal-bearing UI session` and shows the displayed panels actually receiving changing telemetry.

### 5. Capture enough real-device context to interpret results

- [`tools/pi-ui-preflight.sh`](../tools/pi-ui-preflight.sh) records platform, model, RAM, Chromium and display variables. The screenshot test captures GPU metadata, but tolerates `gpu: unavailable`; neither path by itself verifies a useful hardware renderer. The runbook recommends temperature/throttle/load context, but the automated artifact does not yet gather it.
- **Recommended revision:** Record display resolution/scaling/refresh rate; Chromium version and selected graphics backend/renderer (mark GPU/software rendering explicitly); CPU/load, free memory, temperature and current/historical throttling **before and after** the workload using read-only available tools. Capture missing commands as `unavailable`, not zero. Do not infer an active power fault from an old throttling flag.
- Ensure `test-results/` and `build_reports/pi-ui/` survive a failing job. If the GPU/desktop is unverified, report **measurement qualification needed** rather than comparing its FPS with a hardware-accelerated baseline.

## P1 — owner acceptance checks and UX decisions on the actual display

### 6. Confirm cabling behavior before treating patching as accepted (phase 3.5)

- [`PatchCanvas.ts`](../src/canvas/PatchCanvas.ts) currently **replaces** an existing signal-input cable on ordinary drop; **Shift-drop** adds another. This is explicitly unresolved **Q-B** in [PLAN.md](PLAN.md), whose default is **sum/add** to agree with the engine. Multi-input add via keyboard modifier is also awkward in the **touch** interaction profile.
- **Action:** Owner either confirms the current replace/add distinction or approves the plan's add-by-default behavior, and records the choice in [DECISIONS.md](DECISIONS.md). If hardware touch is in scope, ensure the add behavior is accessible without a physical Shift key. Test connecting/removing/undoing multi-input and stereo edges, including save/reload.

### 7. Exercise editing and signature panels at negotiated screen scale (phases 4 and 6)

- Browser CI exercises undo/redo, clipboard, autosave/recovery, modal focus, keyboard notes, all 12 starter layouts at both mouse/touch densities, and 50/100/185% zoom. These are valuable but **not an observation on the actual 14-inch Pi monitor**.
- **Action:** At the measured resolution/scaling, manually try patching and mapping, cable selection/amount/color, module drag, typed knob/fader value, multi-select, copy/paste, undo/redo, save/recovery, panic, settings/modal focus and the auto-profile preference. Inspect side-rail ports and the phase-6 **800px sequencer**, **24-model Macro bank**, Mixer/Output meters and all starter layouts for clipping, overlap, hit targets, contrast and readable text; repeat representative actions at 50/100/185% zoom.
- **Touch scope:** Unless a real touchscreen is attached, mark `touch profile geometry checked with pointer`; do **not** claim physical touchscreen gesture validation.

## P2 — observation to keep out of the Pi UI pass/fail boundary

### 8. Step Sequencer tie behavior is UI-visible but FakeEngine is only a simulation (phase 6)

- The panel distinguishes Rest/Note/Tie and draws joining bars. In [`FakeEngine.ts`](../src/engine/FakeEngine.ts), the sequencer gate output is currently `(gate > 0 ? 1 : 0)`, so **Tie** shares an active gate value with **Note**; this does not prove musical note-sustain semantics, especially when Tie follows Rest. Existing sequencer tests emphasize edit/persistence/playhead behavior.
- **Recommendation:** If users are expected to judge sequencer *behavior* during the Pi walkthrough, add deterministic fake-engine state tests for Rest → Tie and Note → Tie (and specify the expected interpretation). Otherwise label this **fake-engine semantic limitation**, not a hardware performance blocker. Real sequencing/gate behavior and audio are Phase 7 work.

### 9. Do not expand the Pi UI gate into native-engine acceptance

- [PHASE8_HANDOFF.md](PHASE8_HANDOFF.md) already records the native v2 codec/output-routing, async acknowledgement/refusal, telemetry snapshot and CC-mapping integration gaps. These are real handoff tasks, but **not a reason to fail the current fake-engine Pi Chromium UI baseline**. No native sound, real MIDI/OSC, audio dropouts or realtime host validation is claimed.

## Suggested sign-off record

- **Source:** final merged commit SHA; URL of passing hosted checks; owner-approved PR order.
- **Infrastructure:** runner/service ready; headed physical Chromium session; OS, Pi model/RAM, Chromium build/renderer, actual resolution/DPR/refresh; whether evidence came from X11/XWayland/Wayland.
- **Functional:** Pi Playwright result; manual mouse checklist, explicit touch status, phase-6 signature panels, patch save/reload and Q-B decision.
- **Performance:** profile, zoom, workload type, warm-up/duration, average FPS, p95/p99 interval, dropped/long frames, telemetry application time; pre/post thermal/load/power context and any omissions.
- **Disposition:** `PASS`, `APP ISSUE`, `RUNNER/DISPLAY BLOCKED`, or `NOT RUN` for each stage. Close [DESIGN.md](DESIGN.md)'s **OPEN-5** and record device figures in [PERFORMANCE.md](PERFORMANCE.md) **only after** physical-run evidence is reviewed. A hosted green result is not a substitute.

**No runtime code, workflow triggers, Pi configuration or owner decisions were changed by this review.**
