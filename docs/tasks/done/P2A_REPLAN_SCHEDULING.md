<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2A: hosted live-replan park scheduling (#297)

Keel's bounded-retry ruling is recorded in #297. Every hosted attempt retains
the engine-enforced three-block park deadline. Only the specific park-timeout
refusal may retry; any other failure must fail the test. A timeout must retain
generation and zero faults, and cancellation retires before the next attempt.
At most three hosted attempts; print retry counts. Strict mode via
--max-gap-ms or WIRESCAPE_REPLAN_STRICT=1 allows one attempt only.
No production change, skipped assertion, relaxed deadline or gap threshold.

Read first: native/tests2/live_replan_tests.cpp Audio/live_edits,
native/engine2/engine2.cpp Engine::replan, #297 and #320 review 5812013997.
Changes are the test fixture and this task. Merge current main without
discarding its planner state. Independent cross-family T2 review required.

```acceptance
tier: T2
size: <= 120 changed lines, <= 3 files
must_pass:
  - ctest --test-dir build/native -R "^abi2_live_replan_tests$" --output-on-failure --no-tests=error
  - ci: exact-head windows-x64 live-replan test passes, full ARM64/sanitizers remain green
  - pi: live-replan test preserves <=20ms at256frames on pi5
  - pi-run: build/native/abi2_live_replan_tests --max-gap-ms 20
stops: [threshold loosening, production realtime change without Keel ruling, test removal, device access]
```
