<!-- SPDX-License-Identifier: GPL-3.0-only -->
# UI import

Source: [kalharbi11/Wirescape-UI](https://github.com/kalharbi11/Wirescape-UI), branch `main`.
Imported commit: `da600d852d09415aa39e6d48106e3e58f0dd8aa9`.

Imported with `git subtree add --prefix=apps/studio` without `--squash`;
the original commits remain ancestors of this branch. Follow-up changes adapt
agent guidance, SPDX headers and hosted CI without changing UI behavior.

UI development now lives here. The owner archives the source repository after
this import merges. Historical plans and Pi runner instructions are retained as
reference; the root plan and AGENTS.md govern current work. The old workflows
are inactive examples under `tools/ci-reference/`, never public-repo Pi jobs.

Run `npm ci`, `npm run typecheck`, `npm run build`, then `npm test` here.
Set `WIRESCAPE_CHROMIUM` to the runner's Chromium executable. The tests use
FakeEngine and localhost, with no real audio or MIDI devices.
