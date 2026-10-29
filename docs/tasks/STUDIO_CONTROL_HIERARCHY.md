<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio UI: module-specific knobs, sliders and control hierarchy

Depends on the refinement brief only; coordinate Main.qml/test edits with layout task, otherwise separate PR. Own ParameterControl.qml/ModuleCard.qml/new reusable QML controls and module presentation metadata. Read module descriptors to map EXISTING stable parameter IDs; no new DSP parameter, range/default/unit or patch-format change. Same source of accepted controller values and failure rollback as current sliders.
Build reusable accessible rotary knob and slider presentations, keyboard arrows/fine adjustment, numeric/display value, focus, hover and drag behavior. Respect existing log/linear and stepped mappings. Large primary controls, smaller related secondary controls and compact selectors form hierarchy; do not turn every slider into an identical knob. Palette stays warm off-white/charcoal with restrained orange and signal accents; vary size/grouping first, color only when meaningful. No new font/asset/dependency.
Starting module decisions: VCO pitch/frequency primary knob, waveform as existing stepped selector and smaller related controls; LFO rate primary knob with smaller shape/depth controls where descriptors exist; VCF/Ladder cutoff large, resonance small, drive secondary; ADSR stage sliders as a related group with sustain level distinct; VCA gain slider; mixer level faders; clock tempo knob plus existing division/pulse-width secondary controls; function envelope rise/fall paired controls, shape secondary; slew rise/fall paired; scale/offset compact paired knobs; Noise level slider. Only map parameters that exist, and document exceptions. Output shows route/meter/state, not invented gain. CHAOS/Plaits use this shared stock visual language for now, with no new catalog registration; bespoke skins deferred.
Modulation indicator API may be stubbed as explicit unavailable/hidden state only. No synthetic animation, no fake moving dot, no audio-thread reads. Telemetry task supplies actual values later. Tests cover normalized mapping, stepped values, external parameter updates, rejected-write rollback, keyboard control and unchanged stable IDs. Provide representative module screenshots; user approval can refine cosmetics after review.

```acceptance
tier: T1
budget: 1 Codex session or bounded ChatGPT task
size: <= 650 changed lines, <= 10 files
must_pass:
  - ctest --test-dir build/native -R "studio_ui_tests|controller_failure_tests|audio_device_tests" --output-on-failure --no-tests=error
  - ci: Windows Qt build and actual named offscreen tests green on exact head
stops: [ABI header edit, new dependency, format or ID change, tests removed, unapproved threshold change]
```
