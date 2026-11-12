<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #599 Studio v1 visuals: Random Source, cable gestures/activity and Output meter

Owner-reported Studio fixes from #597. This is one T1 UI-only packet; the
native per-output peak telemetry correction is already merged in #598.

## Scope

- Render a bounded, scrolling history of actual Random Source `step` and
  `smooth` outputs. Never invent an LFO phase/sine for Random Source; when
  output telemetry is absent, say it is unavailable.
- Alt+drag on an output detaches its newest outgoing signal/event cable by the
  source end. Ordinary output drag still starts a new cable. Pulling a
  destination end and dropping either detached end on empty canvas removes
  that cable; Undo restores it. Selected cable Delete and context-menu removal
  must also work.
- Indicate carrying-signal cables from available source-output observations at
  about −60 dBFS (linear 0.001), with ~200 ms decay and ≤30 Hz updates. No
  per-frame allocations or animated motion for reduced-motion users.
- Verify the Output card subscribes to actual module-specific peak telemetry,
  including the existing FakeEngine path. Do not synthesize native peaks.

## Verification

The Playwright review-artifacts suite records video and requires its pinned ffmpeg binary. Install that Playwright-managed browser helper in the task environment before running the full, unchanged suite; do not skip the recording test.

Playwright with FakeEngine: Random Source non-sinusoidal step changes and
smooth output; unplug at both ends and Undo; context-menu removal; reduced
motion cable highlight; Output meter moves from FakeEngine levels. Attach
screenshots of all four UI behaviors to the test report. No DSP, protocol,
module/port/range, saved patch format, dependency or device change.

Patch guide impact: none — visual/gesture behavior only.

```acceptance
tier: T1
size: <= 500 changed lines, <= 8 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green
stops: [native engine change, protocol or patch format change, new dependency, device/LAN access, skipped tests, relaxed thresholds]
```

Signed: ChatGPT (GPT-6 builder), 2026-11-11
