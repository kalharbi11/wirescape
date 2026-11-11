<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Versions

From 1.0 on, Wirescape ships in numbered versions: **1.0, 1.1, 1.2, …**
(owner decision, 2026-11-11). A patch fix to a released version is 1.0.1.

- **One file per version** here (`v1.0.md`, `v1.1.md`, …). Each lists what's in
  it, known limits, the exact commit SHA, and the test sheet.
- **Pinning.** A version is pinned when the owner's test of a main commit passes.
  The SHA goes in that version's file and the commit is tagged `v1.0.0`
  (annotated, by Codex Lead or the owner). Main keeps moving; the tag doesn't.
- **Scope.** Each new version gets a GitHub milestone, `v1.1` and up. Issues are
  planned into a version. Anything unplanned waits in "Later".
- **Version strings.** `apps/desktop`, `apps/studio`, `native/CMakeLists.txt`
  and the engine `hello` all carry the same number, bumped in the release PR.
- **Install and update:** [../INSTALL.md](../INSTALL.md).
