# M1-R: Planner review of the ABI 2 foundation slice (Claude)

**Agent:** Claude (planner/reviewer). Read-only review; no feature code.
**Prerequisites:** M0-3 CI is green, ideally including the ARM64 run of the ABI 2 suites and benchmark.

**Read first:**
1. `docs/PLANNER_HANDOFF.md`, section 6 (checklist)
2. `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`
3. `docs/specs/ABI2_SPEC.md`
4. Then verify selectively in `native/include/wirescape/module2.h`, `native/sdk/`, `native/engine2/` and `native/voice/`.

## Output: one comment on this issue
- Findings by severity. For each: file and line, trigger, consequence, proposed fix and test.
  - Separate observed defects from design preferences and hypothetical risks.
- **A performance diagnosis plan** for the 1.89× mono gap: which profile to collect, the likely hot spots, and the acceptance numbers for Windows and ARM64.
- What can stay as it is.
- The decisions to record in `docs/specs/ABI2_PLAN.md` section 9, including the experimental semantics (overflow, timestamps, latency).
- **Go/no-go for M2**, and the conditions for it.

## Then
- Write `docs/tasks/M1-3_SLICE_FIXES.md`, and open its issue (`agent/codex`, `size/m` or `size/l`, milestone M1).
- Update `docs/PLAN.md`: the current milestone and the checkboxes.
