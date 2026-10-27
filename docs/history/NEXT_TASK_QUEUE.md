> Current consolidated priority/review plan: [CURRENT_PLAN.md](CURRENT_PLAN.md). User requested implementation wrap for external review. The queue below is historical stage detail and must not override that pause or current scope.

# Wirescape staged task queue

October 26, 2026. The user authorizes continuing staged work while account access allows, with model failover and concise handoffs. This queue does not declare the foundation released or its experimental SDK stable.

## Operating rules

- Finish one reviewable stage, save its evidence and update BUILD_CHECKPOINT before starting the next.
- Default coordinator plus one lower-cost worker. A second worker requires independent files and checks. Mechanical inventory/tooling: Luna Medium; substantial native/controller work: Sol Medium.
- At a model-specific limit, preserve the current diff, failed command/log, next action and acceptance test; dispatch that bounded remainder to another available suitable model. Do not repeatedly retry the limited model or automatically use Ultra/Max.
- Two reasonable implementation attempts per difficult failure, then a focused evidence-based review. Research should answer a specific unresolved question using primary sources. Avoid changing assertions merely to obtain a green result.
- Never spend a nearly exhausted context on broad rediscovery. Read the current checkpoint and the task packet. Do not modify another worker's owned files without coordination.
- Automated tests and startup probes keep audio off. Actual hardware sessions require user initiation. Preserve user_data, Surge binaries and baseline/0.5.2-local.

## Current gate: review hardening — accepted

**Latest acceptance:** `check-20261026T152345Z.json` passed after F04 and F03 integration; 26 Python tests, seven CTest suites, both UI sizes/startup/benchmarks/render/package passed. Smart App Control was turned off by the user. Earlier signing blocks are historical. F03 local portable acceptance strengthening is accepted. The ARM64 evidence packet remains preparation until executed on actual target hardware.

**Owner:** coordinator; no active worker remains at this checkpoint. Read BUILD_CHECKPOINT.md before selecting new bounded work.

Completed implementation includes quiescent initial parameter setup, shared native ramps, format probing/device refresh/session diagnostics, real save failure injection, keyboard tracking, process-tree timeouts, actual artifact/source reporting, startup readiness, and a single scripted check/summary command.

The popup regression is resolved with a test-only window-activation precondition and its original assertions intact. All seven current CTest suites pass. See IMPLEMENTATION_STAGE_2026-10-26.md for historical investigation.

**Acceptance:** `python tools/dev.py check --package` passes without skipping the regression; both normal-window and headless focused Escape paths are corroborated; current source/build identity and remaining hardware limits are recorded.

## Task F01: historical document fixture inventory

**Status:** completed independent preparation; six fixtures and persistent shape checks. No migration is implemented.

**Model/ownership:** Luna Medium; `native/tests/fixtures/legacy_documents/` synthetic JSON and `docs/LEGACY_FORMAT_FIXTURES.md` only.

**Inputs:** targeted reads of active legacy serializers/loaders under `wirescape/`; approved plan's document milestone. Search first; do not read/cache all legacy assets.

**Deliver:** minimal synthetic fixtures for formats/versions actually evidenced in code (`wirescape.patch` v1/v2/v3 and `wirescape.visual.study` v4 where supported), including unknown module/field/cable payload and malformed/future documents. Record exact producer/reader locations and fields. Mark any historical format not evidenced locally as unresolved rather than inventing it. Use no private presets or copied Surge data.

**Acceptance:** JSON parse and schema-shape checks; fixture paths/format/version are unique and provenance documented. No migration implementation or claim of migration equivalence yet. Concise output lists evidence, fixtures and unresolved mappings.

## Task F02: pure document validation/migration seam

**Dependency:** F01 inventory accepted; the current native-v1 validation tests stay green.

**Model/ownership:** Sol Medium; a GUI-independent native document helper plus focused fixture tests. Coordinator owns integration into StudioController.

**Deliver:** pure format/version dispatch and transactional staging; preserve unknown state and reject unsupported future execution without overwriting bytes. Migrate data meaning first. Keep historical musical behavior changes separate from schema translations. Do not alter live graph lifecycle to implement loading.

**Acceptance:** all evidenced fixtures pass round-trip/preservation or intentional diagnostic expectations; wrong types/unsupported versions preserve the accepted document; GUI and headless callers share the same validation path. No broad legacy migration claim from a handful of synthetic fixtures.

## Task F02C: shared document inspection on open — accepted

**Owner/model:** completed by one Sol Medium worker `document_open_seam`; only native/ui/studio_controller.cpp, native/ui/CMakeLists.txt and native/ui/tests/controller_failure_tests.cpp. Root owns builds and docs.

**Deliver:** loadPatch uses the existing QtCore inspector for bounded format dispatch before existing native graph validation. Valid native v1 retains transactional loading and unknown-module preservation. Legacy/future/invalid documents receive an actionable rejection while the current accepted patch and source bytes remain unchanged. No automatic conversion, new editor workflow or engine dependency.

**Acceptance:** synthetic legacy/future/malformed/oversized file tests preserve modules, connections, name, dirty/readiness and audio-off state; valid native and unavailable-module round-trip regressions pass. All targets compiling the controller link the document helper; engine remains Qt-free. Full check --package after integration.

## Task F03: portable deployment evidence packet — local acceptance complete

**Dependency:** green current native checkpoint. Local package smoke, normal-window readiness with stripped environment and automatic integrity inventory passed in check-20261026T152345Z.json (1,407 files). Clean-machine/Pi acceptance remains unverified.

**Model/ownership:** Luna Medium; packaging inventory/reporting docs and scripts only. Do not rewrite DSP.

**Deliver:** dependency/version/hash/source-notice inventory for the locally staged Windows build, precise clean-machine test instructions, and an ARM64 headless build recipe with actual tool availability reported. Do not install a broad toolchain or publish anything merely to complete a checklist. If real Pi/clean Windows is unavailable, produce a bounded test packet and record the hardware gate.

**Acceptance:** package re-creation, isolated-path smoke and normal-window startup identify the same binaries; no SDK path assumption; actual clean-machine/Pi results remain distinct from recipes or local mocks.

## Task F04: independent module/SDK proof — accepted

**Dependency:** current gate green; document/descriptor conventions documented sufficiently for an independent author.

**Model/ownership:** narrow lower-cost implementation or regular-chat ZIP packet; one new module directory and its processor tests. Coordinator owns discovery/integration checks.

**Delivered:** Scale/Offset control utility using existing descriptor/ramp APIs. Actual DLL loading, processor tests and ADSR-to-VCA graph equivalence passed. See BUILD_CHECKPOINT.md for exact semantics and acceptance report.

**Acceptance:** independent build/discovery, saved stable identity, partition/rate/reset/guard tests, no processing allocations and no host module-ID branch. If a generic capability is missing, return a precise contract question before implementing a workaround.

## Deferred gates

Native Surge integration, equivalent kernel/runtime experiments, typed asynchronous provider proof, CHAOS/sequencer migration, MIDI/OSC/Pico, polyphony, trust/update manager, full release and Pi validation remain in the approved master plan. They are not automatically completed by the current Essentials or local portable build. Choose the next one from new evidence after the small gates above, without restarting settled architecture decisions.


## Task F05: bounded numerical reference experiment — accepted

**Owner:** kernel_reference_bench, Luna Medium. **Files:** tools/experiments/reference_kernels.py, tools/foundation/tests/test_reference_kernels.py, docs/REFERENCE_KERNEL_EXPERIMENT.md.

Independent Milestone1 preparation while Windows blocks GUI execution. CPython and existing NumPy only: two-input gain mix, linear ramp fill, integer ring delay; binary64 precision, deterministic inputs, prepared output/scratch and finite work. Missing PyPy/Numba/Cython are availability observations only, not tested candidates.

**Status: completed.** Final corrections prevent mislabeled metrics: CPU seconds per audio second at48kHz, batch-normalized wall distributions rather than true per-block latency tails, delay inputs long enough to wrap, actual executable PyPy discovery, and correctness validation before timing. The worker completed the bounded run with correctness validation before timing; all 23 foundation Python tests passed after integration. BUILD_CHECKPOINT records acceptance. No GUI/DLL/audio execution, external installation, architecture restart or language-winner claim.

After F05 acceptance, select F02 only after resolving its explicit mapping/contract questions; do not rush a lossy legacy import. Public/native deployment acceptance still needs clean-machine and hardware gates. The prior local signing-policy block was cleared by the user.

## Task F06: deterministic asynchronous-provider proof — offline acceptance complete

**Dependency:** F02C/F03/F04 accepted. Native ABI remains experimental; this is a simulated provider/control-service proof before real devices, not a general data graph.

**First deliverable:** a short contract packet covering one producer owner, absolute-frame/timebase generation conversion, bounded latest-value coalescing, ordered note/gate loss handling and explicit panic/reconciliation, late/out-of-order policy, rate limits, and deterministic shutdown. The device/controller owns engine enqueue; do not feed a nominal SPSC queue from multiple threads. Keep acquisition/parsing work outside processing and preserve audio-off tests. Use current APIs if sufficient; document a concrete gap rather than silently widening the ABI.

**Implementation acceptance after contract:** deterministic fake-clock tests for bursts, ordering, stale generations, overflow, stop and teardown; bounded storage/work; no real MIDI/OSC/serial dependency or install. Keep engine/UI independent of optional Python. One Sol Medium worker for substantial implementation; High only for an evidenced timing/ownership question after two reasonable attempts.

## Personal-use priorities after F06

Latest user steering prioritizes a solid local app and original musical plans. Future distribution remains considered, but release/marketplace work is not a near-term priority. User uses Ableton intermittently: physical audio/interface tests and performance benchmarking remain paused until cleared. `python tools/dev.py check --functional-only --package` automates the allowed functional pipeline and explicitly reports deferred timing work.

F06 was accepted in nine-suite functional checkpoint check-20261026T155150Z.json (provider proof plus LFO). It is an offline single-owner simulation, not live ingress integration. See PROVIDER_BRIDGE_CONTRACT.md for applied-reset boundary and remaining live-ownership questions.

F07 Essentials LFO is accepted: generic DLL/control graph, analytic multirate/partition/reset/guard/ramp tests. No ABI expansion. App launchers and local font/icon are installed; repoint Desktop/Start-menu links after a new accepted package.

F08 read-only native Surge feasibility preparation is complete in docs/NATIVE_SURGE_FEASIBILITY.md (Luna Medium). Inventory existing source/headers/binary boundary and specify a minimal single-voice experiment. Do not replace Surge binaries, install toolchains or begin full CHAOS migration just to fill this task. Select the implementation from actual evidence; approved plan's timing/state/quality and preservation requirements remain.

Next F09: verify official pinned Surge source/API prerequisites, then isolate a one-voice native wrapper before full CHAOS migration. Check the F08 evidence packet first. Defer heavy build/benchmark/device tasks during Ableton use; do not label the existing Python extension as a proven native engine ABI. Preserve its binary and all user presets.

## Essentials module expansion — latest user-approved scope

See ESSENTIALS_MODULE_ROADMAP.md for every requested module and actual readiness. External kit essentials_build_guide plus deliverables ZIP is current; old module_build_guide is legacy only. Six explicit source-only tasks can be delegated independently, beginning Noise/Slew; filters need deeper DSP review. Integrate returned modules here with full tests and saved identities preserved. Core-first event/pitch/voice/provider/analysis contracts must precede pretending sequencers, MIDI/OSC/UDP or Scope/Spectrum are ordinary ABI-v1 plugins. Existing local Surge task remains relevant, but this module-list request does not authorize blindly copying firmware or rebuilding the whole UI/core.

## Returned six-module acceptance — latest

Noise, Slew, Function Envelope, Clock, VCF and Ladder are integrated and passed full check-20261026T172345Z.json (27 Python/16 CTest). See SIX_ESSENTIALS_IMPORT_2026-10-26.md; do not assign their original READY tasks again. User lifted Ableton restrictions; no process was present to close. Automated audio remains off. Next: actual user feedback, bounded measured optimization of settled filter coefficients if justified, then remaining pitch/event/voice/provider or native Surge work. Demo patch is native/examples/Six_Essentials_Demo.json.
