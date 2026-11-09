<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Contributing to Wirescape

People and AI agents use the same [workflow and engineering rules](AGENTS.md).
The product is Electron + Studio + the native `wirescape-engine`; start with
[the source-build quickstart](README.md). [Board #473](https://github.com/kalharbi11/wirescape/issues/473)
and the latest issue handoffs govern current claims. Finish existing work before
starting features while the plan's D18 limit applies.

1. Claim the issue before editing. Use one `task/<issue#>-<slug>` branch and one
   PR with `Closes #<issue#>`. Open it as a draft until its acceptance is green.
2. Read the named task file and its acceptance block, then the relevant spec.
   Use [the SDK guide](docs/SDK_AUTHOR_GUIDE.md) for modules; keep existing module,
   port and parameter IDs and preserve unknown patch data.
3. Build with `python tools/dev.py build --headless --config Release` and test
   with `python tools/dev.py test --config Release` (`python3` on the Pi).
   Run the task's `python tools/dev.py accept <issue#>` commands as well.
   Studio tests use `npm ci && npm test` in `apps/studio`; they require Chromium
   and Playwright's FFmpeg, as provisioned in the Web workflow.
4. Keep automated audio/MIDI tests on fake devices or the null sink. Physical
   checks are owner-run, behind explicit confirmation. Never use an old install
   or generated artefact as evidence for a different source revision.
5. Sign commits with `git commit -s` under the [DCO](https://developercertificate.org/).
   Add SPDX headers to new files; SDK/ABI 2/voice paths are MIT and application
   code is GPL-3.0. Record third-party revisions and licences in `DESIGN.md`.
6. Post exact-head evidence and a signed handoff. Independent review and required
   CI/task/Pi checks must pass before merge. External/incomplete checks are not passes.

Never commit generated assets, binaries, fonts, credentials, user data or
machine-specific paths. Optional hooks use `pre-commit install` and
`pre-commit run --all-files`; changed C++ lines use LLVM18 formatting. Hooks
warn about protected paths locally, while CI enforces their authorization.
