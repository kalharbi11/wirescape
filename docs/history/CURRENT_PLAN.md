# Current plan and high-level review brief

Consolidated October26,2026. User requested implementation wrap and an up-to-date review handoff. Pause new feature work for review. Preserve the working app and evidence; do not interpret the unfinished queue as permission to implement during this review.

## Current priorities and accepted decisions

1. A solid personal standalone app covering the original musical plans. Future sharing is considered in source boundaries, not a reason to prioritize publishing/marketplace infrastructure now.
2. Native C++17 realtime DSP/engine; standalone Qt Quick UI; optional Python authoring/control outside realtime. No browser runtime requirement. Essentials are standard built-in modules.
3. Deterministic prepared processing, bounded queues/work, explicit lifecycle/cleanup, accepted document authority, stable IDs, lossless unsupported-data preservation and meaningful scripted checks.
4. Reusable DSP source for hardware integration by another agent. Thin board/host adapters and declared resources; Wirescape need not export or flash firmware. Avoid desktop dependencies inside DSP; do not force all desktop modules into a Daisy budget.
5. Quota-efficient work: bounded tasks and file ownership, one worker by default, independent second only when warranted, targeted higher-effort review for actual timing/architecture/numerical risks. Two reasonable attempts before escalating evidence. These model names in AGENTS apply to Codex execution; a Claude reviewer need not imitate a Codex model selector.

## Immediate next task: unattended audio capture and analysis

The user explicitly says they may be away and future tests must not depend on asking them to hear output. Current manual probe only runs a device stream and records counters; it is not the requested recorder/analyzer. Do not mislabel it as loopback validation.

Design one reusable, explicitly invoked test harness, separate from generic audio-off tests:

- Capture the tested Windows output endpoint/session using an appropriate loopback path where available; prefer isolation of Wirescape output. Do not use a microphone by default. Detect/report mixed unrelated audio or ambiguous capture rather than falsely passing. If a backend/endpoint cannot be captured, report unavailable distinctly.
- Produce deterministic reference WAVs/vectors from the same patch, parameters, sample rate and scheduled events. Save captured WAV plus endpoint/config/provenance and JSON/Markdown findings under build_reports, not user presets.
- Handle startup latency, alignment/correlation, channel mapping and possible sample-rate conversion explicitly. Never demand byte equality of a resampled/system-processed endpoint recording and raw engine samples.
- Analyze non-silence/RMS/peak/clipping/DC, dropouts/discontinuities, clock/envelope timing, spectral/response behavior and reference similarity with justified tolerances. Distinguish application faults, transport/device counters, capture failure and signal mismatch.
- Use known tones/pulses/sweeps and module-specific test patches in addition to combined Noise demo. Random-looking noise alone is weak evidence for filter correctness or timing. Include negative/self-tests proving silence/clipping/mismatch detection.
- Bound duration/storage/CPU, lower levels and stop cleanly on failure. No unattended endless playback, automatic audio start from GUI smoke or default check, forced app close with unsaved work, or fake perceptual certification.

Review questions: suitable Windows capture API/library and minimal dependency; per-process vs endpoint capture availability; safe start/stop ownership; honest scope of measured output vs physical speakers. Research primary documentation for actual API choice before implementing. This is test infrastructure, not a broad audio-device rewrite.

## Then: MIDI, OSC/UDP and bounded monitoring

User order is audio testing first, then communication modules, then sequencers/chords. Nothing in this stage implements these yet.

First settle the shared control/message boundary: single engine-enqueue owner, acquisition timestamps vs audio frames, bounded synchronized ingress, continuous-value coalescing, ordered note/gate loss handling, panic/reset acknowledgement, endpoint identity and shutdown. Existing engine enqueue is SPSC; callback/network/UI threads must not all call it. F06's offline barrier is not a live reset acknowledgement.

Implement in reviewable stages with fake clocks/transports first, actual loopback next and real devices last:

- MIDI In/Out/Monitor sharing device services; notes/channels/source ownership, explicit release/all-notes-off, scheduling, port-close/reconnect. Output event sinks must run even without an audio-output graph route.
- OSC In/Out: bounded packet/address parsing, address filters, numeric/string/trigger/structured values, destination/port settings and explicit timetag/late policies. Numeric values may cross a declared modulation adapter; strings/binary/objects do not become float-buffer hacks.
- Raw UDP Receive/Send as distinct text/binary packet services, with bounds, encoding/source metadata, backpressure and loopback tests. No synchronous sockets/parsing in audio callbacks.
- Shared Monitor/Print: bounded log/snapshot queue, pause/clear/filter/copy, timestamps/rate limits/dropped counters. UI formatting/copies must not lock rendering or grow without limit.

Acceptance: loss/overflow/out-of-order/generation/stop tests, serialization bounds, no devices needed for unit tests, truthful loopback evidence and clean teardown. Prefer proven libraries/adapters after focused primary-source review; don't silently widen the engine ABI or add installers.

## Then: musical timing, pitch and polyphony

Current API has no processor event outputs or polyphonic cable lanes. Define the missing contract before implementing a sequencer as if these exist.

- Pitch units/reference, scale/root/tie/hysteresis and a VCO pitch-CV/parameter target convention; then Quantizer.
- Global/patch transport vs standalone pulse Clock, ordered timed note/gate outputs, pattern persistence/reset/swing/ties/retriggers, late and overflow behavior; then Basic Sequencer.
- Note/voice allocation, source/channel/note identities, release/panic, voice limits/stealing and CPU/memory budget; then Chord Sequencer with Strum. Stacking note-ons into the current monophonic ADSR is not polyphony.
- Use offline reference timing/state tests and deterministic rendered captures before live device tests. Keep reusable scheduling/DSP separate from host/hardware services.

Remaining musical modules/enhancements: VCA LPG mode (preserve ordinary mode/IDs/defaults); ADSR cable gate with event/cable precedence; exponential Lag; oscilloscope/spectrum using shared bounded analysis taps and worker/UI-rate snapshots; one pinned-source Plaits or Braids port. See ESSENTIALS_MODULE_ROADMAP.md for all requested items. The six external DSP tasks are accepted; don't ask another agent to rebuild them.

## Native Surge / CHAOS and performance

NATIVE_SURGE_FEASIBILITY.md records a local Python extension but no direct native source/header setup. A small pinned-source, single-voice wrapper is the next feasible experiment before whole CHAOS migration. Establish revision/dependencies, parameter/preset state, block granularity, note behavior and output equivalence. Preserve the old binary and user presets. Do not assume a language change alone fixes CPU cost or that a desktop Surge design fits a microcontroller.

Separate equivalent optimization from deliberate sound redesign. Benchmark matched quality/latency, allocation and full callback cost; then optimize expensive repeated math, buffer traffic and crossings with regression evidence. Do not blindly change precision/quality, add threads or oversampling, or replace native algorithms to win a benchmark. Original benchmark71-test/old CHAOS numbers describe the old build, not current performance ceilings.

## Focused reviewer checklist

Prioritize concrete evidence for: callback/engine lifetime and cleanup races; SPSC generation/ordering and applied-reset gap; graph/event budget sufficiency and failure behavior; pure/staged document handling and unknown-data preservation; DSP stability and actual CPU costs; reusable hardware boundary/resource metadata; build provenance/process timeout behavior; private data/dependency/font isolation; correctness of latest claimed validation. Identify any subtle API contract gaps the new providers/polyphony would expose.

Deliver a concise findings report (severity, file/line, trigger/consequence, evidence, proposed bounded remedy/tests), what can remain, and an ordered task plan. Don't redo exhaustive historical reading, delete legacy/private data, claim unavailable hardware support, or freeze the SDK prematurely. Public distribution, trust/update marketplace and broad deployment remain lower priority.
