<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2B-a (#281): playable on-screen keyboard

Read issue #281, Studio AGENTS/design guidance, Keyboard.ts, App.ts construction,
EngineClient playing methods and the mock-bridge tests. Implement the Phase 2B
keyboard through existing noteOn/noteOff, hidden by default behind KEYS.
Use existing theme tokens and event-driven updates. Share the computer keyboard's
octave and per-source pitch ownership; release original notes across octave
changes, pointer release/cancel, blur, hidden page, hiding and Panic. Keep typing
and modal guards. No protocol, ABI, patch, dependency or native-engine changes.

```acceptance
tier: T1
size: <= 450 changed lines, <= 7 files
must_pass:
  - bash -c "cd apps/studio && npm run typecheck && npm test"
  - ci: web job including existing desktop regression suite green
stops: [new dependency, engine or protocol changes, hardware access]
```
