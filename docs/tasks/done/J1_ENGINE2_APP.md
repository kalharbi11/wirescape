<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J1: engine2 becomes the app engine (#29, PR #78)

Spec: `docs/tasks/M2_PORT_AND_APP_ENGINE.md` § M2-6, plus the J additions and planner decisions in `docs/tasks/J_JAM_CHECKPOINT.md` (app-path render bound ≤ 1e-7 with stated cause; per-module equivalence exact; single catalogue registration point).

```acceptance
tier: T2
budget: 1 Codex session (remaining)
size: n/a
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "app_patch_tests|app_engine_tests|document_reader_tests|patch_document_tests|patch_render_tests|abi2_integration_tests" --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers, guard green on the exact head
  - pi: pi5 and pi5/soak green on the exact head
stops: [ABI edit, equivalence delta beyond 1e-7, test removal]
```
