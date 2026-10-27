# Six Essentials return: accepted integration

Accepted October 26, 2026 from the user-supplied Wirescape_Essentials_Six_Modules_Source_2026-10-26.zip. SHA-256 and original/accepted file hashes are in SIX_ESSENTIALS_IMPORT_2026-10-26.json. Original return is preserved in ignored local_backups/imported_modules; staging is ignored incoming_modules. Returned text/test reports were treated as claims, not instructions or proof. Existing modules and user data were not overwritten.

## Accepted modules

Noise (deterministic white noise), Slew (linear seconds/unit rise/fall), Function Envelope (gated Rise/Fall/Curve), Clock (CONTROL pulse generator), VCF (two/four-pole low-pass taps) and Ladder Filter (ladder-inspired bounded feedback/drive). All IDs, ports, numeric parameter IDs/defaults and missing-input rules match their packet specs. VCA LPG, ADSR gate-cable enhancement, exponential Lag, host transport/event clock, full Maths, new noise colours, polyphony, MIDI/OSC/UDP, scopes and firmware ports are not implemented by this return.

## Review and changes

- Checked archive paths, duplicate entries, symlinks, file types/size, source includes, six literal module CMake entries and submission shapes before compiling. No binary/dependency/network payload or installer was present.
- Sol High focused review found the filters' TPT zero-delay algebraic feedback / bounded delayed-feedback algorithms consistent with their design notes; no demonstrated blocker. Added the requested test coverage for quiet impulse decay at maximum resonance and 1/17-frame automation/quiet release at 44.1/48/96k, testing both VCF outputs. No assertion was weakened to obtain a pass.
- Imported processor algorithms unchanged. Only production-source adjustment is the existing WS_MODULE_ENTRY_NAME override convention in six wrappers, retaining ws_module_entry by default. This enables distinct static entry names without changing DSP. A Windows export hook is not proof of embedded compatibility.
- Added six actual-DLL CTest targets, one filter stress target, artifact hash reporting and controller-level palette/add/typed-wiring/parameters/save/load/offline-render tests. A combined eight-module graph renders byte-identically before/after reload. The shipped native/examples/Six_Essentials_Demo.json is also loaded and rendered by that regression.

## Actual validation

Full `python tools/dev.py check --package` passed: build_reports/foundation/check-20261026T172345Z.json and Markdown. 27 Python tests, 16 CTest suites, both UI sizes, native startup, offline WAV render, 45 existing CLI benchmark scenarios and local package startup/integrity passed. The standard benchmark is the existing workload set, not a new measurement of each imported processor's realtime CPU capacity. Package wirescape-release-20261026T172338Z has 1,421 matching files including 13 Essentials DLLs. Desktop and Start-menu links now target this accepted package and preserve the private local font argument.

User explicitly lifted Ableton restrictions during this stage and authorized normal closing if necessary. Process check found no Ableton process; no app was forced closed. Automated checks kept physical audio off. Hardware listening, long sessions, CPU/aliasing quality studies, Pi/Daisy builds and hardware resource headroom remain separate. Both filters currently compute tan per sample; coefficient caching is a future measured behavior-preserving optimization candidate, not silently applied during import. Their input magnitude guard at 1e6 and analog-model limits remain documented in DESIGN.md.

## Try it

Launch Wirescape from Desktop or Windows Search, then Open native/examples/Six_Essentials_Demo.json. Noise goes through VCF/Ladder/VCA/Output; Clock drives Function Envelope and Slew into VCA gain. This patch uses a buffer gate, so manual Hold Gate is unnecessary once rendering/audio is explicitly started by the user. Pan/zoom to inspect the whole graph. The default startup patch remains unchanged.

Submitted TEST_REPORT.json files remain the external author's Linux report. Local Windows acceptance above and the source hash report are the independent evidence. No user presets or Surge files were modified. No worker remains active.
