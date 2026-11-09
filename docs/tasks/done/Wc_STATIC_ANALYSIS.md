<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-c: static analysis job (clang-tidy diff, cppcheck, clang-format, actionlint, ruff) (#87)

**Do:** `.github/workflows/static.yml` (protected path named here): on PRs, `clang-tidy-diff` over changed C++ lines with a checked-in `.clang-tidy` (bugprone-*, performance-*, a short realtime-relevant set; no style checks), `cppcheck --enable=warning,performance` on changed files, `clang-format --dry-run` on changed lines only (add `.clang-format` matching the current dense style as closely as possible; never reformat existing files), `actionlint`, `ruff check tools/`. **Non-required** at first (`continue-on-error` off, but not in branch protection). The Pi nightly runs full clang-tidy (W1). Report counts in the step summary.

```acceptance
tier: T0
budget: 1 Codex session
size: <= 250 changed lines, <= 5 files
must_pass:
  - actionlint .github/workflows/static.yml
  - ci: static job runs on this PR and reports; its findings on this PR's own files are zero
  - ci: linux-arm64, windows-x64, guard green
stops: [reformatting existing files, new required check]
```
