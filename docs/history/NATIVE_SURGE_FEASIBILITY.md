# Native Surge feasibility note

**Status:** bounded local inventory only; no native implementation, research, download, build, or execution performed. The question here is whether existing local assets establish a direct native integration path.

## Findings

- `modules/chaos/engine.py:33-47` creates **two** Surge objects through `surgepy.createSurge(sr)`. The class docstring says calls occur from `render()`; UI-facing changes enter a Python queue.
- `modules/chaos/engine.py:101-120` discovers parameters at runtime from control groups, entries, scenes, names, and IDs. `:150-170` applies normalized raw parameter values using runtime min/max/type metadata. Macro targets use named scene/name pairs (`:204-216`, `macros.py`).
- A one-voice render requires oscillator/control initialization, event handling, and block rendering: note-on is velocity 100 and note-off routes to `playNote`/`releaseNote` (`engine.py:251-255`); the default engine quantum is 512 frames at 48 kHz (`:27-28`); each Surge block size is queried (`:35`) and the instance's `createMultiBlock`/`processMultiBlock` path is used (`:402-411`). CHAOS then reads stereo float output and applies its own mixer/effects. The proof can stop before those CHAOS effects.
- Existing preset files are parameter-state JSON, not embedded Surge patch binaries. The README says KEEP stores parameter values; `ui.py:120-156` loads macros and raw overrides (plus B recipe, LFO shape/target). For one voice, useful preservation covers macro defaults/targets, voice A raw overrides, stable scene/name parameter addressing, and preset round-trip. Sequencer state and full CHAOS topology are outside this experiment.
- The only Surge binding artifact found in the named integration area is `modules/chaos/surgepy_bin/surgepy.cp312-win_amd64.pyd` (7,748,096 bytes). `modules/chaos/surge_path.py` searches that directory and optional ignored `modules/chaos/surge/...` build folders for `surgepy*.pyd`/`.so`.
- No Surge source tree exists at `modules/chaos/surge/`. The bounded `modules/chaos/surgepy_bin/` listing contains only that `.pyd`. `.tools/` top level contains `bootstrap`, `fonts`, and `qt`; no Surge source/header/library was identified in those locations. `rg --files` in the bounded relevant paths found only `surge_path.py` and the `.pyd` as Surge-named assets.
- The `.pyd` is a CPython 3.12 Windows extension. Its presence proves a Python binding is available to that interpreter, not a callable C++ ABI, nor that the DLL exports Surge's engine API. No Surge C++ headers, import/static library, CMake project, or native integration example were found in the bounded inventory.

## Requirements for a minimal one-voice proof

The proof should own one pinned Surge revision natively and expose only a small wrapper boundary:

1. Create/reset one synth at a declared sample rate; load a known saved preset or an explicit documented default patch.
2. Resolve and set a small stable parameter set, including one ordinary float and one enum/integer parameter. Confirm scene/name or ID mapping against the actual pinned revision; do not rely on Python-generated names as a native ABI.
3. Accept ordered note-on, note-off, and panic/reset events, render stereo blocks at the engine-supported quantum, and return finite samples. Establish supported block granularity and whether controls/events apply only at block boundaries.
4. Save and reload the selected preset/parameter state, then render a deterministic note and compare output to the existing Python binding at matched revision, rate, preset, events, and render quantum. Define a numerical tolerance before comparing; also verify the parameter values after load.
5. Measure wrapper/render time for one voice against `surgepy` under identical conditions. Treat any speed result as local to this workload; the foundation plan's broader matched-quality benchmark remains separate.

Acceptance: native build links against the pinned Surge source without importing Python; preset and parameter round-trip preserve all selected fields; event ordering and note release/panic behave as specified; output is finite and meets the predeclared render tolerance; timings and build provenance are recorded. A failed source/API availability check should stop the experiment before changing CHAOS.

## Preserve versus redesign

Preserve the existing `.pyd`, preset JSON files, parameter catalog, source macros/raw ownership semantics, and `user_data/` unchanged. This experiment is an isolated one-voice feasibility branch/proof, not a rewrite of `engine.py`, a dual-instance migration, or a new ABI commitment. Keep original musical behavior as the comparison reference. Any changed synthesis behavior requires explicit versioning and listening review under the approved foundation plan.

## Unknowns and next experiment

Unknown locally: exact upstream Surge revision associated with the binary, source availability, C++ API/export/link model, licenses/build prerequisites for the intended distribution, native preset load/save entry points, event precision, block-size limits, cross-platform/ARM build viability, and comparative performance. Do not infer any of these from the `.pyd` filename or inspect/execute the binary as a substitute for source/API evidence.

Next: obtain an authorized, pinned Surge source checkout in a separate working location; inspect its public headers/build targets and licensing; then compile a tiny headless one-instance wrapper and run only offline preset, parameter, event, block, and output-equivalence checks. Do not touch the preserved Python binary or user data. This local stage says nothing about physical audio, hardware workflows, Pi readiness, broad benchmarks, or future publication suitability.
