<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Telemetry slots macro hygiene (#416)

Rename only the host-helper member Plan::slots to entries. Include the
real helper under an empty Qt-style slots macro in the sidecar unit
translation unit, before session.h can consume its include guard.
No runtime behavior, public schema, IDs, counters, thresholds or analyzer
policy changes. Existing telemetry/sidecar/Web tests remain mandatory.
Read AGENTS.md, telemetry_taps.h, protocol_unit_tests.cpp and #379.

```acceptance
tier: T1
size: <= 120 changed lines, <= 3 files
must_pass:
  - ctest --test-dir build/native -R "abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests" --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers green
  - ci: Web native/wasm tests and actual static diagnostics reviewed
stops: [ABI or protocol change, changed telemetry keys, analyzer suppression, weakened tests]
```

Independent T1 review required. Extracted declaration compiler checks are
diagnosis only, never production/full-header acceptance. Missing static
evidence remains incomplete; no device or subjective claims.
