# M0-3: CI, guardrails and repo hygiene

Branch `task/<issue#>-ci-setup`, draft PR. Follow AGENTS.md. This task is explicitly allowed to change `.github/**`.

## Deliverables
1. **`.github/workflows/ci.yml`:**
   - **linux-arm64** (`ubuntu-24.04-arm`):
     - `python tools/dev.py build --headless` plus the native tests, **including the ABI 2 suites and the ABI 2 benchmark**
     - upload the benchmark summary as an artifact, since M1-R needs the ARM64 numbers
     - runs on every push and every pull_request
   - **windows-x64** (`windows-latest`):
     - MSVC, plus Qt 6.10.x MSVC x64 through a maintained install-Qt action, with caching (set `WIRESCAPE_QT_DIR`)
     - PortAudio built from a pinned upstream revision, or from a pinned vcpkg baseline. Set `WIRESCAPE_PORTAUDIO`. Record the revision and licence in `docs/ARCHITECTURE.md`. The M0-1 report has the details.
     - runs `python tools/dev.py build`, then `test`, then `ui-test`
     - triggers: pull_request events where the PR is **not a draft**, PRs with the label `ci/windows`, pushes to `main`, and `workflow_dispatch`
   - **Both jobs:**
     - `concurrency: { group: ci-${{ github.ref }}-${{ matrix.os }}, cancel-in-progress: true }`
     - a 30-minute timeout and cached dependencies
     - **audio always off**
     - short summaries as artifacts; full logs only on failure
   - **A commented-out `pi5` job** with `runs-on: [self-hosted, linux, ARM64, pi5]`. It may only run on `push` to branches in this repo, **never** on `pull_request` from forks. Include notes on how to register the runner.
2. **The guard job** (`guard`, runs on every PR, takes seconds), which fails when:
   - the PR changes protected paths (the AGENTS.md "Safety rails" list) without the label `allow-protected`
   - the PR deletes or renames test files without the label `allow-test-removal`
   - the PR adds binary or private files (`*.dll *.exe *.zip *.wav *.ttf *.otf`, or `user_data/`, `.tools/`, `build/`)
   - the PR adds machine-specific paths (a Windows drive-letter user-profile path, or `/home/<user>`) outside `docs/history/`, `legacy/` and the M0-1/M0-2 task files

   Use a small Python script, `tools/ci/guard.py`, and give it unit tests.
3. **`.github/CODEOWNERS`** listing the protected paths, with `@kalharbi11` as owner, so review requests are automatic.
4. **`.github/dependabot.yml`** for GitHub Actions versions (monthly).
5. Make sure `tools/dev.py` exits non-zero on any failure and prints one summary line per suite.
6. Add the labels `allow-protected` and `allow-test-removal` with `gh label create`.
7. Add a "CI" section to `docs/ARCHITECTURE.md` (5–10 lines).

## Acceptance
- **`linux-arm64`, `windows-x64` and `guard` are green on the PR.** Add the label `ci/windows` to run Windows while it's still a draft.
- A deliberately failing test (a throwaway commit, then reverted) turns CI red.
- A throwaway edit to `AGENTS.md` turns `guard` red without the label.
- A handoff comment lists the exact check names and the branch-protection settings for the user:
  - require a PR before merging
  - required checks: `linux-arm64`, `guard`, and `windows-x64`
  - block force-pushes and deletions
  - no required approvals: the solo owner can't approve their own PRs
