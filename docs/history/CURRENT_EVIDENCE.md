# Current verification and limits

Consolidated October26,2026. Reports are local ignored artifacts, not assertions of universal support. No audio was captured from the physical output during this work.

## Accepted evidence

| Scope | Actual evidence | What it establishes |
|---|---|---|
| Full six-module checkpoint | build_reports/foundation/check-20261026T172345Z.json and .md | 27 Python tests,16 CTest suites, both1400x900/1000x650 UI checks, normal startup, offline render,45 existing CLI benchmark scenarios and local package checks passed |
| Imported filter review | docs/SIX_ESSENTIALS_IMPORT_2026-10-26.md/.json | Focused Sol High review and added maximum-resonance quiet-tail/1-and17-frame modulation checks at44.1/48/96k; no demonstrated algorithm blocker in tested scope |
| App module integration | native/ui/tests/controller_failure_tests.cpp | Palette/add/typed wiring/parameter updates, exact source preservation on bad input, combined-module save/load and byte-identical offline WAVs, shipped demo load/nonzero render |
| Package | package-20261026T172345Z.json | 1,421 files matched inventory,13 Essentials DLLs, offline and normal-window startup passed; not clean-machine certification |
| Manual audio probe build | build-20261026T173715Z.json | Explicit-only device probe compiled using existing SDK; not part of generic check |
| Real device session | audio-session-six-20261026.json and .log | Three-second six-module demo on Realtek Speakers, MME,48kHz,256 frames,stereo output.555 rendered blocks; nonzero observed processing peak~0.00256; zero reported underruns,callback faults,queue overflows,late events and engine overloads; clean stop/no pending cleanup |
| Probe guard | audio-probe-no-confirm.log | Missing --confirm-audio exited2 before controller/device construction |
| Post-probe offline regression | test-20261026T184344Z.json | Existing16 CTest suites passed after the probe/CMake addition; no fresh full package/check is claimed after this addition |
| External kit | essentials_build_guide/VALIDATION.md | Windows C++ example/DLL discovery and4 verifier tests passed; extracted ZIP was built independently of repository paths |

The real-session JSON is diagnostic evidence only. The recorded peak is an engine-side observation; it does NOT prove the physical output was captured or heard. User was away and did not provide listening feedback. System mute/volume, endpoint processing, actual speaker output, recording alignment and perceptual quality have not been measured.

## Pending evidence / known limits

- No output-recording/analysis pipeline yet. This is now required for future unattended audio tests; see CURRENT_PLAN.md.
- No MIDI/OSC/UDP service, loopback tests, event-output graph API, general polyphony or new sequencer implementation. Legacy Python sequencer is not migrated.
- No new-module realtime CPU/capacity claims: the45 benchmark cases are the existing CLI workload set. Per-sample tan in filters and curve expm1 may be optimization candidates after representative measurements and output-equivalence tests.
- No long-session device/dropout/reconnect matrix, listening-quality certification or physical CV/MIDI tests. One short MME session is not all-backend validation.
- No Pi/ARM64/Daisy cross-build or hardware result, no clean-Windows result. Recipes and static source patterns are not successful target builds.
- No live provider ingress validation. F06 is an offline one-owner proof with an explicit render barrier; generation changes alone do not acknowledge an applied reset.
- No full legacy musical migration. Inspector recognizes historical shapes and safely refuses unsupported execution; document preservation is not sound-equivalent import.
- ABI1 is experimental and has already evolved internally. Do not mix old host/module binaries. No SDK freeze, marketplace, auto-installer/hot reload or release certification.

## Safety/history clarification

User turned Smart App Control off; agents did not change protection. Its prior unsigned-binary failures are historical. User lifted Ableton restrictions and allowed normal close if necessary; no Ableton process was found during the checks, and no app was forced closed. Real audio was subsequently explicitly authorized by the user. Default startup and all automated offline checks still keep audio off. No security changes or additional scheduled automations are needed for review.

Current audio-session documentation originally used a stale executable name; it is corrected to wirescape_audio_session.exe. Local builds/reports show the actual target. The latest wrap adds documentation only after the successful post-probe regression; no feature behavior was changed for review.
