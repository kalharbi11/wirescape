<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W0: Codex cloud setup script (workflow v2) (#83)

**Why:** Codex cloud tasks start from a fresh container. Without a setup script each task spends its first turns installing CMake/Ninja/Qt and guessing; and some tasks can't read CI logs (log downloads redirect to an artifact host the container's network policy blocks).

**Do:**
1. `tools/cloud/setup.sh` (bash, idempotent, no `sudo` assumptions beyond what the Codex image allows): installs the pinned `cmake==4.2.3` and `ninja==1.13.0` via pip as CI does, checks for `g++`/`clang++`, prints versions, then runs `python tools/dev.py build --headless` once to warm the build. No Qt (headless is enough for cloud work; UI tests run in CI). Under 60 lines.
2. `tools/cloud/README.md`: what to paste into the Codex environment settings (setup script path, env vars), and the network allowlist the user adds so CI logs are readable: `api.github.com`, `github.com`, `objects.githubusercontent.com`, `productionresultssa*.blob.core.windows.net` (GitHub Actions log/artifact storage), `pypi.org`, `files.pythonhosted.org`.
3. `tools/ci_logs.py <PR#|run_id> [--tail 80]`: fetches failed-job logs through the GitHub REST API (`GITHUB_TOKEN` from env), prints only the lines around `error`/`FAILED`/`Assertion`, capped at 200 lines. Never prints secrets or env dumps.
4. `.github/workflows/ci.yml` step "Summarize failures": also write the tail (≤80 lines per failed log) to `$GITHUB_STEP_SUMMARY`, so the failure shows on the run page without downloading artifacts. Protected path: this issue names `.github/workflows/ci.yml`.

**Read first:** `.github/workflows/ci.yml` lines 40–160, `tools/dev.py` `command_build`.

```acceptance
tier: T0
budget: 1 Codex session
size: <= 250 changed lines, <= 5 files
must_pass:
  - bash tools/cloud/setup.sh                       # from a clean clone, exits 0, prints toolchain versions
  - python tools/ci_logs.py --self-test             # parses a recorded sample log fixture, prints the error lines only
  - ci: a deliberately failing test on a throwaway branch shows its tail in the run's step summary (link in the PR), then the branch is deleted
  - ci: linux-arm64, windows-x64, guard green on the exact head
stops: [new dependency, secrets printed, protected path other than ci.yml]
```
