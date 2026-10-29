# Contributing to Wirescape

Thanks for helping. Wirescape is built by people and AI agents together, under the same rules.

1. **Read `AGENTS.md` first.** It has the read map, work rules and engineering rules. They apply to humans too.
2. **One issue, one branch, one PR.** Use the branch name `task/<issue#>-<slug>` and put `Closes #<issue#>` in the PR description. Open an issue before large work.
3. **Sign your commits** (Developer Certificate of Origin): `git commit -s`. By signing, you certify you have the right to submit the code under the project's licences (https://developercertificate.org/).
4. **Tests and CI.** Every change ships tests. Run `python tools/dev.py test` locally. CI must be green before review.
5. **Licences.** Add an SPDX header to new files. SDK and ABI paths are MIT; everything else is GPL-3.0. Record any third-party source, its pinned revision and its licence in the module's `DESIGN.md`.
6. **New modules.** Use the experimental SDK template (`native/sdk/template/`) and `docs/SDK_AUTHOR_GUIDE.md`. The ABI 2 contract is not frozen yet; expect changes until milestone M2.
7. **Never commit** fonts, personal data, binaries, credentials or machine-specific absolute paths.

Pull requests from forks need maintainer approval before CI runs.

Optional local hooks: install `pre-commit` in your development virtual environment,
then run `pre-commit install` and `pre-commit run --all-files`. These local hooks
inspect the Git index: SPDX is required only for new files, binary/private rules
match CI, and protected paths produce a local warning while CI still enforces
authorization. Staged C++ lines require `clang-format` (version 18 in CI) and are
checked without rewriting files or including unstaged edits. A clean index passes
without checking old SPDX headers or reformatting existing code.
