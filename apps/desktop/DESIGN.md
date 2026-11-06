<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Desktop shell design — P1F / #233

Electron main is the sole owner of the native window, file selection, and supervised child.
The Chromium renderer loads built `apps/studio/dist` through a secure, standard, **local-only**
`app://wirescape/` handler. There is no HTTP server, remote page, or socket. Main refuses
out-of-root asset paths. The renderer has `contextIsolation`, no Node integration, a
sandboxed preload and the **single** `window.wirescape` object defined in
`docs/specs/ENGINE_PROTOCOL.md` §3.

Main relays bounded NDJSON between the renderer and the engine over the child's
stdin/stdout. It owns restart and shutdown, but never edits a patch. File dialogs
deliver only the patch text and display name; paths stay in main. Saves are atomic
same-directory renames. Engine confirmation flags are forwarded only when the
owner starts the desktop app with them; the test stub mandates `--null-audio`
and refuses device confirmation flags.

## Dependencies and licensing

- `electron@36.9.5`, pinned exactly, MIT, npm package
  (`https://github.com/electron/electron`). No vendor sources are copied here.
- Playwright and TypeScript are **existing** `apps/studio` toolchain dependencies,
  not new `apps/desktop` dependencies.
- `electron-builder` is D12-approved for the future installer phase, but not
  required or installed by P1F.

## Development

From `apps/studio`, run `npm ci` once. From `apps/desktop`, run `npm ci`,
`npm run build`, and `npm test` (Electron Playwright under Xvfb on Linux).
A real engine is resolved from `--engine PATH` or next to the app/fallback build
path; test launches substitute `tests/stub-engine.cjs` with `--null-audio`.
Pi and Windows user launchers explicitly supply `--confirm-audio --confirm-midi`.
