<!-- SPDX-License-Identifier: GPL-3.0-only -->
# iPhone Safari web audio-start splash (#339)

The static web demo's first-run audio permission card must remain usable on
narrow iPhone Safari viewports, including short landscape windows and the
visible viewport shrinking under browser chrome. Keep the entire card within
the available width and safe-area insets; when its content is taller than the
viewport, scroll the overlay to reach the Start/Try again button and errors.
Preserve the centered desktop appearance and the existing click-to-start audio
behavior. No changes to the engine, AudioWorklet or app-wide viewport zoom.

Scope: `apps/studio/src/web/start.ts` and
`apps/studio/tests-web/web-demo.spec.ts`. Canvas pinch and mobile toolbar
behavior belong to #334 / PR #338, not this task.

Owner-only verification: physical iPhone Safari portrait and landscape with
expanded/collapsed browser bars, long audio errors, and Try again. Chromium
Playwright is not a substitute for iOS Safari evidence.

```acceptance
tier: T1
size: <= 200 changed lines, <= 3 files
must_pass:
  - ci: web-demo site job green, including narrow/short splash Playwright test
  - ci: web job typecheck, build and Playwright
  - bash -c "git diff --check"
stops: [audio-engine behavior change, global accessibility zoom change, new dependency, test removal, threshold loosening]
```
