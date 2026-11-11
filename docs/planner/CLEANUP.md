<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Qt retirement and repository cleanup (Compass plan, 2026-11-09)

Source: owner direction in #437 (Qt is permanently retired) and #436 (Windows-first install). Plan decisions D15–D19 in `docs/PLAN.md`. This file is the sequence; Codex Lead splits each packet into issues with acceptance blocks. Keel rules on anything marked T2. Keep this file short and tick packets as they merge.

## Why
The repo carries three products' worth of code for one product: the Qt app (`native/ui/`, a QtCore document library, Qt CI, Qt launcher tooling), the ABI1 engine and its 13 modules (each re-ported in `native/modules2/`), and 192 archived files (`legacy/`, `docs/history/`). Agents keep paying for it: Qt macro-collision issues (#370, #416), Qt in static analysis (#435, #361), a 1,000-line `tools/dev.py` built around the Qt launcher, and a README that still tells newcomers to install Qt 6.10. The goal is one obvious product, one setup path per platform, and a small read map.

## Verified inventory (main `659b960`)
| What | Where | Blocks deletion? |
|---|---|---|
| PortAudio header + licence used by the **new** audio service | `native/ui/third_party/` ← `native/io/audio/portaudio_service.cpp:3` | Yes: move first (R1) |
| Qt UI build on by default | `native/CMakeLists.txt` (`WIRESCAPE_BUILD_UI` ON, Qt prefix discovery) | Flip default (R1) |
| QtCore document library (v1/v2 reader, writer, migration tests) | `native/document/{document_reader,patch_document,app_patch}.*`, `native/tests/{document_reader,patch_document}_tests.cpp`, fixtures `native/tests/fixtures/legacy_documents/` | Yes: swap coverage first (R2) |
| Soak document mode links QtCore | `native/engine2/CMakeLists.txt:206-210`, `native/engine2/tests/soak.cpp:102` | Yes (R2) |
| v1 import already Qt-free | `apps/studio/src/patch/PatchDoc.ts:62,253` (`migrateV1`); engine reads v1 execution copies in `runtime_patch` | Reuse (D16) |
| Qt launcher / package tooling | `tools/dev.py` (launch, ui-test, startup-test, package, updates), `tools/foundation/{launcher_runtime,package_inventory}.py` + tests, `tools/install_windows_shortcuts.ps1` | R4 |
| Qt in CI | `.github/workflows/ci.yml` (Windows Qt install, `wirescape-ui-enabled.txt`), `static.yml`, `codeql.yml`; private `wirescape-pi` `laptop.py`/`laptop.yml` Qt mode | R3 |
| ABI1 engine path | `native/src/engine.cpp`, `native/modules/*`, ABI1 bridge in `native/engine2/engine2.cpp` (`add_abi1`, `rebind_abi1_descriptor`), `native/control/app_engine.cpp` (`legacy` engine, `app_engine_load_module`), `native/cli/play.cpp:69`, `ws_engine_config` used as a config struct in sidecar and web, ABI1 links in `abi2_benchmark`/`abi2_integration_tests`/`abi2_cost_probe`/`abi2_returned_filter_stress`, `tools/capture/reference.py` | Separate track (R6, D17) |
| Windows sidecar SKIP 77 | fixed by #439 (merged) | No longer a blocker |

## Status and order (Compass, 2026-11-11, for v1.1)
The owner runs the v1.0.0 tag for a week, so cleanup lands on `main` alongside that. Builders are independent agents (ChatGPT/Codex). Leads come in only for a Stop. All Qt T2 work goes to Coda in **one** batch review.
| # | Packet | Tier | Builder → reviewer | Depends on |
|---|---|---|---|---|
| 1 | **R3a** #482: product goldens without Qt (draft; finish exact-head CI) | T2 | existing Codex writer → Coda (batch) | — |
| 2 | **R3b**: delete `native/ui/`, the QtCore document targets, Qt in `ci.yml`/`static.yml`/`codeql.yml` and `--library=qt` in `tools/ci/static_checks.py`. Set `archive/qt-final` on the merge parent. | T2 | any agent → Coda (same batch) | R3a |
| 3 | **#571** cppcheck Qt model: **folded into R3b** (deleting Qt removes the model); close #571 when R3b merges | — | — | R3b |
| 4 | **R5c**: Qt/pre-1A clauses out of `AGENTS.md`/`CLAUDE.md`; remove `legacy/` and `docs/history/` (label-only, Compass applies `allow-protected`) | T0 | any agent | R3b |
| 5 | **R4** #481: `tools/dev.py` around Electron, < ~500 lines | T1 | any agent → another session | R3b |
| 6 | Task sweep: move task files for closed issues to `docs/tasks/done/` and refresh the index | T0 | helper | none (start now) |
| 7 | **R6** ABI1 retirement #470 | T2 | Keel-led | v1.2 |
| 8 | **R7** renames | — | Keel decides | after R6, optional |
`allow-test-removal` for Qt-only tests is pre-authorized (Keel), provided the R3b PR body carries a replacement table (removed test → the Qt-free test covering the same guarantee). Required check names never change.

## Packets (one issue each; small PRs)
- [x] **R0 Decisions** (Compass; merged by Keel 2026-11-09): D15–D19, Phase 4 rewrite, this file. The `archive/qt-final` tag (lightweight, never moved) is set on R3's base commit just before R3 merges (Keel ruling: the archive should be the last tree that still has Qt).
- [x] **R1 Qt-free by default** [#464, merged #476] (T1, now, parallel with #436 W1). Move `portaudio.h` + `PORTAUDIO_LICENSE.txt` to `native/third_party/portaudio/`; update `native/io/audio/portaudio_service.cpp`, `native/io/audio/DESIGN.md`, `native/README.md`. `WIRESCAPE_BUILD_UI` defaults OFF and Qt discovery runs only when it's ON. CI jobs that build Qt today pass `-DWIRESCAPE_BUILD_UI=ON` explicitly until R3, so no coverage disappears silently. Accept: a fresh build directory on a machine without Qt passes `ctest`; `grep -rn "ui/third_party" native` is empty.
- [x] **R2 Document coverage swap** [#465, merged #478] (T2, Keel review). Assert every fixture in `native/tests/fixtures/legacy_documents/` (v1 migrate, v2, v3 unknown-field preservation, v99 refusal, malformed) against the Studio codec (`PatchDoc.ts`, Vitest/Playwright) **and** against `runtime_patch` for the engine copy. Point the soak's document mode at `wirescape_runtime_document`. Keel confirms that the new tests cover everything the QtCore tests asserted. Nothing is deleted in R2.
- [ ] **R3 Delete Qt** [#468] (protected; owner/Compass/Keel set `allow-protected`, owner sets `allow-test-removal` for Qt-only tests). After R1, R2 and a working #436 W1 smoke. Remove `native/ui/`, the `WIRESCAPE_BUILD_UI` option and Qt discovery, the QtCore half of `native/document/CMakeLists.txt` and its sources/tests, and Qt from `ci.yml`, `static.yml` and `codeql.yml`. The required check names (`guard`, `linux-arm64`, `windows-x64`, `sanitizers`) stay the same. Separate PR in `wirescape-pi`: drop the laptop runner's Qt mode. Close #370 and #416 as not planned, and rescope #435 and #361.
- [ ] **R4 Tools** [#469] (T1, with or after R3). In `tools/dev.py`, `launch` starts the Electron app (null audio unless `--confirm-*`), and the Qt `ui-test`, `startup-test`, `package` and `updates` commands go. Keep `doctor`, `build`, `test`, `accept` and `check`, plus their reporting and hashing. Remove `launcher_runtime.py`, `package_inventory.py` and their tests. The #436 W1 script replaces `install_windows_shortcuts.ps1`. Target: `dev.py` under about 500 lines.
- [ ] **R5 Docs and read map** [#466 (R5a docs), #467 (R5b prune/archive); R5c AGENTS/CLAUDE + legacy/ goes with #468] (T0; Compass reviews; can start now).
  - Rewrite `README.md` (one quickstart, "build from source" on Windows and the Pi; "download" marked not available yet), `native/README.md`, `CONTRIBUTING.md`, and `docs/STATUS.md` (current evidence only; the Oct 27 table moves to git history).
  - Replace `docs/WINDOWS_DEMO_UPDATE.md` with `docs/WINDOWS_SETUP.md` from #436 W1.
  - R5b #467 removes the stale roadmap, Pi kiosk/CHAOS demo, lanes and module-planner documents after repairing active references. The planner handoff remains unchanged until R5c/#468 because the protected-path list in AGENTS.md still names it (Codex Lead ruling). Keep `docs/DEMO_CLASS.md` until #271 closes.
  - Move task files whose issue is closed to `docs/tasks/done/`. First teach `tools/dev.py accept` to resolve both folders and add `docs/tasks/README.md` as the index.
  - `AGENTS.md` and `CLAUDE.md` (label-only): drop the Qt, pre-1A and `wirescape-ui` clauses.
  - Remove `legacy/` and `docs/history/` from the tree at the archive tag (label-only; owner sets `allow-protected`).
- [ ] **R6 ABI1 retirement** [#470] (T2, Keel leads, after R3). Freeze the ABI1-vs-ABI2 equivalence vectors and benchmark baselines as data first. Then remove the ABI1 bridge from engine2, `native/src/`, `native/modules/`, the ABI1 tests and the ABI1 header usage. `ws_engine_config` becomes `engine2::Config`. ABI header edits are label-only and come with a migration note.
- [ ] **R7 Renames** (mechanical, optional, Keel decides). Rename `native/modules2` → `native/modules`, `engine2` → `engine` and `tests2` → `tests` in one PR, during a quiet window announced in STATE a day ahead with no open native PRs. Module, port and parameter IDs don't change.

**Order:** R1 ∥ R2 ∥ R5 (docs not touching Qt code) ∥ #436 W1 → R3 → R4 → R6 → R7. Nothing here pre-empts an owner-reported failure.

## Target layout (after R7)
```
apps/studio        web UI (TypeScript)          apps/desktop      Electron shell
native/engine      ABI2 realtime engine          native/sdk        MIT module SDK + examples
native/modules     first-party modules           native/sidecar    wirescape-engine process
native/document    runtime patch (Qt-free)       native/io         audio + MIDI services
native/voice  native/web  native/cli  native/tools  native/third_party/portaudio
tools/             dev.py, ci, pi, windows, capture, bench
docs/              PLAN, ARCHITECTURE, STATUS, SDK_AUTHOR_GUIDE, WORKFLOW, PI_SETUP, PI_RUNNER,
                   WINDOWS_SETUP, specs/, tasks/ (+ done/), reviews/, planner/ (LEADS, STATE, CLEANUP)
```
Other repos: `kalharbi11/Wirescape-UI` is archived on GitHub (owner, one click in Settings, already read-only by D2). `kalharbi11/wirescape-pi` stays private, losing only its Qt mode.

## Must not
Delete user patches or settings, rewrite history, force-push, loosen a test or threshold, drop old-patch import or unknown-field preservation without the R2 replacement, open devices, or publish a release.

Signed: Compass (Claude), 2026-11-09
