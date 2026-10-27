# Current review handoff — feature work wrapped

October26,2026. Start at ../START_HERE_CURRENT_BUILD.md. Consolidated current documents are CURRENT_BUILD.md, CURRENT_EVIDENCE.md and CURRENT_PLAN.md; these supersede historical handoffs/status fragments below. User requested wrap/review preparation, not further module implementation. No active worker or running audio session.

Manual explicit audio probe compiled and completed a3s Realtek/MME session with555 blocks/nonzero peak/no reported faults or underruns, clean stop; no output WAV/capture or listening feedback. Report audio-session-six-20261026.json. No-confirm guard exits2 before device construction. Latest offline regression test-20261026T184344Z.json passed16 CTest suites after probe addition. Last full app/package checkpoint remains check-20261026T172345Z.json. Future unattended output capture/analysis is required but NOT implemented. MIDI/OSC/UDP and sequencer/chord/polyphony work has not started. Preserve the working state for Claude review.

---

# Latest accepted stage: six returned Essentials integrated

October 26, 2026. See docs/SIX_ESSENTIALS_IMPORT_2026-10-26.md and .json for source hashes, precise changes and evidence. All six requested returned modules (Noise, Slew, Function Envelope, Clock, VCF12/24, Ladder) are installed under native/modules and packaged. Existing modules/user presets/Surge remain intact. Full `check-20261026T172345Z.json` passed: 27 Python tests, 16 CTest suites, both UI sizes/startup/render/45 baseline benchmark cases/package integrity. Package wirescape-release-20261026T172338Z contains 13 Essentials DLLs and 1,421 matching files. Desktop/Start-menu launchers updated; private font retained.

The user explicitly lifted Ableton restrictions and allowed normal close if needed. No Ableton process was found; none was closed. Future performance work is allowed again. Automated tests still keep audio off; user starts physical audio. Earlier pause statements below are historical.

One focused Sol High filter algorithm review and one Luna Medium filter-test worker, coordinator integration/final validation. Added max-resonance quiet-tail and short-span automation tests, app palette/wiring/parameters/save/load/deterministic-render checks, and native/examples/Six_Essentials_Demo.json. Filter source algorithms were not changed; six entry wrappers gained the existing static entry-name override hook. No new engine ABI or platform-specific DSP dependency. No active worker. Shared account telemetry at completion: 95% of five-hour allowance and30% of weekly allowance used; account-wide, not per-task totals. Finish this accepted checkpoint rather than starting another large stage.

Next useful work: user listening/interaction feedback on new modules, followed by measured filter coefficient caching if worthwhile; then remaining core pitch/event/provider/voice contracts or isolated native Surge proof per the approved plan. Do not claim analog equivalence, real-time capacity for all new modules, polyphony or Daisy/Pi support. Frozen authoring kit assignments are now completed; consult updated ESSENTIALS_MODULE_ROADMAP.md to avoid duplicating them.

---

## Latest architecture clarification: embedded reuse

User wants future agents to integrate saved Wirescape patches/modules into Daisy Seed or other hardware projects with minimal DSP changes; Wirescape need not implement an exporter or firmware builder. See docs/EMBEDDED_PORTABILITY.md and updated kit addendum. No current hardware support is claimed. DSP/UI/provider/host separation is required; current desktop engine has OS dynamic loading and lock-free64-bit atomics that prevent claiming bare-metal drop-in compatibility. Future modules should expose reusable C++ DSP, explicit state/buffer/resource needs and thin adapters without forcing existing ABI/code changes. This stage changes documentation and the share ZIP only; no firmware, audio, dependency installation or processing-code change.

# Latest stage: Essentials inventory and external-agent kit

October 26, 2026. User requested a complete module audit and an updated shareable source kit, not implementation of all missing modules in this stage. Product built-ins are named **Essentials**; implementation directory/API IDs/patch format remain stable.

- Full requested-module matrix: docs/ESSENTIALS_MODULE_ROADMAP.md. Built requested modules: ADSR (host event-gated), two-input mono Mixer, Scale/Offset (attenuverter/offset). VCA exists without LPG. All other requested modules are missing in the current Essentials build; legacy Python sequencer does not count as a migrated Essentials sequencer.
- Six explicit external tasks: Noise, linear Slew, gated Function Envelope, CONTROL-pulse Clock, VCF12/24 and Ladder (filters require deeper review). Specs reserve IDs/ports/units/defaults and meaningful tests. MIDI/OSC/UDP, event sequencers/chord+strum/polyphony, pitch/Quantizer contract, monitors/analysis UI and Plaits/Braids port remain local core-first work. Prototype kernels may later be delegated after contracts are fixed.
- Current self-contained packet: essentials_build_guide/ and deliverables/Wirescape_Essentials_Agent_Kit_2026-10-26.zip. Includes frozen actual headers/engine/reference sources, source-only example, CMake, bounded check runner, structural validator, six task specs, all-module status, copyable agent prompt and manual source return/import guide. No fonts/user data/Surge/compiled binaries. Old module_build_guide is explicitly marked legacy Python/browser.
- One Luna Medium worker audited and built scaffold; coordinator wrote contracts/specs, strengthened the teaching tests, audited packaging and verified a freshly extracted ZIP. No high-effort review was needed for this documentation/scaffold stage. Actual kit C++/dynamic-loader CTest and four verifier tests pass. No Linux/Pi claim. Main app check-20261026T163643Z.json passed 27 Python tests/nine CTest suites/UI/startup/render/package; timing benchmark deferred. Audio stayed off; Ableton restriction remains.
- Corrected LFO and Scale/Offset manifest metadata to map names to their actual parameter IDs, matching other modules. No DSP/default/ABI/saved-ID change. App palette/footer now say Essentials. Updated launchers should target latest accepted package after this check.

Next: user uploads the kit to one regular ChatGPT agent per READY task (start Noise, then Slew), saves returned source folders under incoming_modules for review, and Codex validates/builds/integrates them. Do not blindly overwrite existing modules or treat source copy as installation. No missing requested module was silently built by this packet.

---

# Current continuation — personal-use foundation

Latest user steering: prioritize a solid local app covering the original plans. Future sharing is considered in boundaries, but release/marketplace/distribution work is not the priority. User is using Ableton intermittently for several hours: physical audio/interface work remains paused, and performance benchmarks are deferred to avoid contention/misleading results. Do not resume hardware tests until user says Ableton use is finished. Offline functional tests are allowed.

## Accepted October 26 continuation

- F06 offline provider proof: `native/control/provider_bridge.*`, actual-engine tests and `docs/PROVIDER_BRIDGE_CONTRACT.md`. One Sol Medium implementation worker, narrow Sol High planning/final risk review; no blockers in reviewed ownership/timing scope. Exactly one serialized enqueue owner, fixed storage/flush examination budget, timestamp checks, latest-value coalescing, late/release-loss panic, explicit active render barrier before rebase. NOT connected to the live UI, NOT MPSC ingress or verified live concurrency. Engine ABI unchanged.
- Static Studio reference restoration: Luna Medium worker refined original card/port/button details; root removed missing-glyph symbols. Native Qt only; no decorative blur/shadow/continuous animation. Two-size UI checks passed, with smallest layout visually inspected. No performance improvement or equivalence claim while Ableton runs.
- Black circle/white outline logo generated deterministically in SVG, PNG and multi-size ICO under native/ui/assets; Windows exe and Qt window icon embedded. Root created Desktop and Start-menu Wirescape.lnk, pointing to package `build/packages/wirescape-release-20261026T154700Z`. Search Windows for Wirescape. Shortcut installer: tools/install_windows_shortcuts.ps1; repoint both after a newer accepted package, retain the targeted package until then.
- User supplied ABC Areal for personal local app use. Private font+terms extracted only to ignored `.tools/fonts/`; no system font install, Git copy or package embedding. `--local-font PATH` loads at startup; shortcuts pass it. Actual readiness confirmed family `ABC Areal Variable`, audio false and native Windows window ready. Local-font 1000x650 smoke and visual inspection passed. No claim of future embedding/distribution rights.

Validation: `test-20261026T154639Z.json` passed **eight CTest suites**; all **26 Python helper tests** passed; `ui-test-20261026T154528Z.json` passed both sizes; final `package-20261026T154706Z.json` passed offline smoke, normal-window readiness and SHA inventory. `local-personal-app-check.json` confirms actual packaged font/readiness/smoke. One interim build correctly failed source-stability while worker edits overlapped; finalized source rerun passed. No physical stream was opened. Full check --package/performance benchmarks deliberately not rerun during Ableton use; do not label focused functional validation as a fresh full benchmark checkpoint.

## Accepted next stage: Essentials LFO and functional-only runner

`python tools/dev.py check --functional-only --package` passed: `check-20261026T155150Z.json` and matching Markdown. **27 Python tests, nine CTest suites**, both UI sizes, normal startup, offline render and package checks passed. Benchmark is explicitly **deferred**, with no performance claim. Shared account telemetry earlier this stage was 61% five-hour /25% weekly used; account-wide only.

New ordinary Essentials module `essentials.lfo` has no inputs and a sine CONTROL output: rate .01..20Hz/default1, depth0..1/default1, offset-1..1/default0, raw output without clipping. Shared 5ms ramps, per-sample phase progression, phase0 reset. Tests cover analytic phase at three rates, exact partition behavior, reset/guards/ramps and generic DLL LFO-to-VCA graph. One Luna Medium worker; coordinator corrected analytic test expectations before build, integrated CMake and validated. No engine ABI or host module-ID branch.

One independent Luna worker added `check --functional-only`, with a mocked regression proving the default benchmark still runs and the flagged run truly defers it in JSON/Markdown. The whole functional pipeline then passed. Desktop/Start-menu links now point to accepted package `build/packages/wirescape-release-20261026T155143Z`, with private font argument retained. Prior package references above are historical.

## Accepted read-only preparation and next task

F08 local Surge inventory is saved in docs/NATIVE_SURGE_FEASIBILITY.md (one Luna Medium worker, no binary execution or downloads). Relevant checked paths contain the CPython Surge extension, but no direct native source/header/import-library build setup. The old benchmark's reported revision is historical reference evidence, not verified provenance for an independently built native wrapper.

No worker remains active. Accepted implementation recovery commits: `534890e` (F06/design/icon/local launch) and `c54f0a0` (LFO/functional-only runner). The following docs commit completes this handoff. Desktop/Start-menu launch the latest accepted package with audio off and optional private font. Start-menu indexing itself was not inspected through Windows Search UI; shortcut targets were verified via Windows shell API.

Next important work: verify the official/pinned Surge source and headers, then build an isolated one-voice native wrapper preserving selected parameter/preset/event behavior. Read NATIVE_SURGE_FEASIBILITY.md first; do not replace the preserved binding or redesign all CHAOS at once. Heavy compilation, performance experiments and physical interface work remain deferred during the user's Ableton sessions. A small real sequencer/control-source integration follows only after defining single-producer ingress/reset ownership; F06 is not a live provider implementation.

The current foundation is useful but incomplete: native CHAOS, full sequencer/polyphony, playable legacy migration, real MIDI/OSC ingress, sustained device sessions and Pi remain unverified/unimplemented. Personal reliability and original musical scope have priority over distribution machinery. No additional automatic follow-up has been scheduled.

---

# Wirescape current build checkpoint

Updated October 26, 2026. This file records current accepted evidence; [earlier checkpoint history](history/BUILD_CHECKPOINT_2026-10-26_BEFORE_F04.md) retains the investigation and prior stages.

## Accepted stage: F04 Scale/Offset

`python tools/dev.py check --package` passed: `build_reports/foundation/check-20261026T151447Z.json` and matching Markdown. All **23 Python tests and seven CTest suites** passed, both UI sizes (1400x900 and 1000x650), normal self-closing startup, 45 offline benchmark cases, offline WAV rendering and local portable packaging. Audio stayed off throughout automated checks.

Scale/Offset is a normal native Essentials plugin: `essentials.scale_offset`, one CONTROL input/output, `output = input * scale + offset`, unpatched input zero, no output clipping. Scale ranges -2..2 (default 1), Offset -1..1 (default 0). Both use the existing 5 ms sample-count ramp; initial/reset values snap. Processing allocates nothing and takes no locks by code inspection. Tests cover guards, negative/unclipped results, reset, three sample rates, partition equivalence, actual DLL discovery, and ADSR-to-VCA graph behavior. Default settings render sample-identically to the direct envelope path. No host module-ID branch or ABI expansion was added.

Model assignment: one Luna Medium worker implemented module/tests; coordinator integrated CMake, checked the contract, corrected a test descriptor member name (`type` to `kind`), and ran acceptance. No higher-effort escalation. Shared account telemetry at this milestone: 33% five-hour allowance used, 21% weekly allowance used; these are account-wide, not task costs.

## Accepted stage: F03 local portable acceptance

`python tools/dev.py check --package` passed again: `build_reports/foundation/check-20261026T151805Z.json`. All **26 Python tests and seven CTest suites**, both UI sizes, startup, 45 offline benchmark cases, WAV render and package passed. The packaged app reached normal-window readiness with SDK paths and inherited Qt/audio overrides removed; the offline smoke explicitly confirmed audio off. The process is instructed to self-close; the readiness probe establishes readiness, not sustained operation or clean shutdown under arbitrary faults.

Package: `build/packages/wirescape-release-20261026T151758Z`. All **1,407 files** matched its manifest; no missing/unexpected/hash-mismatched entries. Scale/Offset DLL and metadata are included. The inventory report is saved outside the package, avoiding self-generated drift. Binary provenance now excludes project/archive/build-state files and includes the new module and document test executables. One Luna Medium worker changed tooling/tests; coordinator inspected the diff and ran full acceptance. No new downloads or audio streams.

## Accepted stage: F02C shared document inspection

Final `python tools/dev.py check --package` passed: `build_reports/foundation/check-20261026T152345Z.json` and matching Markdown. All **26 Python tests and seven CTest suites** passed, both UI sizes, normal startup, 45 offline benchmark cases, WAV render and the strengthened portable package checks. Audio stayed off.

Patch opening now performs bounded file reads and uses the pure document inspector before native graph validation. Known legacy documents receive an explicit migration/execution explanation; unknown/future/malformed/oversized inputs are rejected with diagnostics. Tests verify accepted modules, connections, name, output, dirty/readiness and audio-off state survive rejection; source bytes remain exact. Valid native loading and unknown-module/cable/state round-trip tests pass. No source document is rewritten, no conversion is attempted, and the headless engine gains no Qt dependency.

Model assignment: one Sol Medium worker owned controller/CMake/failure tests; coordinator reviewed and ran acceptance. No High/Ultra/Max escalation or external installation. Latest shared account telemetry: 42% five-hour allowance used, 22% weekly allowance used (account-wide, not task token costs). F03 recovery commit is `bfa0e7a`; F04 is `64ffbc0`; the following commit captures F02C.

The read-only [ARM64 evidence packet](ARM64_HEADLESS_VALIDATION.md) is complete: CMake and a WSL launcher exist; no PATH clang/Linux ARM64 compiler or inspected MSVC ARM64 compiler was found. WSL was not launched. No target build/hardware claim.

## Next bounded task

No worker remains active. Current local acceptance is green. The next implementation packet should prove the asynchronous-provider boundary with a deterministic simulated source before adding MIDI/OSC/hardware. First specify timestamp conversion/generations, continuous-value coalescing, ordered event overflow/panic behavior, producer ownership and shutdown. Use the existing bounded engine entry points where possible; no JSON, Python, locks, allocation or new ABI structures in processing merely to fit a provider. A measured, demonstrated contract gap warrants a focused architecture review. Do not start CHAOS migration or freeze the SDK before this proof.

Keep F02's remaining legacy mapping/SDK scope separate: recognizing/rejecting formats is complete here; playable legacy migration and a recovery/import UI are not. Clean-Windows and real Pi/device checks require their actual environments.

## Foundation state and remaining gates

- User turned Smart App Control off and authorized continuing. Read-only policy state confirmed 0. Agents did not change security settings. No downloads/installations were needed. Investigate actual security flags; keep normal protection enabled. Historical policy blocks are resolved for these local checks.
- F01 synthetic historical fixtures and provenance are accepted. F02A pure QtCore document inspection/preservation helper is runtime-verified outside the Qt-free engine. It does not import legacy musical behavior. F02B mapping evidence intentionally finds no sound-equivalent Essentials substitution for CHAOS/sequencer/legacy modules.
- F05 CPython/NumPy reference experiment is accepted as bounded preparation, with correctness checks and honest batch-normalized timing labels. Other runtimes/native Surge comparisons are not completed.
- Milestone 0 preservation is established. Milestone 1 native proof/local package is established, with direct Surge, ARM64/Pi and clean-Windows gates remaining. Milestone 2 document/SDK safety is partial. Milestones 3-5 event/provider/sequencer/CHAOS/device/release work remain.
- Native ABI remains experimental. Current Essentials: sine VCO, ADSR, VCA, Mixer, Output and Scale/Offset. No verified Pi support, polyphony, full migration, SDK freeze or release claim.
- User previously verified live audio/meter movement and Mixer. Automated tests never start physical audio. Sustained hardware sessions and clean-machine deployment remain separate gates.
- Preserve baseline tag `baseline/0.5.2-local`, `user_data/`, Surge binaries and current edits. F04 rollback commit: `64ffbc0`; the following commit captures F03 and this checkpoint. Nothing is pushed.
- The one-time six-hour follow-up fired and was removed; no automation remains scheduled.

Read [NEXT_TASK_QUEUE.md](NEXT_TASK_QUEUE.md), the approved [foundation plan](WIRESCAPE_REVISED_FOUNDATION_PLAN.md), and AGENTS.md for bounded continuation. Use `python tools/dev.py check --package`; keep failures truthful and stop repeated retries after two reasonable attempts on a difficult failure.


## ABI2 isolated foundation — October26,2026; STOP for Claude review

Implementation checkpoint `34b948e1d1404e6108ea3ceeec3ee951959e1c4a`. Read [FOUNDATION_SLICE_REVIEW.md](FOUNDATION_SLICE_REVIEW.md) and [CLAUDE_CODEX_COMMUNICATION.md](CLAUDE_CODEX_COMMUNICATION.md) before continuing. This entry supersedes the older “Next bounded task” above for ABI-affecting work. Current app remains ABI1; isolated engine2/SDK/voice/five ports are offline-only.

`python tools/dev.py check --package` passed: build_reports/foundation/check-20261026T230043Z.json (27 Python,28 CTest,UI/startup/offline benchmarks/render/package). ABI2 evidence collector and Debug CRT pass: build_reports/abi2/summary.json,12 isolated CTest suites,135 matrix configurations, each mono port max error0, raw steady/scripted legacy demo bit-identical, three→eight-note synth with stealing/release and zero counted render allocations. Protected92 source hashes unchanged. No physical audio opened.

Performance remains a failed target: latest mean ABI2 mono/ABI1=1.890×, poly8/eight ABI2 mono=0.913×. ARM, real drivers, MPE, full transport events, UI/patch migration and Track C unverified/unimplemented. Release overflow uses reserved capacity + emergency global reset/generation ack; latency and event accounting semantics need review. Allocation hooks cover C++ allocation, not arbitrary third-party malloc.

Model assignments: inherited coordinator; one Sol Medium worker for allocator/guide and later one Luna Medium worker for documentation, never simultaneously. No High/Ultra/Max. No per-task quota totals available. Code and reference WAVs are committed; final review documentation follows. All current workers completed; unused pending historical worker stopped. Next action is **manual Claude review through the user**, not implementation.
