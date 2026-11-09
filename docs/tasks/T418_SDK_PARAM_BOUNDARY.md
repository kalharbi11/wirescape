<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T418 (#418): SDK 64-parameter prepare/render and high-index ingress regression

**Purpose.** Complement the merged #404 add-node cap test with an actual SDK descriptor whose 64 parameter IDs are distinct. Prove SDK prepare, Engine render, parameter views at 0/31/32/63, and a bounded last-index PARAM event.

**Scope.** Tests only in `native/engine2/tests/port_behavior_tests.cpp`. No shipped module, ABI/SDK header, production engine, catalogue, protocol, UI, dependency or CI change. Preserve the existing 50/64/65 add-node boundary assertions. A production failure is a T2 follow-up, not permission to change production in this lane.

**Read first.** `AGENTS.md`, `docs/SDK_AUTHOR_GUIDE.md`, #404's reviews, `native/engine2/tests/sdk_tests.cpp`, `native/engine2/tests/port_behavior_tests.cpp`, and the Engine PARAM-ingress tests.

**Evidence.** Device-free native CTest plus exact-head required CI; source-only review is not a build pass. No Pi/physical-device claim for this test-only change.

```acceptance
tier: T0
size: <= 180 changed lines, <= 3 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "^abi2_port_behavior_tests$" --output-on-failure --no-tests=error
  - python tools/dev.py test
  - ci: guard, linux-arm64, windows-x64 and sanitizers on the exact head
stops: [production fix, ABI or SDK header edit, new dependency, test loosening]
```
Signed: ChatGPT (GPT-6, SDK64 boundary-test builder), 2026-11-08
