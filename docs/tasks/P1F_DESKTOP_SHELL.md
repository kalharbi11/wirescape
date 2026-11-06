<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P1F: the desktop app shell (Electron) (Phase 1, after P1.0; parallel with P1B and P1C)

The standalone Wirescape window. It loads the built UI, starts and supervises `wirescape-engine`, and relays protocol lines. This is what the owner double-clicks on the Pi and on Windows.

**Read first:** the engine protocol spec (`docs/specs/`, from P1.0) §1–2 and §10, `docs/PLAN.md` D1, D4, D6, D12, Electron's security checklist (context isolation, no `nodeIntegration`, no remote content).

**Build (`apps/desktop/`, GPL-3.0 SPDX headers):**
- `package.json` with pinned `electron` (D12), and scripts `start`, `test` and `build-ui` (runs `apps/studio`'s `npm run build` and loads its `dist/index.html` via `file://` or a custom `app://` protocol; no HTTP server).
- `main.ts`: one `BrowserWindow` with the normal OS title bar (minimize, maximize, close). Menu: **File** (Open patch…, Save patch, Save as…, using native dialogs; Quit, Ctrl/Cmd+Q), **View** (Toggle fullscreen F11; Esc leaves fullscreen; Reload UI), **Engine** (Restart engine, Panic). Window size and position are remembered. Fullscreen at start only with `--fullscreen`. Esc and the close button always work.
- **Engine supervision:** spawn `wirescape-engine` (path from `--engine` or next to the app) with the confirm flags the app itself received (D6). Relay stdin/stdout lines both ways, with a 2 MiB cap per line. On exit, send `app.engineExited` to the renderer and restart up to 3 times in 30 s, then show an error with a Restart button. On quit, close stdin and wait 2 s, then kill.
- `preload.ts`: expose only `window.wirescape = { send, onMessage, openPatch, savePatch, platform }` through `contextBridge`.
- **Patch files:** open and save read and write whole files chosen in the native dialogs. The renderer gets the text; the UI stays the document owner (D3).
- **Launchers:** `tools/pi/wirescape.desktop.in` + `tools/pi/install-desktop-entry.sh` (owner-run, per-user, writes the paths and `--confirm-audio --confirm-midi`; never sudo). `tools/windows/Wirescape.cmd` for the laptop. Packaging (installers) is Phase 4.

**Tests:** Playwright `_electron` with a stub engine (a tiny Node script that speaks the spec's test vectors), so CI needs no C++ build. Covers: the window opens and the UI loads with the real-engine path; lines are relayed both ways; stub crash → restart → `app.engineExited` delivered; quit closes the engine; the File menu open/save round-trip (dialogs stubbed); Esc leaves fullscreen. Add the job to the `web` workflow under `xvfb-run` on the hosted Linux runner. Tests only; the owner's Pi has a real display.

```acceptance
tier: T1
size: <= 900 changed lines, <= 14 files
must_pass:
  - ci: web job green incl. apps/desktop Electron tests
stops: [network listener, nodeIntegration, remote content, new dependency beyond D12]
```
