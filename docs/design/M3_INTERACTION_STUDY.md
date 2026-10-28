<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3-0: interaction study for the ABI 2 patching UI

**Scope:** design/acceptance study for issue #59, not an implementation specification or a claim that the app already uses engine2. No QML, C++, contract, schema, CI or device changes. M3 remains gated by M2-R; Preview 0.7 is a patching preview, not the Beta 0.8 playable-instrument milestone. Claude owns unresolved architectural/product decisions.

**Contract:** [ABI2_PLAN §2 D1–D7, D10, D12](https://github.com/kalharbi11/wirescape/blob/main/docs/specs/ABI2_PLAN.md) and [§5 B5](https://github.com/kalharbi11/wirescape/blob/main/docs/specs/ABI2_PLAN.md); [M3 and Preview 0.7](https://github.com/kalharbi11/wirescape/blob/main/docs/PLAN.md). **Live-session source:** [M2_PORT_AND_APP_ENGINE §M2-L](https://github.com/kalharbi11/wirescape/blob/main/docs/tasks/M2_PORT_AND_APP_ENGINE.md). Every suggestion below is a **proposed interaction**, not an existing API or an approved visual design.

## Baseline: what is actually in the app

These are source observations against main at the time of this study; the original HTML study is **illustrative only**, not the shipped QML.

| Current source | Verified behavior / M3 gap |
|---|---|
| [Main.qml:96–105](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L96-L105), [ModuleCard.qml:42–62](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ModuleCard.qml#L42-L62) | Patch cables use two clicks (output then input), enforcing matching legacy kinds. No drag-to-knob modulation target. |
| [Main.qml:111–120](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L111-L120), [Main.qml:273–306](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L273-L306) | Endpoints are positioned by port row; one cubic ShapePath per connection, fixed 2.2 px/3.2 px selected stroke and one circular hit target. No poly count, amounts, delay, split or per-channel peek. |
| [ModuleCard.qml:43–69](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ModuleCard.qml#L43-L69) | Input/output rows show label, legacy kind and click target; parameters appear as a flat list. No stereo_pair, hint, channel count or per-voice control decoration. |
| [ParameterControl.qml:9–54](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ParameterControl.qml#L9-L54) | A linear/log normalized Slider shows one accepted base value/display and calls setParameter. No knob rings, modulation targets, effective values or per-voice selection. |
| [studio_controller.cpp:105–132](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L105-L132) | QML receives id/name/kind on ports and numeric id, range, value, unit/display on parameters; connections are raw document objects. The exposed maps do not currently supply ABI2 channel counts, hints, flags, amounts or effective per-channel values. |
| [studio_controller.cpp:164–183](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L164-L183), [studio_controller.cpp:280–293](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L280-L293) | ABI1 document validation rejects cycles and audio/control kind mismatches, allows one cable per input, and connectPorts creates a plain connection. M3 must use the ABI2/app bridge after M2 rather than styling unsupported legacy behavior. |
| [studio_controller.h:12–76](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.h#L12-L76) | Existing Q_PROPERTY/Q_INVOKABLE API has no per-channel signal peek, modulation amount operation, cable undo/redo, feedback metadata or stereo split operation. These are integration gaps, not permission to alter the ABI. |
| [Main.qml:161–184](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L161-L184), [Main.qml:261–268](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L261-L268) | On-screen musical-keyboard focus/release safety, canvas mouse-pan and wheel zoom exist. Pointer/touch patch gestures and undo are not provided here. |
| [ui_tests.cpp:156–205](https://github.com/kalharbi11/wirescape/blob/main/native/ui/tests/ui_tests.cpp#L156-L205), [ui_tests.cpp:298–307](https://github.com/kalharbi11/wirescape/blob/main/native/ui/tests/ui_tests.cpp#L298-L307) | Real-QML tests cover minimum 1000×650 layout, keyboard focus/held-note cleanup and output/input mouse-click cabling without audio. They are a useful offscreen test style to extend. |
| [Original HTML:79–88](https://github.com/kalharbi11/wirescape/blob/main/docs/design/WIRESCAPE_Studio_Original.html#L79-L88), [189–215](https://github.com/kalharbi11/wirescape/blob/main/docs/design/WIRESCAPE_Studio_Original.html#L189-L215) | Exploratory circular knobs and explicit L/R ports appear in the visual prototype; its mock waves/meters and simulated controls are **not evidence of QML or runtime functionality**. |

**Cross-cutting interaction rules to test:** mouse and touch target the same semantic actions; keyboard navigation has visible focus and an escape/cancel path; tooltips have persistent/tap-accessible alternatives; don't rely on color, line thickness or animation alone. All value edits must distinguish **base** from **effective** value. A pending gesture is not an undoable committed edit. Actual graph edits currently require audio stopped ([Main.qml:96–104](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L96-L104), [studio_controller.cpp:240–243](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L240-L243)); whether M3 may stage edits while audio runs is a Claude/engine integration question, not a UI assumption.

## 1. Poly cables: thickness, count badge and inspection

**Contract:** D1 (1–16 channels), D2 (source-count propagation, mono broadcast; SUM/FIRST/POLY at sink), D5 (allowed signals), B5 (thickness/badge, per-channel scope peek).

**Show / allow (proposal):** encode source channel count as bounded width *and* a readable “1 ch”, “4 ch”, “16 ch” badge, with full count/port hints on selection. When dynamic voice count changes between blocks, the badge updates without treating the change as a new cable. Clicking the cable opens a lightweight inspector with each endpoint, source count and the sink's declared conversion (SUM/FIRST/POLY) **if exposed**. A one-channel connection must not falsely display 16 solely because it feeds a poly consumer. A selected channel may show a bounded, UI-rate waveform/signal peek after an engine-provided tap exists; never sample audio from the QML thread.

- **Mouse:** choose/click cable, hover for count and conversion; connect ports through the existing selection flow until drag routing is implemented.
- **Keyboard:** tab to an accessible cable target, Enter opens inspector, arrows choose a channel, Escape closes without changing routing.
- **Pi touch:** tap a generous hit region to select; tap again/inspector for details; do not require hover or precision drag to read count.
- **Undo:** a changed connection is one undoable operation; runtime channel-count fluctuations and inspector/channel selection are not document edits.
- **Current/gap:** [Main.qml:273–302](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L273-L302) renders fixed-width cable geometry; [studio_controller.cpp:105–132](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L105-L132) has no source-channel data.

**Acceptance scenarios (offscreen with injected counts, audio off):**
1. Given 1/4/16-channel output models, each connected cable renders the corresponding text badge and monotonic bounded thickness; selecting reports the same count, independent of zoom.
2. Updating one mocked output from 4 to 1 to 16 channels updates the badge without creating/removing a cable, changing base parameters, or marking the patch dirty.
3. A mono-to-poly cable and a poly-to-SUM cable are distinguishable in the inspector; keyboard/touch inspection returns the same metadata.

**Question for Claude:** which engine2/controller snapshot exposes live per-edge counts and safe decimated channel samples, and at what UI refresh cadence? Is per-channel scope peek part of M3's first cut or a follow-on within B5?

## 2. Drag-to-knob modulation and cable amount

**Contract:** D6 (only MODULATABLE parameters), D7 (multiple sources, attenuverter −1..+1, normalized effective math, non-destructive base), D5 (hints warn but do not forbid signal routing), B5 (drag-to-knob and amount handle).

**Show / allow (proposal):** mark eligible parameter targets during a drag from *any signal output*. A successful drop creates a **modulation cable with its own stable identity and amount control**, distinct from a regular signal-port cable; default amount should be visible and confirmed. Show signed value, zero midpoint, clamping at −1/+1, and multiple cables targeting one parameter without conflating them. Non-MODULATABLE or STEPPED-disallowed targets explain rejection without mutating the patch. Soft hint mismatch is informational, not a block.

- **Mouse:** drag from output jack onto highlighted knob/parameter hit target; ghost route follows pointer; release commits. Drag amount handle vertically/horizontally or select and type a numeric amount. Escape or release over empty canvas cancels.
- **Keyboard:** focus output, invoke “Connect modulation”, choose target parameter from a searchable/accessible list, set amount in a signed numeric field; Enter commits, Escape cancels. Tab reaches the amount handle after connection.
- **Pi touch:** press/select source, tap eligible target, then use a large signed amount slider or minus/plus controls in the inspector; keep the canvas-pan gesture separate from cable initiation.
- **Undo:** one undo removes the created modulation cable (including amount); amount edits have undo/redo entries, restoring the previous signed value but **never** a base parameter value. Cancel has no history entry.
- **Current/gap:** [Main.qml:96–105](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L96-L105) requires a matching legacy input; [ParameterControl.qml:23–45](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ParameterControl.qml#L23-L45) is only a Slider, not a cable destination; [studio_controller.h:62–68](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.h#L62-L68) has no mod-cable or amount mutation API.

**Acceptance scenarios (offscreen):**
1. Mouse-drag output to a MODULATABLE cutoff control: exactly one modulation connection appears with a visible amount, while cutoff base value remains byte-for-byte unchanged. Escape before release creates none.
2. Attach two sources to the same cutoff; set amounts +0.5 and −0.25; both display independently and persist on save/reload. Mock effective value reflects the normalized D7 sum/clamp, without rewriting base.
3. Keyboard-only and synthetic touch workflows produce the same saved connection fields; invalid/stepped target rejects cleanly, emits an accessible reason, and leaves history/patch unchanged.

**Question for Claude:** choose stable document IDs and a reversible command/undo model; what is the default modulation amount? How should editing behave while audio runs, given current topology lockout?

## 3. Base-value versus modulation rings

**Contract:** D6 (stable parameter descriptors, taper, PER_VOICE flags), D7 (base + effective modulation, normalized-space clamp), B5 (base pointer plus modulation ring).

**Show / allow (proposal):** a primary knob/control represents the **saved base**. A separate ring/arc indicates the **current effective value** and its positive/negative displacement; nearby text or an accessible inspector identifies both numerically in the parameter's real units. Clamp must be visualized without implying that the base was clipped or moved. Unmodulated parameters show base only; never label an animated value “saved” or draw a stale effective ring when data are unavailable. Prefer non-color cues (pointer shape, labels and ring separation).

- **Mouse:** drag/scroll the primary control to edit base; hover or click displays base, effective and contributing modulation amounts. Dragging the ring is **not** an implicit base edit.
- **Keyboard:** arrow/page keys change base using descriptor step/taper, Home/End honor bounds; focusable “modulation details” gives a textual effective value. Escape cancels an in-progress uncommitted edit.
- **Pi touch:** vertical/rotary touch edits base with a numeric readout and an accessible fine-adjust mode; tap displays effective value without changing base.
- **Undo:** base edits undo/redo the base only. Effective-value animation is not undoable; changing a cable amount is a separate command (feature 2).
- **Current/gap:** [ParameterControl.qml:9–53](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ParameterControl.qml#L9-L53) stores only acceptedValue/acceptedDisplay and its normalized Slider; the HTML mock's circular knob [at lines 79–88](https://github.com/kalharbi11/wirescape/blob/main/docs/design/WIRESCAPE_Studio_Original.html#L79-L88) has no engine-backed effective ring.

**Acceptance scenarios (offscreen):**
1. With base 0.4 normalized and an injected effective 0.8, UI exposes two distinct values; saving records 0.4 as base and not 0.8.
2. Effective value reaches 0 or 1 under negative/positive modulation; the ring is clamped while moving base or undoing base edits changes only the saved value.
3. Removing modulation hides the effective overlay without moving the base pointer. Keyboard and touch inspectors still expose the base; unavailable live data says “no live value” rather than guessing.

**Question for Claude:** should the effective ring show latest selected voice, min/max envelope across all voices, or both? How is a UI-rate effective-value snapshot exposed without polling raw audio buffers?

## 4. Feedback/back-edge cable markers and delay

**Contract:** D10 (cycles valid, deterministic SCC back-edge and per-back-edge delay equal to saved feedback_block); D12 (latency visibility), B5 (marker and delay).

**Show / allow (proposal):** every engine-designated back-edge gets a non-color icon/dashed segment and “feedback: N frames” label. A loop can contain multiple edges; mark **only** the edges selected by engine2, not every connection in the SCC. Selecting the edge explains the deterministic delay, current feedback_block and any setting-to-delay relationship. For invalid setting/loading failures, explain rather than silently omit.

- **Mouse:** select marked edge to see delay; change patch feedback setting via a dedicated inspector if exposed.
- **Keyboard:** tab to edge, Enter reads delay, accessible feedback settings use numeric spinbox/field with validation.
- **Pi touch:** tap the marker or wide hit zone; delay remains legible without hover, and controls do not overlap nearby cables.
- **Undo:** changing feedback_block is a single reversible patch-level setting; selecting/inspecting edges is not. If changing settings causes edge recalculation, rebuild markers from engine metadata, not local graph guesses.
- **Current/gap:** [studio_controller.cpp:164–183](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L164-L183) rejects cycles; [Main.qml:273–306](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L273-L306) has no metadata for selected back-edges.

**Acceptance scenarios (offscreen):**
1. A synthetic three-node cycle with precisely one engine-selected back-edge marks exactly that edge with “32 frames”, while forward edges are not labelled delayed.
2. Change mocked feedback_block from 32 to 1: marked edge reads “1 frame” without deleting the cycle; save/reload retains setting; undo returns to 32.
3. Reorder node insertion in a fixture representing the same graph: UI reflects *supplied engine designation* deterministically; keyboard/touch read the same delay.

**Question for Claude:** which API reports SCC/back-edge identity, actual delay and per-bus path latency? How does UI safely preview/reconfigure feedback_block while audio is running?

## 5. Stereo-pair ports and a splittable paired cable

**Contract:** D3 (left/right are **two independent ports** linked by stereo_pair hint, channels mean voices), D5 (routing), B5 (paired cable with split).

**Show / allow (proposal):** give linked L/R jacks one visual stereo grouping with explicit L and R labels, and a bundled cable when both corresponding edges are present. A **Split** action reveals the two ordinary cables and independent endpoints; an **Unsplit** view preserves physical port identities and cable IDs. One missing side should appear as a partial pair, not a silently duplicated mono source. Two voices of stereo must remain distinct from two audio channels L/R.

- **Mouse:** drag/click grouped stereo source to paired target for two explicit L/R connections; select the bundle and choose Split. Individual rerouting requires visible individual cables.
- **Keyboard:** group has a focusable “Connect stereo pair” action, with separate L and R destinations announced; Split/Unsplit are menu actions accessible via Enter/Space.
- **Pi touch:** larger joint jack plus labelled L/R split targets; avoid a tiny double-click-only split affordance.
- **Undo:** the paired connect is one reversible compound operation restoring both edges; rerouting only R changes only R. Presentation-only Split/Unsplit must not rewrite audio routing unless explicitly defined as a saved visual preference.
- **Current/gap:** [ModuleCard.qml:42–63](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ModuleCard.qml#L42-L63) treats each port as an unrelated row; [Main.qml:273–306](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L273-L306) draws each connection separately. The [HTML prototype:189–206](https://github.com/kalharbi11/wirescape/blob/main/docs/design/WIRESCAPE_Studio_Original.html#L189-L206) names L/R but is not the app.

**Acceptance scenarios (offscreen):**
1. Connect paired source L/R to paired target L/R: underlying model has **two** distinct stable port-to-port connections but rendered view shows one named stereo bundle.
2. Split, redirect right only and undo: left is unchanged throughout; right returns to its previous endpoint. Save/reload keeps original port IDs and stereo intent.
3. A source with four voices on both L/R shows “stereo × 4 voices”, never “8-channel stereo”; a half-connected L-only pair is visibly incomplete.

**Question for Claude:** is grouping determined purely from matching stereo_pair hints or explicitly saved visual grouping? How should a grouped source connect to two unrelated mono inputs, or a mono source feed both sides?

## 6. Per-voice parameter display

**Contract:** D2 (1–16 dynamic voices), D6 (PER_VOICE), D7 (poly modulation), B5's per-channel scope peek. No per-note MIDI UI is required in this study.

**Show / allow (proposal):** on PER_VOICE parameters or poly-modulated parameters, display a voice-count label, a selected voice index (1-based for users) and that voice's **effective** value alongside the single saved/base control. Changing the selected voice is an inspection action, not a hidden automatic channel allocation or per-voice base override. Show out-of-range/inactive voice as unavailable after channel-count changes. A concise per-channel scope peek belongs near selected signal/knob when the engine supplies safely decimated values.

- **Mouse:** click voice badge for 1…N chooser; hovering the effective arc shows selected voice and effective value; canvas shows scope peek on explicit inspection only.
- **Keyboard:** focus voice selector, arrows/Home/End choose existing indices; screen-reader text includes “voice 3 of 4” and whether parameter is PER_VOICE versus poly-modulated.
- **Pi touch:** tap a large voice chooser, swipe/tap individual voice chips without interfering with knob adjustment; scope peek can be opened/closed by tap.
- **Undo:** voice selection and peek visibility are view state, not patch edits. Base parameter edits retain D7's non-destructive semantics. Never invent per-voice stored values unless a later contract explicitly supports them.
- **Current/gap:** [ParameterControl.qml:9–61](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ParameterControl.qml#L9-L61) receives one display value; [studio_controller.cpp:105–118](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L105-L118) does not expose PER_VOICE flags/effective per-channel snapshots.

**Acceptance scenarios (offscreen):**
1. Mock four distinct effective values with one base: voice selector shows indices 1–4; choosing voice 3 changes only the effective readout, not base/dirty state.
2. Drop channel count from 4 to 1 while voice 3 is selected: no stale voice-3 sample remains, and selection resolves to a valid voice with an accessible change notice.
3. For a non-PER_VOICE, unmodulated parameter, voice selection is absent; for a poly-modulated mono base parameter, channel-effective values may differ while the base remains shared. Test at 1/4/16 voices.

**Question for Claude:** where is live, per-voice effective metadata published, and which parameters qualify for an individual channel display? Are scopes part of the same bounded analysis-tap service planned for M6, or a small M3-only preview?

## B5 adjacent details not to lose

- **Soft hint mismatch — D4/D5:** compare stable signal hints (audio, pitch, cv_bipolar, cv_unipolar, gate, trigger, any); use understated warning color **plus** warning icon/text, without rejecting legitimate audio-rate modulation. Current controller rejects mismatched legacy audio/control kinds ([studio_controller.cpp:280–287](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L280-L287)). A B5 offscreen case should verify the mismatch warning and allowed connection after the ABI2 bridge is in place.
- **Safe paint/data boundary — D1/D11 and B5:** display only bounded, immutable UI-thread snapshots for dynamic channel badges/effective rings/scopes. Neither QML nor its test fixture is an audio callback; sampling an engine buffer directly or forcing a redraw per audio frame is not acceptable. Choose a cadence/buffer budget during M3 integration.
- **Gestures/navigation:** preserve existing Escape-to-cancel and held-note/focus safety ([Main.qml:29–41,161–184](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L29-L41)); use distinct affordances for port drag, parameter adjustment, canvas pan and module move. Touch interaction should be simulated in offscreen tests; a real Pi-screen trial is a later user/device task.
- **Undo is a dependency, not a hidden feature:** [studio_controller.h:51–76](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.h#L51-L76) exposes no command history API. Acceptance describes reversible outcomes; Claude must assign a separate command-transaction model before implementation. M3 must not solve this by patch-schema or ABI header edits without a scoped issue.

## M2-L live-session checklist → UI touchpoints

The M2-L checklist belongs to the earlier engine2 milestone and real audio remains **user-only**. M3 should preserve these workflows and extend offscreen assertions, not claim it can verify their sound.

| M2-L item | UI component / M3 interaction | Headless/offscreen surrogate |
|---|---|---|
| 1. Build VCO → VCF → VCA → Output, ADSR, LFO → cutoff | [ModuleCard.qml:43–70](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ModuleCard.qml#L43-L70), new modulation target/amount | Insert known nodes, connect ports and drag/drop LFO→cutoff; assert routing, amount, unchanged base and no audio device opened |
| 2. Single notes, chords and rapid repeats | [Main.qml:161–184](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L161-L184) keyboard; poly channel badge/voice inspector | Synthesize key presses/releases and focus changes; assert no latched key, then inject 1/4/16 channels for badge/voice UI. Musical timing requires live session |
| 3. Turn every knob while playing | [ParameterControl.qml:23–53](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/ParameterControl.qml#L23-L53), new base/effective rings | Simulate mouse/keyboard/touch base adjustments; assert parameter bounds, display and no change to modulation amounts |
| 4. Save/close/reopen | [Main.qml:43–88](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L43-L88) dialogs; new stereo/mod/feedback state | Round-trip v2 fixture with exact stable endpoint/parameter IDs, modulation amounts and feedback setting; confirm base values remain distinct from effective display |
| 5. Demo and legacy patch | [studio_controller.cpp:105–132](https://github.com/kalharbi11/wirescape/blob/main/native/ui/studio_controller.cpp#L105-L132) projection and patch-loader migration | Load v1/migrated v2 fixtures; show old cables/parameters without phantom stereo, voice or modulation metadata; do not claim sound equivalence |
| 6. Leave patch running for 20+ minutes | [Main.qml:373–380](https://github.com/kalharbi11/wirescape/blob/main/native/ui/qml/Main.qml#L373-L380) status/CPU/underruns | Static/streamed fake status and per-channel samples stay responsive and bounded while the offscreen window is up. Dropout and audio quality remain physical-user checks |
| 7. Record confusing controls | All six affordances, especially touch/no-hover and feedback delays | Task-based checklist: connect, modulate, inspect voice, split stereo, explain delay, undo and keyboard/touch access. Log confusions as issues after session |

## Suggested M3 test fixture and decision handoff

Use the existing [offscreen QML harness](https://github.com/kalharbi11/wirescape/blob/main/native/ui/tests/ui_tests.cpp#L156-L205) at **1000×650 and 1400×900**. Feed QML a **read-only/mock ABI2 view model** with two modules, 1/4/16 channel transitions, one MODULATABLE/PER_VOICE parameter, one nonmodulatable parameter, two mod cables, one engine-labelled back-edge and one stereo_pair. Assert object-visible labels/geometry and serialized command requests in addition to real mouse/keyboard events; synthesize Qt touch events where supported. Do **not** build a pretend DSP graph or assert waveform/audible results. Make test hooks stable through objectName/accessibility roles rather than pixel/color snapshots alone. Existing source tests prove click cabling ([ui_tests.cpp:298–307](https://github.com/kalharbi11/wirescape/blob/main/native/ui/tests/ui_tests.cpp#L298-L307)), not these M3 gestures.

**Claude decisions before implementation:** (1) controller/UI snapshot API and update cadence; (2) undo/redo transaction model; (3) graph-edit restrictions while audio runs; (4) default amount and exact mod-cable targeting UX; (5) voice-ring aggregation and scope peek budget; (6) stereo grouping persistence; (7) presentation of engine-selected back-edges and feedback_block; (8) touch-screen minimum viewport and accessible target sizes. No decisions are made by this document; no current ABI or patch schema is modified.

**Verification status:** source review only. No QML tests, CI, Windows/Pi runs, audio or MIDI devices exercised by GitHub-connected ChatGPT.
