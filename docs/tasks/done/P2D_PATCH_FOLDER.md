<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2D-a (#276): native patch-folder defaults

Read issue #276, `apps/desktop/main.ts`, `apps/desktop/DESIGN.md`, and
`apps/desktop/tests/shell.spec.cjs`. This is the independent patch-folder slice
of PLAN Phase 2D; library browsing and disk recovery are follow-ups.

Use Electron's existing per-user application data directory, with a lazy
`patches/` child. Open starts there; first Save As suggests `Untitled.json`.
After a file is selected, reuse its path. Cancellation preserves selection;
directory errors reject the operation without blocking startup. Never seed or
overwrite existing patches. Preserve raw text, bounded file access, atomic saves,
the five-member bridge and main-owned paths. Tests use temporary directories,
cancelled/stubbed dialogs and the null-audio engine stub only.

```acceptance
tier: T1
size: <= 200 changed lines, <= 4 files
must_pass:
  - ci: web job green including desktop build and shell.spec.cjs patch-folder cases
  - bash -c "git diff --check && node --check apps/desktop/tests/shell.spec.cjs"
stops: [new dependency, engine or protocol changes, hardware access]
```
