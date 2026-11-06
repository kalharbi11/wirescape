<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2B-a (#281): playable on-screen keyboard

Read issue #281, Studio AGENTS/design guidance, Keyboard.ts, App.ts construction,
EngineClient playing methods and the mock-bridge tests. Implement the Phase 2B
keyboard through existing noteOn/noteOff, hidden by default behind KEYS.
Use existing theme tokens and event-driven updates. Share the computer keyboard's
octave and per-source pitch ownership; release original notes across octave
changes, pointer release/cancel, blur, hidden page, hiding and Panic. Keep typing
and modal guards. No protocol, ABI, patch, dependency or native-engine changes.

Full UI validation runs in the existing Web job, which provisions npm, Chromium,
Playwright's recorder and Electron. The native task-acceptance job does not
install that toolchain. Local equivalent: `cd apps/studio && npm run typecheck &&
npm test`; missing recorder support is pending evidence, never a test pass.

```acceptance
tier: T1
size: <= 450 changed lines, <= 7 files
must_pass:
  - bash -c "git diff --check"
  - ci: web job typecheck, production build, full Playwright and desktop Electron suites green
stops: [new dependency, engine or protocol changes, hardware access]
```
