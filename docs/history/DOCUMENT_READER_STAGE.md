# Document inspection and patch-open boundary

Updated October 26, 2026. F02A pure inspection is runtime-verified; full checkpoint `check-20261026T152345Z.json` passed all seven CTest suites, including document_reader_tests. The historical Windows policy block was cleared by the user; agents did not change security settings.

## Accepted F02A contract

`native/document/document_reader.h/.cpp` provides QtCore-only `inspect(QByteArray)`, outside the Qt-free engine. It classifies native v1, evidenced legacy patch v1/v2/v3 and visual-study v4, unknown format, unsupported version and invalid input. It bounds parsing at 2 MiB, retains original bytes in every result, retains parsed opaque objects where possible, and inventories sequence/module_state independently (including absent versus explicit null).

Structural inspection does not accept a playable graph: descriptor/parameter values, coordinates, ports, resource limits and topology still require controller validation. Unknown module state is opaque. No algorithms, units, positions, state or cable semantics are translated. Exact source bytes preserve duplicate JSON keys and numeric spelling; Qt's parsed object does not.

`native/tests/document_reader_tests.cpp` covers six synthetic F01 fixtures, unknown native state, malformed/type/duplicate-module cases, future versions, conflicting legacy state fields, byte limits and preservation. Native engine linking remains independent of Qt. CMake omits this helper/test when QtCore is unavailable.

## Accepted F02C controller integration

StudioController::loadPatch now uses the shared inspector after a bounded file read, before native graph validation and transactional graph staging. Known legacy formats receive an explicit migration/execution rejection; unknown/future/malformed/oversized inputs receive inspection diagnostics. No rejected source file is written.

Expanded controller regression tests preserve modules, connections, name, output, dirty/readiness, audio-off state and exact source bytes across rejection. Valid native load and unknown-module opaque state/cable round-trips pass. All three controller targets link the QtCore helper; the engine stays Qt-free. Final check-20261026T152345Z.json passed 26 Python tests, seven CTest suites and the complete audio-off UI/startup/benchmark/render/package pipeline. One Sol Medium worker implemented, coordinator reviewed and verified. No higher-effort review was required.

## Migration remains separate

[LEGACY_MIGRATION_MAP.md](LEGACY_MIGRATION_MAP.md) records why current Essentials cannot provide sound-equivalent substitutions for missing stereo/event/Surge and sequencer paths. F02A and F02C do not implement migration. No automatic legacy-to-native execution should be inferred from recognizing a file's structure.
