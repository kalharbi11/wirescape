# M1-1: ABI 2 foundation slice (DONE before the migration; kept for reference)

**Status:** implemented before the GitHub migration, at checkpoint `34b948e…` in the local archive. It arrived in the repository with the first commit.
- **Review package:** `docs/reviews/M1_FOUNDATION_SLICE_REVIEW.md`
- **Spec:** `docs/specs/ABI2_SPEC.md`
- **Author guide:** `docs/SDK_AUTHOR_GUIDE.md`
- **Code:**
  - `native/include/wirescape/module2.h`
  - `native/sdk/`, `native/engine2/`, `native/voice/`
  - `native/modules2/{vco,lfo,vcf,adsr,vca}/`
  - `native/tests2/`

**Don't reimplement it.** Its next steps are:
1. **M1-R** (`docs/tasks/M1-R_PLANNER_REVIEW.md`): the planner reviews the slice.
2. **M1-3:** a fix issue written by the planner from that review. It includes the open performance gate: mono ABI 2 runs about 1.89× ABI 1 time, against a target of ≤ 1.05×.

The original acceptance list (tests A–N and the hard boundaries) is recorded in the review package and in `docs/specs/ABI2_PLAN.md` section 5 (B1–B3).
