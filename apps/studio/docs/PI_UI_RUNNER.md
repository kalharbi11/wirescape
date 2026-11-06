<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Phase 7 — dedicated Raspberry Pi UI pretest runner

**Scope:** Headed Chromium/FakeEngine UI-only checks before Phase 8 native integration. This is the UI repository's dedicated runner, not the main `wirescape` native engine runner. No real sound/DSP/MIDI/OSC hardware validation is claimed.

Runner: `pi-ui`. Required labels: `self-hosted`, `Linux`, `ARM64`, `pi`, `ui`, `pi5`.
The `pi-ui` job in Pi UI checks runs the browser suite with system Chromium **headed**,
then records the 20-module/40-cable fixture at fit and 100% in Normal and Pi profiles.
No audio engine, real MIDI/OSC, SSH, sudo or system-package installation is involved.

There are deliberately no automatic PR/push hardware triggers. Once the cumulative Phases 3.5–6 implementation **and Phase 7 test-harness updates are on `main`**, and the owner has connected the display, use Actions → Pi UI checks → Run workflow as the repository owner on `main`. Other actors/refs are skipped. Already-dispatched runs must be
cancelled separately in Actions; changing triggers does not cancel them.
The job uses read-only repository permissions and does not persist checkout credentials.

## Desktop prerequisites

Run the runner as a user with access to a logged-in graphical session. Its service
must inherit `DISPLAY` for X11/XWayland, or `WAYLAND_DISPLAY` plus `XDG_RUNTIME_DIR`
for Wayland. Repository variables `PI_UI_DISPLAY`, `PI_UI_WAYLAND_DISPLAY` and
`PI_UI_XDG_RUNTIME_DIR` may select that existing session; they do not grant access.
Pure Wayland selects Chromium's Wayland backend. Install Chromium beforehand or
set `WIRESCAPE_CHROMIUM` in the runner environment. Node 22 is set up by Actions;
locked npm dependencies and Playwright's small ffmpeg helper are installed per job.

Do not substitute Xvfb for this hardware-display measurement. Leave the desktop
available during the run: switching focus or minimizing the browser invalidates
its performance evidence. A missing/unusable desktop fails rather than falling
back to headless. Hosted tests retain the 1920×1080 baseline. Pi tests use an un-emulated, maximized
browser window and record the actual desktop screen, viewport and DPR.

Owner hardware: Raspberry Pi 5B rev1.0, BCM2712, 8GB RAM, MicroSD; a 14-inch,
16:9 externally powered monitor described as 2K. The exact negotiated resolution
and scaling are measured at run time rather than inferred from the marketing name.

## Evidence

Download the `pi-ui-<SHA>` artifact. It includes preflight OS/model/memory/browser
information, `playwright.json` (test results, frame report, GPU metadata), screenshots,
video and failure traces. Each sample records measured requestAnimationFrame FPS,
p95 frame interval, visibility/focus, smoothed telemetry-application cost and the
20-module/40-cable counts. Pi drawing remains capped at30Hz; loop FPS is separate.
The job checks the ≥30FPS UI target in both profiles/views. Hosted cloud results
remain separate. Inspect renderer/display details and the exact revision before
recording results in PERFORMANCE.md and closing OPEN-5. This does not validate
real audio contention or latency; those belong to **Phase 8** in `wirescape`.

Hosted UI checks run independently, so an unavailable physical runner cannot block
the normal hosted regression job. Physical Pi runs serialize across revisions.

The current acceptance checklist, including Phase 7 fixes required before final benchmarking, is [PI_UI_TEST_PLAN.md](PI_UI_TEST_PLAN.md). Phase 7 currently requires longer Pi-only frame samples (15–30 seconds per condition), a second **signal-bearing FakeEngine** fixture, pacing/renderer/thermal context and a separate slow-device watchdog check; the existing 120-frame sample does not certify sustained Pi performance. Align per-test and job timeouts before extending the test. No physical Pi acceptance has been reported; the manual owner-only trigger is retained.

**Git branch reminder:** PR #6 was merged into intermediate `phase/5-system`, while `main` currently includes only through Phase 3.5. The Phase 7 branch is based on the cumulative Phase 6 merge. Review/merge the cumulative implementation into `main` before interpreting any main-only hardware run.
