<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2F-a (#278): bounded, offline sidecar edit soak

Standalone `tools/sidecar_edit_soak.py` selects a **local executable**, starts it
with `--null-audio --settings <temporary file>` only, and never opens real MIDI,
audio or network devices. No arbitrary engine arguments or `--confirm-*` passthrough.
Requires the engine's documented protocol v0, `essentials.output` and
`essentials.vco`; tests use a synthetic process rather than hardware.

Run a short smoke after a headless build (or select a local binary using `--engine`):

```text
python tools/sidecar_edit_soak.py --seconds 2 --seed 278
```

A later dedicated runner may use `--seconds 3600` with its built executable.
Each cycle creates a fresh module ID, changes VCO frequency twice, connects and
disconnects signal audio, removes the module, sends synthetic note on/off, then
panics. Replies must match request ID, session, protocol and exact expected generation. Per-reply deadlines,
stdout size/queue bounds, concurrent stderr draining, and EOF shutdown are
fail-closed. Errors include seed, step and retained stderr tail. Duration measures
active iterations; a final in-flight cycle and shutdown may exceed the requested
seconds by bounded deadlines. A deterministic seed controls parameter choices.

The summary reports commands and topology generation, **not** rendering quality.
`performance_evidence: unavailable` means no sampled render timing or verified
overrun histogram; missing telemetry must not be read as zero. The optional
#274 bench summary is not enabled or consumed here. The 60-minute Pi run is
**pending**, not an acceptance pass. This T1 harness does not alter the sidecar,
CI scheduler, Pi workflows, thresholds or protocol.

```acceptance
tier: T1
size: <= 400 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_sidecar_edit_soak.py"
  - python tools/dev.py build --headless
  - python tools/sidecar_edit_soak.py --seconds 2 --seed 278
  - ci: linux-arm64 and windows-x64 with null sink, no physical devices
stops: [new dependency, real device/LAN access, test or threshold changes, protocol changes]
```
