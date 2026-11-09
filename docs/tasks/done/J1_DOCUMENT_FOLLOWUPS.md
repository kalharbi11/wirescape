<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J1 document-loading follow-ups (#132)

From Claude's J1 review: hand-written unknown v2 modules without an inactive
annotation must stay editable/inactive and retain original fields and opaque
preserved data. Preserve strict validation and value-identical normalized reloads.
Document why the controller dispatch override is still needed; inspect handles
the v1 envelope while read_patch validates v1/v2. Verify the gain_cv rejection
case uses two existing ports of incompatible signal type, with a diagnostic.
No patch-format/ID change, test removal or tolerance change.

Self-decisions (review later): file cap6 (+20% from5 under WORKFLOW3c) includes
this discoverable acceptance file; line cap120 and all acceptance tests retained.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 120 changed lines, <= 6 files
must_pass:
  - ctest --test-dir build/native -R "patch_document_tests|app_patch_tests|studio_ui_tests|controller_failure_tests" --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers, guard green
stops: [patch format change, test removal]
```
