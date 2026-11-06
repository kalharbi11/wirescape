<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1A (#229): import the web UI as `apps/studio/` + web CI (Phase 1, parallel with P1.0)

Decision D2: the UI moves into this repo with its history.

**Steps:**
1. `git subtree add --prefix=apps/studio https://github.com/kalharbi11/Wirescape-UI.git main` (no `--squash`; keep the history). Pin the imported SHA in `apps/studio/IMPORT.md`.
2. Rename the imported `apps/studio/CLAUDE.md` to a one-line pointer to the root `AGENTS.md`, and trim `apps/studio/AGENTS.md` to the UI-specific parts (design system, performance rules, kit map). Remove anything that duplicates root workflow rules.
3. Leave the imported Pi-UI workflow inactive: move `apps/studio/.github/` to `apps/studio/tools/ci-reference/`, since GitHub only reads the root `.github/`. Add a root workflow `.github/workflows/web.yml` that runs on PRs touching `apps/studio/**` or `docs/specs/` (the engine protocol from P1.0) or `apps/desktop/**`, on `ubuntu-latest`: `npm ci`, typecheck, `npm run build`, `npm test` (Playwright with the runner's Chromium), and uploads screenshots and traces as artifacts on failure. Pin the Node major version.
4. Root `.gitignore`: add `apps/studio/node_modules/`, `apps/studio/dist/`, `apps/studio/test-results/`.
5. Make `guard` understand the new tree: `apps/studio/**` is normal code. Licence headers: GPL-3.0 SPDX on UI source files that lack one (in the same PR).
6. Update `README.md` and `docs/ARCHITECTURE.md` (new "Web UI" section), and add a line to the UI repo's README pointing here. The owner archives the UI repo after the merge.

```acceptance
tier: T1
size: n/a
must_pass:
  - ci: web job green on exact head (typecheck, build, Playwright)
  - ci: linux-arm64 and guard green
stops: [test removal, new dependency beyond the UI's existing package.json, binary files committed]
```
Protected path named: `.github/workflows/web.yml`
