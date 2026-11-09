<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #385 Evidence A: device-free ABI2 render and behavioral WAV cases

Seam: standalone `wirescape_render` under `native/tools/render/`, using the existing
`app_engine` and production patch-v2 `runtime_patch` loader. No second engine,
sidecar flag, audio/MIDI device, protocol change, dependency or reference hash.
Do not extend ABI1-only `tools/capture/reference.py`.

Input: bounded patch-v2 and a sorted frame-addressed JSON event script; rate,
block, duration (at most 10 seconds), and seed. Output: unclamped float32 stereo
WAV, measurements JSON and Markdown with source/stimulus/renderer provenance.
Any render, meter, load, or write failure exits nonzero. Late/dropped/rejected
events and missing measurements fail closed.

Three four-second fixtures: two ADSR-gated notes; VCO 220→440 Hz frequency
step; two VCOs selected by merged `sound.blend`. Finite audio, active/silent
windows, bounded peak and ±2% zero-crossing tests are mandatory. Negative
fixtures for silence, NaN, clipping, wrong window/frequency and deliberately
late injection must fail. No exact-hash references.

CTest names `render_evidence_*` execute device-free on hosted and Pi.
Upload linux-arm64 artifact only, 14-day retention and <=20 MiB cap.
A WAV artifact is automated evidence, not subjective listening approval.
Separate independent T1 review and Keel seam look are required.

```acceptance
tier: T1
size: <= 560 changed lines, <= 10 files
must_pass:
  - ctest --test-dir build/native -R render_evidence --output-on-failure --no-tests=error
  - pi-run: ctest --test-dir build/native -R render_evidence --output-on-failure --no-tests=error
  - ci: linux-arm64 uploads render_evidence artifact
stops: [new DSP path, protocol change, device access, new dependency, exact-hash references]
```

Signed: ChatGPT (GPT-6, offline evidence integration), 2026-11-08
