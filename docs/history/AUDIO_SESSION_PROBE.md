# Manual audio session probe

`wirescape_audio_session.exe` is a manually run, explicitly authorized check of one real output-device session through the native Essentials engine and `StudioController`. It is separate from CTest, offline rendering, and GUI smoke checks. It does not assess listening quality, latency, or physical device capability.

The executable requires `--confirm-audio` and valid `--modules`, `--patch`, and `--report` paths before constructing the controller or opening an audio stream. Without that flag, it exits without audio. The probe uses the native Six Essentials demo patch and changes its Noise Level parameter to `0.05` in memory for the probe only. It does not save the patch or touch user presets/data.

Example (run only when ready to authorize live audio):

```text
wirescape_audio_session.exe --confirm-audio --modules <native-module-directory> --patch <path-to-Six_Essentials_Demo.json> --report <new-report-path.json> [--duration 3]
```

The duration defaults to three seconds and is limited to one through ten seconds. The report is written atomically after a start failure or after the session is stopped. A successful report requires observing the stream running, rendered blocks greater than zero and a positive observed engine output peak, a stopped stream with no pending cleanup, and zero callback faults, underruns, and overload blocks. Backend/device failures and unmet criteria are recorded in the JSON report and produce a nonzero exit status. If a backend fails to quiesce, inspect `cleanup_pending` and do not treat the run as a pass.

The report includes device/backend names and session counters exposed by `StudioController::diagnostics()`. It is evidence of this session on this machine only. It does not prove that every Essentials module is audible, establish audio quality, or make a claim about Raspberry Pi or other hardware support.

Actual three-second session passed on Realtek Speakers/MME with555 blocks, nonzero observed peak and zero reported faults/underruns/overloads; see CURRENT_EVIDENCE.md. No endpoint WAV was recorded. User now requires automated capture/analysis for future audio testing; see CURRENT_PLAN.md. The probe is a diagnostic component, not that completed capture pipeline.
