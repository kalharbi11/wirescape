<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio: actual modulation feedback and LFO activity telemetry

Depends on shared control presentation contract; can design bounded telemetry independently. Read engine2 modulation/meter API, module parameter/CV semantics, ui controller/ParameterControl and refinement brief. First document the exact source of truth and parameter-vs-CV-input associations. Do not infer that any cable near a knob modulates that parameter.
Show a subtle moving dot/arc only for a parameter with an active, supported modulation association. Its position reflects the actual effective value after relevant mapping/clamp, while the base knob stays at the saved/manual value. Unmodulated controls have no modulation dot. Disconnect/load/panic/reset clears stale indication appropriately. An LFO's own phase/activity may have a separately named live visualization (if real phase/output telemetry exists); it must not masquerade as modulation of its rate knob.
Publish bounded snapshots from the render owner; UI polls a stable bounded cadence (e.g.30Hz) and may smooth display only. No allocation/locks/Qt/logging in callbacks, no UI dereference of mutable DSP state, no control writes driven by display animation. Mark unavailable telemetry honestly instead of guessing or running a decorative oscillator. CV-port modulation of cutoff/gain must use verified DSP semantics; an unobservable effective target requires a reviewed additive internal instrumentation design. No ABI header changes: stop and request a separate contract task if needed.
Tests with deterministic fake/offline source cover unmodulated/connected/disconnected, positive/bipolar/clamped motion, stopped/frozen/unavailable states, base value preserved, finite bounded samples and race-safe snapshots. Offscreen visual tests never open devices. Request Pi evidence for callback/instrumentation changes; no new performance threshold or patch-format changes.

```acceptance
tier: T2
budget: 1 Codex session or bounded ChatGPT task
size: <= 700 changed lines, <= 12 files
must_pass:
  - ctest --test-dir build/native -R "studio_ui_tests|controller_failure_tests|audio_device_tests" --output-on-failure --no-tests=error
  - ci: deterministic snapshot/concurrency tests and allocation checks green; Pi evidence for callback changes
  - ci: Windows Qt build and actual named offscreen tests green on exact head
stops: [ABI header edit, new dependency, format or ID change, tests removed, unapproved threshold change]
```
