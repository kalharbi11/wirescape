<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio: diagnose silence and make physical audio output selection clear

Priority: first; blocks the user's one-minute audible checkpoint (#45). Screenshots show live audio but no sound; physical interface absent. Do not claim a cause or completed audible evidence from UI state alone.
Read first: native/ui/audio_device.*, studio_controller.*, qml/Main.qml, audio_device_tests and controller_failure_tests, docs/design/STUDIO_REFINEMENT_BRIEF.md.
Confirmed: Refresh/device ComboBox disabled while running; controller refuses refresh while a stream exists; devices() currently hides outputs with fewer than2 channels. Investigate host API/device enumeration and backend/DLL availability using existing diagnostics and fake backends. A mono device must be shown as unsupported with a reason or safely supported; never silently disappear or assume stereo support. No new backend/ASIO dependency or driver download without a separate decision.
Implement an unambiguous stopped/cleanup/refresh/select/start flow, actionable disabled-state explanation and visible active physical device/backend/channel/rate. Keep graph output-node selection distinct from hardware device selection. Preserve callback quiescence and explicit user start. Expose supported channel/rate failures; do not auto-start, guess that a running stream is audible, or replace the user's route silently. Capture diagnostics that distinguish graph silence, unavailable device, wrong route and stream failure.
Tests: fake enumeration incl default/nondefault/mono/disconnected device, refresh after stop/cleanup, selected ID roundtrip, start failure and graph output vs hardware selection. Real listening/interface enumeration is USER ONLY; provide a short checklist and record its result separately. User can stop audio, Refresh, select interface and restart now; failure remains a defect, not user error.

```acceptance
tier: T2
budget: 1 Codex session or bounded ChatGPT task
size: <= 600 changed lines, <= 10 files
must_pass:
  - ctest --test-dir build/native -R "studio_ui_tests|controller_failure_tests|audio_device_tests" --output-on-failure --no-tests=error
  - ci: Windows Qt build and actual named offscreen tests green on exact head
stops: [ABI header edit, new dependency, format or ID change, tests removed, unapproved threshold change]
```
