# Wirescape-UI

Design system and reference implementation for the Wirescape modular synth interface (browser UI, Raspberry Pi first).

- **Start here:** [AGENTS.md](AGENTS.md), then [docs/PLAN.md](docs/PLAN.md)
- **Next:** [Phase 7 Raspberry Pi UI pretest](docs/PI_UI_TEST_PLAN.md), using the [dedicated Pi UI runner](docs/PI_UI_RUNNER.md) and review notes [from 3.5–6](docs/PRE_PI_READINESS_REVIEW.md); [Phase 8 native engine handoff](docs/PHASE8_HANDOFF.md).
- **Spec:** [docs/DESIGN.md](docs/DESIGN.md)
- **Reference kit:** [ui/wirescape-ui-kit.html](ui/wirescape-ui-kit.html) (open in Chromium)
- Performance: [docs/PERFORMANCE.md](docs/PERFORMANCE.md) · Decisions: [docs/DECISIONS.md](docs/DECISIONS.md) · Archive: [docs/ARCHIVE.md](docs/ARCHIVE.md) · References: [docs/REFERENCES.md](docs/REFERENCES.md)
- **Run the app:**
  ```
  npm ci && npm run dev    # Vite dev server
  npm test                 # Playwright (Chromium at /opt/pw-browsers/chromium or $WIRESCAPE_CHROMIUM)
  ```
  App source is in `src/`, tests in `tests/`; see [AGENTS.md](AGENTS.md#running-the-app).
- Exploration sketches (archived): [archive/sketches/](archive/sketches/)
