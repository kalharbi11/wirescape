<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M2-4b: continuous-parameter CV coverage (#49)

**Scope:** Extend the five-port D7 engine2 CV regression to mixer, noise,
output, scale_offset, slew, clock, function_envelope and ladder. Every
continuous parameter must advertise MODULATABLE, accept an engine2 cable,
change the output, and preserve its base value. No DSP math changes.
Output has no parameters. Planner's #49 handoff exempts this tests-only
change from Pi machine evidence.

```acceptance
tier: T0
budget: 1 ChatGPT CI iteration
size: <= 150 changed lines, <= 2 files
must_pass:
  - ctest --test-dir build/native -R abi2_port_behavior_tests --output-on-failure
  - ci: linux-arm64, windows-x64, sanitizers, guard green; equivalence 0
stops: [DSP math change, equivalence delta, test removal]
```
