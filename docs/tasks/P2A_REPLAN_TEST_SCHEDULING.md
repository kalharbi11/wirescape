<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2A CI blocker (#297): fake live-replan callback scheduling

Read issue #297, native/tests2/live_replan_tests.cpp Audio/live_edits,
engine2 park/replan/resume and #209 Keel review. Required Windows CI hit the
three-block replan assertion. The simulator's 256-frame callback period is
5.333ms; a timed sleep can overshoot the 16ms park deadline on a coarse timer.
Replace only the test simulator's timed sleep with an interruptible yield loop
against the same absolute steady-clock deadline. No production engine edits.

Retain all assertions, timeout values, actual steady-clock silence-gap timing,
concurrent ingress and zero-render-allocation checks. This does not guarantee
an overloaded OS will meet deadlines: exact-head Windows and Pi runner evidence
remain required. Pi runs fake devices only via the existing pi5 runner.

## Current evidence / stop

The yield-loop experiment passed one local CTest run, then failed repeated runs
under the shared container's two-CPU cgroup quota. A diagnostic failure reported
36.655ms elapsed (including control staging), seven callbacks, and
`Audio did not park before the edit deadline`. This experiment is not a fix or
acceptance evidence. Stop after two failures; Keel must rule on a deterministic
fixture seam that preserves the real deadline, or require an isolated runner.

```acceptance
tier: T2
size: <= 120 changed lines, <= 3 files
must_pass:
  - ctest --test-dir build/native -R "^abi2_live_replan_tests$" --output-on-failure --no-tests=error
  - ci: exact-head windows-x64 live-replan test passes, full ARM64/sanitizers remain green
  - pi: live-replan test preserves <=20ms at256frames on pi5
stops: [threshold loosening, production realtime change without Keel ruling, test removal, device access]
```
