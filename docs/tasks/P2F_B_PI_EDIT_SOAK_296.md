<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2F-b (#296): hour-long device-free Pi edit soak

Issue #296; Keel ruling: #296 comment 6720719308. Branch `task/296-2fb`.
Read AGENTS.md, docs/tasks/P2F_SIDECAR_EDIT_SOAK.md, #278/#284, the
latest #296 signed handoff, docs/PI_RUNNER.md and docs/WORKFLOW.md.
Tier T1 for the public harness. The private runner is separately reviewed T2.

## Public slice (this branch)

- Launch the local engine only as `--null-audio --bench-summary --settings
  <isolated temporary file>`; never accept arbitrary engine flags, physical
  devices, LAN or SSH. The original bounded edit/notes workload and seed stay.
- After EOF and a clean zero exit, require exactly one anchored stderr
  `bench blocks=<u64> p50=<float> p99=<float> max=<float> overruns=<u64>
  faults=<u64>` line. Use bounded stderr capture independent of the tail.
  Reject malformed, missing, duplicated, non-finite or out-of-range data.
- `blocks >= 0.9 * seconds * 48000 / 256`. A nonzero overrun or render
  fault fails. Do not turn absent evidence into zero or a passing result.
  Report `performance_evidence` as a numeric object with `blocks`,
  `p50`, `p99`, `max`, `overruns`, `faults` only after clean shutdown.
- Report the actual native Engine2 render fault counter in the existing
  null-sink stderr bench line. Accumulate observed counter deltas through
  edits/replans; never add protocol fields or change realtime ownership.
- Add synthetic subprocess and parser negative tests, and run a real short
  headless `--seconds 2 --seed 278` null-sidecar smoke.

## Private runner slice (later, separate PR)

Only after the public slice merges: in `kalharbi11/wirescape-pi`,
read its AGENTS.md. Authorized files are `bridge.py`, `test_bridge.py`,
`README.md`. Preserve the 600-second J1 `ci/pi-soak`, generic 1800-second
acceptance timeout, current fork exclusion/concurrency and retry dedup.
`ci/pi-edit-soak` runs the fixed `python3 tools/sidecar_edit_soak.py
--engine build/native/wirescape-engine --seconds 3600 --seed 278`, timeout
3900 seconds, and reports `pi5/edit-soak`. Parse the JSON's values and
failure cases, not merely exit 0. Independent Claude T2 review precedes
integration and the exact-source-head hour-long run.

Keep #296 open while that measured Pi result is missing. Earlier-head Pi
acceptance and a 600-second J1 demo are not substitute evidence.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files (public slice)
must_pass:
  - python -W error::ResourceWarning -m unittest discover -s tools/foundation/tests -p "test_sidecar_edit_soak.py" -v
  - python tools/dev.py build --headless
  - python tools/sidecar_edit_soak.py --seconds 2 --seed 278
  - ci: guard, linux-arm64, windows-x64, sanitizers
  - pi: pi5/edit-soak successful on exact merged public source head, seed 278, 3600 seconds, measured zero overruns and faults, linked private run
stops: [ABI edit, protocol/ID changes, new dependency, real devices/LAN/SSH, threshold loosening, missing metrics treated as pass]
```

Signed: ChatGPT (Agent 1, GPT-6), 2026-11-08
