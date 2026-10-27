<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Audio capture and analysis harness

Run `python tools/dev.py build` on Windows to compile the manual helper, and
`python tools/dev.py test` for **offline tests only**. The headless Linux build
also registers `capture_offline_selftests`. No CTest command opens a device.

## Explicit manual run (user only)

From the repository root, for one explicit user-authorized run:

```powershell
python tools/capture/run.py --confirm-audio --seconds 3
python tools/capture/run.py --confirm-audio --stimulus sweep --seconds 3
python tools/capture/run.py --confirm-audio --patch native/examples/Six_Essentials_Demo.json --seconds 3
```

Each command makes one bounded run; commands do not run automatically or repeat.
`--stimulus tone`, `pulses` (gated tone) and `sweep` provide known diagnostic signals.
The default `engine` stimulus renders a VCO-to-Output native patch v1 offline.
`--patch` accepts executable native patch v1 nodes, values, connections and outputNode.
`--events` accepts an ordered JSON array, for example
`[{"frame":48000,"node":1,"type":1,"parameter":0,"value":880}]` for the default VCO.
Other optional event fields are source/note; event types follow ABI1.
Use `--engine`, `--modules` and `--native` for nonstandard build locations.

Outputs go into a fresh `build_reports/capture/run-*/` directory: `reference.wav`,
`capture.wav`, their interleaved float32 vectors, exact patch/events/configuration,
endpoint ID/mix rate, native counters, binary hashes, source commit and JSON/Markdown
findings. Failed/unavailable runs retain whatever evidence was actually produced.
The float WAVs retain over-range evidence rather than clipping it during export.
Presets and original patch files are read-only. Reports never reuse an existing run.

## Bounds, isolation and scope

References use 48 kHz, last 0.1–5 seconds and have peak at most 0.05. Native patch
output gain is 0.05 (host gain 0.2 times harness attenuation 0.25); excessive or
non-finite reference output is refused before playback. The pilot adds about 27 ms.
Capture is bounded to eight seconds, activation to three seconds, and the owned
helper process to twelve seconds. Stored vectors/WAVs are bounded to about 16 MiB.
No existing application is stopped, configured or closed. Both owned streams stop
on normal completion/error; the watchdog terminates only its own helper on a hang.

The runner uses WASAPI **process-tree loopback**, requiring Windows build 20348+.
A dedicated helper renders to the default console render endpoint; only that
helper's PID tree is captured. It spawns no applications. Unrelated applications
are excluded by the Windows API, rather than inferred from endpoint session lists.
There is no microphone path and no shared endpoint-mix fallback. Unsupported API,
endpoint or format is `unavailable`; uncertain isolation is `ambiguous` and cannot pass.
The report separates application/reference errors, transport/device counters,
capture failure, and signal mismatch. Live application fault counters are unmeasured.

This measures an offline engine reference played through the isolated helper's
render stream. It does **not** verify physical speakers, endpoint post-processing,
the GUI's live callback, PortAudio or perceptual quality. The endpoint's mix rate is
recorded; WASAPI may convert the render stream to that rate. Process capture is
48 kHz stereo. Analysis supports explicit rate conversion for other offline inputs.
Left/right have opposite aperiodic pilot tags, so swapped capture channels fail;
mono patch output is duplicated after the pilot. Latency is a recording-relative
alignment offset, not calibrated physical or round-trip latency.

## Analysis and evidence

Alignment searches up to 750 ms using the pilot, not repeated tone cycles.
Linear rate conversion handles differing recorded rates without demanding byte
identity. Initial engineering tolerances in `analysis.TOLERANCES` require pilot
correlation ≥0.95, fitted gain 0.5–1.5, residual RMS ≤15%, timing error ≤20 ms,
DC <0.005, and sampled response within 6 dB after gain compensation. Ten-ms windows
separate lost active signal from intended envelope silence; surrounding unexpected
audio and discontinuities are failures. Fixed spectral bins are diagnostics, not
a complete transfer-function measurement or calibrated hardware specification.

Self-tests cover synthetic silence, clipping, mismatch, dropout, discontinuity,
truncation, latency, SRC, envelope timing and wrong channels; actual offline ABI1
renders cover deterministic events, a known-tone VCF patch and the combined Noise
demo. Runner tests use a fake backend exclusively. Windows CI compiles the helper
but never executes it. **No real-device capture has been performed by the agent.**
The user must run the explicit command once and attach `report.md`/`report.json`.

API research: [Microsoft shared loopback](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
records the endpoint system mix; [Microsoft process-loopback sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback)
documents PID isolation and build 20348+. Microsoft-maintained source documentation
and API examples were consulted; the implementation is original SDK/COM code.
