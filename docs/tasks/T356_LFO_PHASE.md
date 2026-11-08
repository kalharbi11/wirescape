<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #356 B-mod: add the LFO phase signal output

This is part B-mod of #350. The binding contract is
`docs/design/ENGINE_TELEMETRY.md` ruling 12; the dependency and ownership map is
`docs/tasks/T350_ENGINE_TELEMETRY.md`. This task-specific acceptance block
makes the requirements from issue #356 runnable by the repository's
`tools/dev.py accept` and CI task selector.

Append a polyphonic unipolar signal output `phase` to `essentials.lfo`:
the actual DSP phase, normalized into [0,1), for free-run and transport sync.
Keep existing port and parameter IDs and order, `control` DSP samples and
legacy patch loading unchanged. Update the DSP, native manifest, README/DESIGN,
and both Studio presentation and generated web catalogue. B-wire host telemetry
mapping belongs to #359, not here.

The saved-patch compatibility requirement is exercised by `abi2_runtime_patch_tests`.
The additional existing test file raises the file budget from 8 to 9 (within the
workflow's 50% builder allowance); the 200-line limit stays unchanged.

```acceptance
tier: T1
size: <= 200 changed lines, <= 9 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ctest --test-dir build/native -R '^abi2_(module|runtime_patch)_tests --output-on-failure
  - ci: engine, linux-arm64, web-demo native
stops: [ABI/SDK header edit, changed existing port or param IDs, DSP behaviour change on control]
```

Signed: ChatGPT (GPT-6, module builder), 2026-11-08
 --output-on-failure
  - ci: engine, linux-arm64, web-demo native
stops: [ABI/SDK header edit, changed existing port or param IDs, DSP behaviour change on control]
```

Signed: ChatGPT (GPT-6, module builder), 2026-11-08
