# Native foundation workflow

The native engine work is an isolated foundation alongside the existing Python editor. Run the commands below from the Wirescape project folder with Python 3.12. The runner uses Python's standard library and the installed CMake/toolchain; it does not install packages, change global settings, start audio, or play rendered files.

## Commands

```powershell
python tools/dev.py doctor
python tools/dev.py snapshot
python tools/dev.py build --config Release
python tools/dev.py build --config Release --headless
python tools/dev.py test --config Release
python tools/dev.py benchmark --config Release
python tools/dev.py render --config Release
python tools/dev.py launch --config Release
python tools/dev.py ui-test --config Release
python tools/dev.py report
```

`doctor` records Python, CMake, generator, compiler architecture, and whether the native CMake project is present. On Windows it looks for the latest Visual Studio installation with the C++ build tools and uses its matching CMake generator. On Unix-like systems it prefers Ninja, then Unix Makefiles. It does not install or activate a compiler.

`build` configures `native/` in `build/native/` and builds the selected configuration. CMake's cache is retained for incremental builds. If a Qt SDK is present under `.tools/qt/`, its prefix is passed to CMake so the optional desktop target can be found; use `--qt-prefix PATH` to select a different SDK. `--headless` disables the optional UI target. Headless builds do not require Qt. `test` ensures that configuration is built and then invokes CTest with failure output. The default configuration is `Release`; use `--config Debug` when needed.

`benchmark` runs the native offline benchmark and saves its machine-readable result with the run metadata. Its default workload is bounded to 256 blocks; pass `--blocks N` to choose another size. The report keeps the benchmark's per-scenario timing percentiles and deadline counters when the CLI supplies them. This is an offline measurement, not a claim of glitch-free playback on a particular audio device.

`render` asks the native CLI to render a demo WAV offline. The default destination is a new timestamped file under `build_reports/foundation/`; `--output PATH` selects another destination. Existing output files are never overwritten. The runner does not play the WAV.

`launch` starts the built `wirescape_studio` app and uses the Qt SDK recorded by the successful build and adds its runtime folders to its process environment. It does not start audio. `ui-test` runs the app with Qt's offscreen/software backends, asks it to save a screenshot and structured smoke report, and captures the full log. It never opens an audio device.

`report` summarizes recent command records. Add `--json` to any command to print its full structured record to the console; JSON records and complete command logs are saved under `build_reports/foundation/` either way. Each record includes the source commit when Git is available, Python/toolchain details, selected configuration, status, and command result. A skipped action is recorded separately from a pass or failure.

## Protect local data before changes

```powershell
python tools/dev.py snapshot --label before-native-update
```

The snapshot includes only `user_data/` and `modules/chaos/surgepy_bin/`, when present. It is written as a new ZIP under `local_backups/` with a sidecar manifest containing SHA-256 hashes for every included file and the archive. A name collision stops the operation rather than replacing a previous backup. The snapshot excludes the virtual environments, build outputs, and Git metadata.

## Report locations and interpretation

- Build output and CMake cache: `build/native/`
- Run records and raw subprocess logs: `build_reports/foundation/`
- Private-data snapshots: `local_backups/`

Reports describe only commands that actually ran. Missing CMake files, executables, or test tools are recorded as skipped or pending. The native foundation workflow does not establish Raspberry Pi support, live audio performance, audio-device availability, or physical hardware behavior. Those require separate measurements on the target system.

## Hardened checks and local packaging

Each run now records a SHA-256 fingerprint of native/tool source files (including untracked source), Git dirty state and the build metadata with binary hashes and configuration. Summary reports exclude summary inputs and retain only the latest action/configuration/source result; older source checkpoints can still show earlier failures. Explicit invalid Qt prefixes fail instead of falling back. Build records distinguish requested UI from the produced target. Runtime SDK paths follow the recorded build.

Commands have finite deadlines (normally 600 seconds, offscreen UI 60, packaging deployment 180); timeout exit 124 and the log are failures. CTest cases also have deadlines; the SPSC test joins its worker before reporting failure and has a 10-second watchdog.

`python tools/dev.py package` stages a uniquely named local portable Windows app under `build/packages/`, deploys Qt/QML/compiler runtimes with the build's windeployqt, includes processors/manifests and PortAudio, and runs an offline smoke check plus self-closing normal-window startup with SDK paths and inherited Qt/audio overrides removed. The package smoke report must explicitly confirm success and audio off; a read-only integrity inventory is saved outside the package after its manifest is written. The headless Qt display plugin is explicitly included. `--portaudio PATH` chooses a backend DLL explicitly. Missing prerequisites fail clearly. Package contents and hashes are recorded in `package-manifest.json`.

This is a local development package, not a public redistribution release or proof on a clean Windows machine. Public distribution still needs bundled dependency license/source notices and the usual release review. Pi deployment remains unverified.

## Staged acceptance

`python tools/dev.py check --package` runs helper tests, build, CTest, two UI sizes, a self-closing audio-off startup probe, bounded offline benchmarks/render, and portable packaging. It stops dependent stages on a failure and saves JSON plus Markdown with exact report/log paths. `startup-test --portable PATH` checks a staged folder; `ui-test` checks both 1400x900 and 1000x650. These are audio-off checks.

Smart App Control previously blocked unsigned local development binaries. The user turned it off and the current acceptance runs pass. Agents did not change security settings. Investigate actual future security flags; do not install unknown programs or change protection automatically. `docs/BUILD_CHECKPOINT.md` records the latest accepted evidence; the one-time scheduled continuation has completed.

Read-only package audit: `python tools/foundation/package_inventory.py --package PATH --output REPORT.json`. Numerical reference preparation: `python tools/experiments/reference_kernels.py`; its batch-average metrics are not realtime latency tails or a native-runtime comparison.

## Personal desktop launch

The Wirescape Desktop and Start-menu shortcuts launch the tested standalone package directly, with audio off. Windows Search can find the Start-menu entry. After accepting a newer local package, refresh the links with `tools/install_windows_shortcuts.ps1 -Package PATH -LocalFont PATH_TO_PRIVATE_FONT` (omit LocalFont for fallback). Keep the currently targeted package until the links are updated. The script verifies shortcut targets and refuses to overwrite an unrelated shortcut.

The optional `--local-font PATH` loads a private font for this process only. Fonts are not installed system-wide, committed or bundled. The present local font and its terms live in ignored `.tools/fonts/`. This local arrangement establishes no future redistribution rights.

While the user works in Ableton, defer physical audio/interface checks and performance comparisons. Offline functional checks remain allowed; build with `CMAKE_BUILD_PARALLEL_LEVEL=2` to limit build concurrency. Record focused stage reports honestly rather than claiming the full benchmark checkpoint ran.

`python tools/dev.py check --functional-only --package` runs the automated functional checkpoint during other music work. It marks performance benchmarking deferred in JSON and Markdown; default check still includes the benchmark. This does not authorize any physical audio testing.
