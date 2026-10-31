<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J MIDI patchable: additive MIDI Coordinator and CC Organizer (#163)

Owner: Codex. T1 feature; first implementation-branch commit is this task file.
Extend the shipped modules so MIDI remains patchable through ordinary ABI2 wires.
The whole issue, including this document, stays within 900 changed lines/14 files.

## Authority and read first

- Issue [#163](https://github.com/kalharbi11/wirescape/issues/163), especially
  [planner decision 6530734892](https://github.com/kalharbi11/wirescape/issues/163#issuecomment-6530734892)
  and [source preflight 6540684932](https://github.com/kalharbi11/wirescape/issues/163#issuecomment-6540684932).
- Current `AGENTS.md`, then only `docs/PLAN.md` Now. The issue-specific approved
  Learn slice does not authorize the deferred Activity/Mappings service.
- `native/modules2/midi_to_cv/{dsp.h,tests.cpp}`;
  `native/modules2/midi_cc_to_cv/{config.h,dsp.h,tests.cpp}`.
- `native/io/midi/{module_test_support.h,service_tests.cpp,CMakeLists.txt}`;
  `native/control/{midi_host.h,midi_host.cpp,midi_host_tests.cpp}`.
- `native/document/{runtime_module_config.h,runtime_module_config_tests.cpp,runtime_patch.cpp,runtime_patch_tests.cpp,app_patch.cpp,app_patch_tests.cpp}`.
- `native/control/app_engine.h`; `native/engine2/engine2.h` and the
  `Engine::save_state`/`load_state` implementation in `engine2.cpp`;
  `native/ui/studio_controller.cpp` save/load and engine-quiescence paths.
- `native/engine2/{CMakeLists.txt,tests/soak.cpp}` and `docs/PI_RUNNER.md` for
  runner evidence; `tools/foundation/acceptance.py` for executable acceptance.

## Approved bounded implementation

1. Keep module IDs `midi.to_cv` and `midi.cc_to_cv`. Preserve every existing
   port/parameter ID, order, default and existing output behavior. Append the mono
   pitch output `last_note` after output index 7 (`notes`), at index 8. Keep
   pitch/gate/velocity and expression outputs on the existing J2 allocator;
   channel filtering remains authoritative in the converter.
2. Endpoint selection, saved-name/system-name identity and backend ownership stay
   with `midi.in`/MIDI Host. Reuse the single existing backend and fake backend;
   no duplicate modules, device code, new dependency or hardcoded owner hardware.
3. Extend the existing eight CC configuration rows with finite min/max endpoints,
   mapping normalized CC 0..1 into the configured range before existing slew.
   Preserve omitted-field/default behavior, bipolar settings, MIDI 1/2 conversion,
   per-source/channel CC14 pairing and the existing 0..2000 ms smoothing policy.
   Validate JSON and the typed runtime configuration; reject nonfinite endpoints.
4. Add per-slot, one-shot CC-number Learn from the next eligible incoming CC.
   Honor the slot's configured channel (0 omni, 1..16 filtered); do not learn or
   overwrite its channel. Consume the armed state once; keep unrelated slots and
   nonmatching-channel messages unaffected. Retain bounded callback work.
5. Persist learned assignments through explicit quiescent control-side writeback
   into saved module configuration. `Engine::save_state` is a quiescent operation;
   DSP serialization alone is not patch persistence. Studio currently saves its
   document configuration/parameters. Reconcile the learned controller into that
   document before saving/rebuilding, and prove save/reload restores it. Never
   read live processor state concurrently or mutate JSON from process/event.
   Preserve unrelated document data and old assignments/defaults on reload.
6. All added outputs are normal ABI2 ports, with ordinary cable behavior; no wire
   special cases, new Activity/Mappings service, backend or device authorization.
   No allocation, locks, I/O, exceptions or logging in process/event/reset.

## Contracts to settle before dependent source changes

The linked planner decision approves the additive route; it does not specify all
user-visible semantics. Record the bounded answers on the issue before inventing
behavior that triggers the AGENTS design Stop:

- `last_note`: confirm the proposal to hold the most recent accepted NoteOn
  pitch through release, reset/start at 0, and exclude bend; also settle source
  choke behavior. These proposed semantics are not established by the decision.
- Min/max: confirm the proposal to allow inverted finite endpoints and settle
  partial endpoint overrides with bipolar defaults. Only finite validation is
  specified by the preflight.
- Learn: exact arm/cancel control and stable additive parameter/config encoding;
  eligibility in CC14 mode; transient arm state versus saved assignments.
- Writeback: confirm quiescent capture before reset, merging learned controllers
  into document config while preserving unknown keys; settle the exact existing
  control operation and failure handling. This is a proposed integration, not an
  existing state/UI contract. No concurrent inspection or new service is implied.

## Verification and missing soak fixture

Extend the existing executable suites rather than adding prose test aliases:
`abi2_midi_service_tests` supplies fake acquisition, `abi2_midi_to_cv_tests`
already tests ordinary engine routing and note/voice mapping, and both converter
suites use the real SDK wrapper. `MidiRig::run` watches allocations around wrapper
process (including event dispatch); the fake service also watches its callback.
The commands below are existing baseline entry points, not evidence that the new
features pass. Extend them before claiming acceptance. New Learn/persistence and
coordinator-poly soak coverage is pending implementation.
Cover new Learn/range/slew behavior, channel rejection, stable old output indices,
mono last-note simultaneous with poly outputs, and learned assignment JSON/patch
save/reload. Include malformed/nonfinite settings and old-config compatibility.
Use relevant document/Studio tests for the actual persistence path, not just a
module state-blob roundtrip. Qt-backed tests must be present when that path changes.

The current `wirescape_soak --graph poly8` constructs VCO/VCF/ADSR/VCA without MIDI;
it is not coordinator evidence. No #163 MIDI poly 60-second fixture/command exists
at this base. Implementation must add bounded synthetic/fake MIDI stimulation of
`midi.in -> midi.to_cv -> poly voice` and ordinary CC wiring, then record its exact
executable 60-second command and runner invocation here before claiming acceptance.
Keep the Pi requirement pending until the private runner publishes exact-head ABI2
and 60-second coordinator-poly soak evidence. Never open devices or SSH to the Pi.

Build first using the existing headless command below; add a Qt-enabled build and
focused existing Studio/document suite when required for writeback. Run
`python tools/dev.py accept 163` after implementation; external entries need their
linked evidence even when the local report succeeds. No source checks were run
merely to create this task file.

```acceptance
tier: T1
size: <= 900 changed lines, <= 14 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -C Release -R "^(abi2_midi_.*_tests|midi_host_tests|runtime_module_config_tests|abi2_runtime_patch_tests)$" --no-tests=error --output-on-failure
  - ctest --test-dir build/native -C Release -R "^abi2_midi_(to_cv|cc_to_cv|service)_tests$" --no-tests=error --output-on-failure
  - ci: windows-x64, linux-arm64, sanitizers green on the exact head
  - pi: ABI2 tests and a 60 s soak with midi.to_cv coordinator in a poly patch; executable fixture and exact command required before acceptance
stops: [ABI header edit, new dependency, ID change, real device access, unspecified design choice, size budget exceeded]
```
