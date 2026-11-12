<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CI speedup 1: parallel sanitizer lanes (#584)

T2 CI change, `.github/workflows/ci.yml` only. Today the `sanitizers` job runs
ASan+UBSan (~13.5 min) and then TSan + Callgrind (~5 min) one after the other
(baseline runs 42937522046, 42937881803, 42938923680: 19.0–19.4 min).

**Scope:** PR-only cancellation of superseded runs, where push and dispatch runs
get a group unique to their `run_id`, so `main` runs are never cancelled or
replaced. Remove `native`'s `ci-${{ github.ref }}` job group. Run the ASan+UBSan lane in
parallel with the TSan → Callgrind lane. Keep the required check name: the
`sanitizers` job has `if: always()` and is green only when both lane results are
exactly `success`. Trial `--parallel 4` on Linux ARM64 (4 vCPU / 16 GB) and
print `nproc` and `free -m`; Windows stays at 2. Keep #569's ccache in both
lanes (per-lane keys, falling back to #569's `ccache-sanitizers-` key). PRs
only restore; main pushes save, so PR runs don't churn the 10 GB repo cache.

**Unchanged:** sanitizer flags, CTest selections (`-R abi2`, TSan
`-E '^abi2_benchmark$'`), cost gate and thresholds, no suppressions.

**Validation:** the aggregate goes red for a failed, skipped or cancelled lane
(throwaway PR #586). Timings against the baseline are compiled in #587.
Keep `-j4` only if the build gets faster and memory stays well under 16 GB.

```acceptance
tier: T2
size: <= 150 changed lines, <= 2 files
must_pass:
  - actionlint
  - ci: guard, linux-arm64, sanitizers green
stops: [test selection change, sanitizer flag change, threshold change, required-check rename]
```

Signed: Keel (Claude), 2026-11-11
