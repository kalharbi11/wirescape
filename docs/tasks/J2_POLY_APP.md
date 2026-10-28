<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J2: polyphony in the app (#69, PR #79, stacked on J1 #78)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "A2 / J2". Summary: shared note→CV core (`native/voice/`, MIT) reused by J4; `host.keyboard` ABI 2 source (outputs `pitch`, `gate`, `velocity`, `notes`; params `voices` 1–16 default 8, `mode` rotate/reuse/reset/mono, `octave` −3..+3) fed by the on-screen and computer keyboard through D13 ingress; chords from the computer keyboard; poly summed at the output; channel-count badge on poly cables; panic chokes all voices.

**Status (2026-10-28):** draft PR #79 on top of #78. Finish after J1's render decision (see `J_JAM_CHECKPOINT.md`, "Planner decisions 2026-10-28b").

**Read first:** `native/voice/allocator.h`, `native/tests2/test_modules.h` (`NotesToCv`), `native/ui/studio_controller.*`, brief `docs/design/BETA08_MUSICAL_CORE_BRIEF.md` §2 (CV-01, 04, 05, 07, 09).

```acceptance
tier: T1
budget: 2 Codex sessions
size: <= 800 changed lines, <= 14 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_host_keyboard_tests|app_patch_tests|abi2_voice_tests" --output-on-failure
  - ctest --test-dir build/native -R studio_ui_tests   # chord produces 4 active channels (offscreen, needs the Qt build)
  - ci: linux-arm64, windows-x64, sanitizers, guard all green on the exact head
  - pi: ctest -R "abi2_host_keyboard_tests|app_patch_tests"
stops: [ABI edit, new dependency, threshold change, unspecified design choice]
```
