<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3 readiness gaps at the current main revision

Historical source snapshot: 2026-10-30. PR statuses below are as observed during this audit; #153 has since merged and #147 has newer commits. Re-check live status before assigning implementation. Audited `origin/main` is `b7586874604de53736b72daacc87240619784685`. This is a source audit, not an M3 review or design approval. [The M3 plan](../PLAN.md) names poly cables, modulation drag-to-knob with an amount, base/modulation rings, feedback back-edge markers, and stereo pairs; Preview 0.7 additionally waits for an M2-L-style live session.

## Current source and open PRs

| M3 capability | Current main at audited SHA | Relevant open PRs / remaining gap |
|---|---|---|
| Poly cables | Partial: `StudioController::connections()` propagates configured voice counts and the canvas displays a count badge when `channels > 1` (`native/ui/studio_controller.cpp`, `native/ui/qml/Main.qml`). J2 documents the badge (`docs/tasks/J2_POLY_APP.md`). | #145 changes cable geometry, not poly or modulation semantics. Head `0c49433a308699cb3653febb22a63a1f501048f8`, open. |
| Modulation cable gesture and amount | Missing in Studio: `connectPorts()` connects typed ports; `ParameterControl.qml` edits only the saved parameter value. ABI2 engine has `Engine::modulate()` (`native/engine2/engine2.h`), but the Studio controller has no parameter-edge operation/model. | No open PR covers this. The data model and user-visible amount need a separately authorized task. |
| Parameter controls and modulation indicator | Main has sliders, accepted-value display and rollback (`native/ui/qml/ParameterControl.qml`, `native/ui/studio_controller.cpp`). It does not show base/modulation rings or effective modulation. | #147 proposes module-specific rotaries/accessibility, open at `92395e0400f8f78bdfbb344bcf7a7c1c1c0b176f`; it does not supply modulation telemetry. #153 is an adjacent clock-control change, open at `295155829ca97323f6345b33225ea55ed2cdfa45`, and also touches `ModuleCard.qml`/`ParameterControl.qml`. |
| Feedback and back-edge markers | Missing in Studio. `validateDocument()` rejects graph cycles with “Feedback cycles require an explicit delay and are unsupported” (`native/ui/studio_controller.cpp`). The UI draws ordinary cubic cables only (`native/ui/qml/Main.qml`). | No open PR covers graph-cycle authoring or back-edge rendering. ABI2 engine loop support does not mean the Studio can save or show those edges. |
| Stereo pairs | Missing in Studio presentation. Module2 descriptors carry `stereo_pair` (`native/include/wirescape/module2.h`); CHAOS-lite declares `left`/`right` as pair `"stereo"` (`native/modules2/chaos_lite/dsp.h`). `StudioController::modules()` exports port id, label and kind but drops pair metadata. | No open PR adds pair metadata or paired-cable display. |

#145 is sidebar/footer and distance-aware cable layout; its open head does not remove the 55-pixel minimum in current main. #147 owns the shared control presentation files; #153 also edits those files, so coordinate before assigning more `ModuleCard.qml`/`ParameterControl.qml` work. These PRs are not merged evidence. Exact audited heads above were read from GitHub on the audit date.

The control-hierarchy task already says modulation indication may stay hidden/unavailable until real values exist. The separate [modulation feedback task](../tasks/STUDIO_MODULATION_FEEDBACK.md) requires tracing actual effective values and bounded snapshots. Do not infer a parameter target from a nearby CV cable, animate a decorative indicator, or read mutable DSP state from QML/audio callbacks.

## Candidate bounded work (not approved)

1. Finish and review existing #141/#142 UI work first. It owns current shared QML edits and provides geometry/control tests; it does not close the M3 modulation, feedback, or stereo gaps.
2. Draft a planner-reviewed parameter-edge model and user flow for drag-to-parameter plus amount. Only after the representation is authorized, a UI/controller task can add save/load and tests for stable parameter IDs, amount round-trip and rejected edits. Ownership: Studio UI/controller agent; engine integration only if the selected contract requires it.
3. After parameter-edge semantics exist, scope actual modulation feedback using `docs/tasks/STUDIO_MODULATION_FEEDBACK.md`; separately scope feedback/back-edge and stereo-pair presentation against data the Studio can author and persist. Candidate acceptance should test effective-value snapshots and base-value preservation, cycle/back-edge state, and stereo-pair metadata without reading DSP state from the UI thread. Ownership: engine/control and UI agents as the authorized boundaries require.

These are sequencing ideas only. The planner must create/authorize each concrete task; this note selects no schema, telemetry API, or UI behavior. No M3 feature or preview is claimed complete, and no laptop session was performed.
