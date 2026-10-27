# M0-1: Local cleanup and clean tree for GitHub (Codex, local only, no push)

**Goal:** produce `../wirescape_clean/`, a cleaned copy of this repo ready to become the first commit of `github.com/kalharbi11/wirescape`. The current folder and its git history stay untouched as a local archive.

The planner has already written the new top-level files in this folder:
- `AGENTS.md`, `CLAUDE.md`, `README.md`, `TRADEMARKS.md`, `CONTRIBUTING.md`, `.gitignore`
- the GitHub files in `docs/tasks/github_files/`. Remote tools can't write `.github/`, so in the clean tree place them as `.github/copilot-instructions.md`, `.github/pull_request_template.md` and `.github/ISSUE_TEMPLATE/task.md`, and don't copy the `github_files/` folder itself
- `docs/PLAN.md`, `docs/specs/ABI2_PLAN.md`, `docs/specs/COMMUNICATION_MODULES_PLAN.md`, `docs/specs/AUDIO_CAPTURE_HARNESS.md`
- `docs/tasks/*`
- copies of the old README, AGENTS, CLAUDE and START_HERE in `docs/history/`

**Use these as they are. Don't rewrite them.** If one contradicts the code, report it; don't silently edit.

## Steps
1. **Commit and archive.**
   - Commit the planner's new files in this repo: "Planning docs for GitHub migration".
   - Create `../wirescape_archive_2026-10-26.bundle` with `git bundle create … --all` and check it with `git bundle verify`.
2. **Privacy audit** (report only, delete nothing). List tracked and historical files matching:
   - fonts (`*.ttf`, `*.otf`, `Dinamo*`)
   - `user_data/`, `local_backups/`, `.tools/`, `surgepy_bin/`, `surge/`
   - `*.log`, `*.zip`, `incoming_modules/`, `deliverables/`
   - anything that looks like a credential

   Also list blobs over 1 MB in history. Confirm that none of them go into the clean tree.
3. **Build `../wirescape_clean/`.** Copy, never move. Copy only what this map allows:

| Destination | Source |
|---|---|
| repo root | `AGENTS.md`, `CLAUDE.md`, `README.md`, `LICENSE`, `TRADEMARKS.md`, `CONTRIBUTING.md`, `.gitignore`, plus `.github/` built from `docs/tasks/github_files/` as described above |
| `native/` | `native/`, without build outputs |
| `tools/` | Only what `tools/dev.py`, CMake, the tests or packaging import or invoke (check with grep). Move the other scripts (for example `verify.py`, `ui_smoke.py`, `ui_module_smoke.py`, `check_modules.py`, `scan_surge_choices.py`, `build_essentials_kit.py`) to `legacy/tools/` |
| `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/`, `docs/tasks/` (without `github_files/`) | As written by the planner. `docs/ABI2_PLAN.md` and `docs/COMMUNICATION_MODULES_PLAN.md` are now "moved" stubs: don't copy them anywhere. Also move `docs/EMBEDDED_PORTABILITY.md` and `docs/PROVIDER_BRIDGE_CONTRACT.md` into `docs/specs/`, and fix any links inside them |
| `docs/ARCHITECTURE.md` (new) | Merge `docs/CURRENT_BUILD.md` and `docs/ARCHITECTURE.md`, native app only. Describe `legacy/` in one paragraph. Use repo-relative commands and paths. Keep it under 200 lines |
| `docs/STATUS.md` (new) | From `docs/CURRENT_EVIDENCE.md`: the evidence table and known limits. Note that the original report files were local and are not in the repo |
| `docs/ROADMAP.md` (new) | From `docs/ESSENTIALS_MODULE_ROADMAP.md`. Update the MIDI/OSC/UDP/Monitor rows to match `docs/specs/COMMUNICATION_MODULES_PLAN.md` section 4: a single Monitor replaces MIDI Monitor, and add MIDI→CV, MIDI CC Map and MIDI Clock |
| `docs/design/` | `design_reference/WIRESCAPE_Studio_Original.html` |
| `docs/history/` | Every other `docs/*.md` and `docs/*.json`, existing `docs/history/`, `BUILD_STATUS.md`, `HOTFIX_0.5.2.md`, `INSTALL_WINDOWS_REPLACE.md`, `START_HERE_CURRENT_BUILD.md`, `essentials_build_guide/` and `handoff/`. No zips |
| `legacy/` | `wirescape/`, `web/`, `modules/` (WITHOUT `chaos/surgepy_bin/` or `chaos/surge/`), the old Python `tests/`, `examples/`, `module_build_guide/`, `requirements.txt`, `run_*.sh/.bat`, `setup_*.sh/.bat`, `tests_sample.py`, and `docs/history/README_LEGACY_0.5.md` copied as `legacy/README.md`, with this note at the top: "Reference only, not built or maintained. The Surge Python binary isn't included." |
| Root `*.py` duplicates (`__init__`, `__main__`, `audio`, `graph`, `mapping`, `modulation`, `plugins`, `schema`, `server`) | Compare with `wirescape/`. Drop identical copies. Put differing ones in `legacy/root_copies/` and report them |
| **Never copied** | `.git/`, `build/`, `build_reports/`, `local_backups/`, `.tools/`, `.venv/`, `__pycache__/`, `user_data/`, `incoming_modules/`, `deliverables/`, `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/`, `aqtinstall.log`, `*verification_results*.json`, any `*.log` / `*.zip` / font, `surgepy_bin/`, `surge/` |

4. **Remove local machine references** from every file in the clean tree:
   - Search for `C:/Users`, `C:\Users`, `Khalid`, `sound_design` and other absolute drive or home paths. Replace them with repo-relative paths or environment variables. `docs/history/` may keep them inside quoted history, and the M0-1/M0-2 task files may name them as search patterns. Nothing else may contain them.
   - Qt discovery in `tools/dev.py` and CMake: use `WIRESCAPE_QT_DIR`, then `CMAKE_PREFIX_PATH`, then the fallback `.tools/qt/6.10.3/msvc2022_64`.
   - The private font loads only through `--local-font` or an env var pointing at an untracked path. The app must start normally without it.
   - Parameterize `tools/install_windows_shortcuts.ps1` (package path argument) and remove hard-coded package folder names.
5. **Fix references to moved files.** Update all links and paths in kept docs, tests and tools that point to moved files. Nothing outside `docs/history/` and `legacy/` may reference `START_HERE_CURRENT_BUILD.md`, `docs/CURRENT_*.md`, `docs/BUILD_CHECKPOINT.md`, `docs/NEXT_TASK_QUEUE.md`, `essentials_build_guide/` or `module_build_guide/`.
6. **Add `LICENSES/`:**
   - `GPL-3.0.txt`, a copy of `LICENSE`
   - `MIT.txt`, the standard MIT text with `Copyright (c) 2026 Wirescape contributors`
7. **Dependencies.** If the build needs a binary that `.gitignore` now excludes (for example the PortAudio library), don't commit it. Make `tools/dev.py` find it outside the repo (an env var, or `.tools/`), and write down in the report how CI should get it (vcpkg, a package manager, or building from source).
8. **Verify in `../wirescape_clean/`**, pointing `WIRESCAPE_QT_DIR` at this folder's existing `.tools/qt/…`:
   - `python tools/dev.py doctor`, `build` and `test` pass
   - `python tools/dev.py launch` opens the GUI with audio off
   - `git status` in a temporary `git init` shows no ignored or private files, then delete that temporary `.git`
9. **Report.** Write `../wirescape_clean/docs/history/MIGRATION_REPORT_2026-10-26.md` (no private details) and post a short summary covering:
   - what was moved, dropped and excluded, and why
   - the audit results
   - test results
   - file count and size
   - open questions

   Then **STOP.** Don't `git init` for real and don't push. That's task M0-2.
