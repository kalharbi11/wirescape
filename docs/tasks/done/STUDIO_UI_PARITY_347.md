<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio mobile and web UI parity (#347)

Compare the current Electron/Studio and static WebAssembly browser interface against
the Wirescape-UI design kit. Fix limited shared chrome, touch defaults, accurate runtime
messages, and the web audio Start gesture; record deeper missing engine-telemetry
parity rather than substituting fabricated signal animation.

Read first:
- `apps/studio/AGENTS.md`
- `apps/studio/docs/DESIGN.md`, `apps/studio/docs/PERFORMANCE.md`
- `apps/studio/ui/wirescape-ui-kit.html`
- `apps/studio/src/web/WebBridge.ts`
- `docs/reviews/STUDIO_WEB_UI_PARITY_2026-11-07.md`

Scope:
- Phone patch name, drawer dismiss/ARIA, coarse-pointer hit defaults, playable keyboard sizing.
- Browser audio-start tap ordering, truthful native/web labels.
- Forward actual engine telemetry fields unchanged when provided.
- Chromium touch/device-emulation UI assertions and Playwright screenshot attachments.
- Review/handoff covering both platform limitations and next telemetry issue #143.

Out of scope:
- Realtime/audio engine semantics, ABI/protocol/patch schema and new dependencies.
- Artificial signal-driven motion or weakening existing tests.
- Claiming physical iOS Safari, Pi or standalone Electron hardware validation from emulation.

```acceptance
tier: T1
size: <= 650 changed lines, <= 10 files
must_pass:
  - ci: web job typecheck, build and Playwright (including mobile gesture checks)
  - ci: web-demo native engine and site jobs including web Playwright
  - ci: Electron shell tests under the web job
  - bash -c "git diff --check"
stops: [ABI/DSP/protocol changes, new dependency, fabricated signal telemetry, reducing test coverage]
```
