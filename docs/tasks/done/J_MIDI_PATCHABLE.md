<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J MIDI patchable: additive MIDI Coordinator and CC Organizer (#163)

Owner: Codex. T1 feature; first implementation-branch commit is this task file. Keel's #290 ruling (6706057275) supersedes the original DSP Learn/writeback scope.
Extend the shipped modules so MIDI remains patchable through ordinary ABI2 wires.
The whole issue, including this document, stays within 900 changed lines/15 files.

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
4. Keep DSP Learn and engine-driven document writeback **out of scope**. Keel
   assigned transient MIDI observation to #381 and knob-to-D8 document mapping
   to #287. The UI remains the only owner of the editable patch (PLAN D3);
   frozen Qt must not be altered (PLAN D11).

## Settled Keel contract (#290 comment 6706057275, 2026-11-07)

- `last_note` is mono index 8, raw V/oct of the latest accepted NoteOn,
  held through NoteOff and source choke. It ignores bend and polyphony,
  initializes to 0 and resets/restore to 0.
- Finite optional JSON row `min` / `max` keys override the matching end
  of the bipolar defaults (0..1 or -1..1); either may be absent. Inverted
  ranges and equal endpoints are valid; non-numbers and non-finite values
  refuse at load without mutating the graph. Keep `bipolar` in the patch.
- Bump typed native CC config to v2, accept legacy v1 bytes with bipolar
  defaults, and reset each CV row to its effective minimum. JSON alone
  persists the mapping. No new ABI, Learn parameter, DSP save writeback
  protocol, or Qt changes.

## Verification and synthetic MIDI soak fixture

Extend the existing executable suites rather than adding prose test aliases:
`abi2_midi_service_tests` supplies fake acquisition, `abi2_midi_to_cv_tests`
already tests ordinary engine routing and note/voice mapping, and both converter
suites use the real SDK wrapper. `MidiRig::run` watches allocations around wrapper
process (including event dispatch); the fake service also watches its callback.
The commands below are existing baseline entry points, not evidence that the new
features pass. Extend them before claiming acceptance. New last-note/range/save-reload coverage and coordinator-poly soak verification remain required.
Cover min/max/range/slew behavior, channel rejection, stable old output indices,
mono last-note alongside poly outputs, JSON patch save/reload, malformed/nonfinite
settings and typed v1 compatibility. Keep the existing wrapper allocation checks.
No DSP Learn/writeback or frozen Qt source tests.

The current `wirescape_soak --graph poly8` constructs VCO/VCF/ADSR/VCA without MIDI;
it is not coordinator evidence. `abi2_midi_to_cv_tests` now contains a one-second
accelerated regression using synthetic immutable source IDs through
`midi.in -> midi.to_cv -> VCO/ADSR/VCA -> Output`, with eight voices. A second
`midi.in -> midi.cc_to_cv -> VCA gain_cv` cable controls the poly audio. Each cycle
starts eight notes, chokes one source, and releases the other; the final block
chokes all sources. Assertions check held-gate counts, CC values, finite/nonzero
audio, paired attacks/releases, zero ingress/render allocations, faults and drops.
The same fixture runs paced for 60 seconds with:

```sh
build/native/abi2_midi_to_cv_tests --soak
```

Request it through the private runner using the `pi-run:` acceptance entry below
and `ci/pi` + `ci/pi-accept` on draft #210; inspect `pi5/accept` on the exact head.
The fixture covers the existing coordinator and ordinary CC routing, not the
unimplemented last-note/range/Learn features. It uses synthetic events, not a
physical or fake acquisition backend; existing service tests cover fake acquisition.
Keep the Pi requirement pending until the private runner publishes exact-head ABI2
and 60-second coordinator-poly evidence. Never open devices or SSH to the Pi.

Keel's binding ruling is [#290 comment 6706057275](https://github.com/kalharbi11/wirescape/issues/290#issuecomment-6706057275).
#287 alone owns UI Learn; its T2 observation prerequisite is #381. Do not
infer CC identity from `status.midi.activity` or invent DSP writeback.
The 60-second fixture at d715bb4 is already present, with exact-head Pi evidence
only for that historical head; new feature heads require new acceptance evidence.

Build first using the existing headless command below. Qt remains frozen under
PLAN D11; resolve writeback ownership before selecting its integration tests. Run
`python tools/dev.py accept 163` after implementation; external entries need their
linked evidence even when the local report succeeds. No source checks were run
merely to create this task file.

```acceptance
tier: T1
size: <= 900 changed lines, <= 15 files
must_pass:
  - python tools/gen_ui_catalogue.py --check
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -C Release -R "^(abi2_midi_.*_tests|midi_host_tests|runtime_module_config_tests|abi2_runtime_patch_tests)$" --no-tests=error --output-on-failure
  - ctest --test-dir build/native -C Release -R "^abi2_midi_(to_cv|cc_to_cv|service)_tests$" --no-tests=error --output-on-failure
  - ci: windows-x64, linux-arm64, sanitizers green on the exact head
  - pi: ABI2 tests and a 60 s soak with midi.to_cv coordinator in a poly patch; executable fixture and exact command required before acceptance
  - pi-run: build/native/abi2_midi_to_cv_tests --soak
stops: [ABI header edit, new dependency, ID change, real device access, unspecified design choice, size budget exceeded]
```
