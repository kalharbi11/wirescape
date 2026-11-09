<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P3C-a (#259): register Step Sequencer and Random Source as built-ins (T1)

Register the existing ABI 2 SDK examples in `wirescape::app::builtin_modules()` without duplicating DSP. Use the published `example.step_sequencer` and `example.random_source` module IDs and unchanged port and string parameter IDs.

**Read first:** `AGENTS.md`, issue #259 and PR #263 (Keel's `legacy_index` ruling), `docs/SDK_AUTHOR_GUIDE.md`, `docs/specs/ABI2_PLAN.md`, `native/sdk/examples/`, `apps/studio/public/wirescape_modules.json`.

**Build and verify:**
1. The shared SDK descriptors assign `legacy_index` from the existing `legacy_parameters` mapping, never a newly numbered slot. They do not modify DSP, patches, string IDs, or the ABI header.
2. Generate/check the UI catalogue from the existing manifests, keeping its identities stable.
3. All twelve UI starters now load and render; genuinely unknown module IDs still refuse.
4. For every built-in, assert unique `legacy_index` slots per module and agreement with the UI catalogue.
5. Device-free tests prove sequencer notes reach a VCO, seeded Random Source is bounded and deterministic, and renders allocate nothing.
6. Request exact-head Pi evidence through the `ci/pi` label; physical audio and MIDI devices stay off.

**Size:** 600 changed lines / 12 files, including this task file. No test threshold reductions.

```acceptance
tier: T1
size: <= 600 changed lines, <= 12 files
must_pass:
  - ctest --test-dir build/native -R '^(ui_catalogue_sync|ui_patch_conformance|abi2_builtin_source_tests|abi2_example_step_sequencer_tests|abi2_example_random_source_tests|abi2_runtime_patch_tests|abi2_app_patch_tests|app_engine_tests)$' --output-on-failure --no-tests=error
  - python tools/gen_ui_catalogue.py --check
  - ci: native Linux ARM64, sanitizers, static analysis and required checks
  - pi: headless Release build + device-free tests on the exact PR head (pi5)
stops: [ABI header edit, renamed ID, changed DSP behaviour, test removal, new dependency]
```
