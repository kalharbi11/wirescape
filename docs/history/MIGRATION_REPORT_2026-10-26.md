<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M0-1 migration report — 2026-10-26

**Status:** done, local handoff ready. No GitHub access, remote, push, PR, label or issue operation was performed.
The clean copy has **no .git directory**. M0-2 belongs to Copilot after the user reviews this report.
Verification ran on October26 local time / October27 UTC; the filename follows the assigned task.

## Backup and original workspace

- Original project remains in `wirescape/`; no data/source directories were moved or deleted.
- Planner files committed as `303b15d` (Planning docs for GitHub migration).
- A planner handoff and two small document changes arrived during cleanup. They were preserved
  in `b6b4f6c12948518bae697df196df9355e4279486` (Include planner handoff in GitHub migration), then copied verbatim to the clean layout.
- `wirescape_archive_2026-10-26.bundle`, beside the original project, was recreated with `git bundle create --all`
  after those updates and passed `git bundle verify`. It records complete history, including the local baseline tag.
- Bundle SHA256: `53db8ccf5c7f94721d151dd4ef5355d6090e20d1ab4f40f1654e7383db00483b`. The first verified bundle is also retained as
  `wirescape_archive_2026-10-26_initial.bundle`. These are **local archives**, never inputs to the public first commit.
- Original Git worktree was clean at report time. Ignored private data and original SDK/Surge files remain untouched.

## Clean layout and selection

- Root planner files and `docs/PLAN.md`, `docs/PLANNER_HANDOFF.md`, `docs/specs/`, `docs/tasks/` copied as written.
  The staged comparison checked 16 planner files byte-for-byte.
- GitHub files copied from staging into `.github/copilot-instructions.md`,
  `.github/pull_request_template.md`, `.github/ISSUE_TEMPLATE/task.md`. The staging `github_files/` folder is absent.
- `native/` keeps source, headers, metadata, public synthetic fixtures and PNG/ICO source icon assets.
  Compiled binaries and the two generated ABI2 WAV fixtures were not copied.
- Active tools: `dev.py`, `foundation/` and tests, `experiments/reference_kernels.py` (imported by its test),
  `bench/abi2_foundation.py` (native acceptance collector), and the parameterized Windows shortcut installer.
- `verify.py`, UI/browser smoke scripts, module/Surge scans, old kit generator and old Python benchmark
  scripts are in `legacy/tools/`. They are reference-only and not part of current CI/build discovery.
- New `docs/ARCHITECTURE.md` describes only the native architecture, with one legacy overview paragraph;115 lines.
  `docs/STATUS.md` consolidates evidence/limits. `docs/ROADMAP.md` replaces MIDI Monitor with the single
  general Monitor and adds MIDI→CV, MIDI CC Map and MIDI Clock per the communication plan.
- Embedded portability and provider contracts moved to `docs/specs/` with active references fixed.
  Original design HTML is in `docs/design/`. Other documents/kits are in `docs/history/`, without ZIPs.
- Legacy Python/browser modules/tests/examples/build guide and launch/setup scripts are under `legacy/`;
  Surge source/binary folders are absent. Its README explicitly says reference-only, not built/maintained.
- `LICENSES/GPL-3.0.txt` is byte-identical to LICENSE; MIT.txt uses the standard notice with
  `Copyright (c) 2026 Wirescape contributors`.

Root duplicate handling (content comparison with the original wirescape package):

- `__init__.py`: retained differing copy under legacy/root_copies/
- `__main__.py`: dropped identical copy
- `audio.py`: dropped identical copy
- `graph.py`: retained differing copy under legacy/root_copies/
- `mapping.py`: retained differing copy under legacy/root_copies/
- `modulation.py`: dropped identical copy
- `plugins.py`: retained differing copy under legacy/root_copies/
- `schema.py`: retained differing copy under legacy/root_copies/
- `server.py`: retained differing copy under legacy/root_copies/

## Privacy/history audit

Both tracked paths and all reachable historical paths/blobs were audited, including refreshed refs after
the planner update. No known token/private-key pattern matches were found in text-like historical blobs
up to3MiB; credential-like filenames were also checked. This is a bounded pattern scan, not a guarantee
that arbitrary prose can never contain sensitive information. No credential values were printed or copied.

- Fonts (`ttf/otf/woff/woff2`, Dinamo names): **0 tracked/historical matches**; no font files copied.
- `user_data/`, `local_backups/`, `.tools/`, Surge sources/binaries: **0 tracked/historical matches**;
  any existing ignored local folders were excluded by the allowlist, not removed from the original.
- Credential filename/token matches: **0**. Historical blobs over1MiB: **0**.
- The13 flagged artifact paths below are the same current-tracked and historical set. None goes to the clean tree.
  The benchmark directory was excluded as an extra environment/report privacy precaution.

| Artifact path | Tracked | Historical | Clean tree |
|---|---|---|---|
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/BENCHMARK_REPORT.md` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/baseline_tests.json` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/chaos_performance.csv` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/environment.json` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/graph_performance.csv` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/memory.csv` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/module_architecture_audit.md` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/optimization_recommendations.md` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/profiling_summary.txt` | Yes | Yes | Excluded |
| `WIRESCAPE_WINDOWS_BASELINE_BENCHMARK/realtime_audio_audit.md` | Yes | Yes | Excluded |
| `deliverables/Wirescape_Essentials_Agent_Kit_2026-10-26.zip` | Yes | Yes | Excluded |
| `deliverables/Wirescape_Essentials_Agent_Kit_2026-10-26.zip.sha256` | Yes | Yes | Excluded |
| `handoff/essentials_mixer_task.zip` | Yes | Yes | Excluded |

Also excluded: generated WAVs, all log/archive/font/compiled-object/runtime extensions, caches,
the old verification-result JSONs, SDKs, private data, backups, incoming module staging and deliverables.
No file over1MiB was copied. No unverified download, install, security-setting change, device probe or socket test was performed.
The detailed local audit inventory is retained only under the original workspace's ignored local_backups/.

## Paths, references and dependencies

- Personal project/home prefixes were removed from copied text. Historical copies were sanitized too;
  the original archive retains exact historical originals. No active file contains a machine-specific user/home path.
  Only M0-1/M0-2 intentionally contain literal privacy search patterns. The planner README's generic
  `C:\path\to\Qt` example is unchanged; it is a placeholder, not this computer's SDK location.
- Active links/code no longer reference the removed CURRENT/BUILD_CHECKPOINT/NEXT_TASK/START_HERE paths
  or old guide locations. Archived documents/legacy and explicit migration search-pattern instructions are exempt.
- Qt discovery: explicit --qt-prefix; otherwise WIRESCAPE_QT_DIR, CMAKE_PREFIX_PATH, exact local fallback
  `.tools/qt/6.10.3/msvc2022_64`. CMake supports the same environment/fallback route. Added an environment-priority
  regression test; invalid explicit paths cannot fall back silently.
- Private fonts load only via the app's explicit --local-font flag. Startup passed with the system fallback,
  without the optional font. The Windows system-font path now derives from WINDIR, not a literal drive path.
- Shortcuts already accepted a mandatory package argument and optional local font; no fixed package version remains.
  No shortcuts were changed during this task.
- Native PortAudio already loads via WIRESCAPE_PORTAUDIO or a runtime beside the executable. Packaging now accepts
  that environment variable or `.tools/portaudio/libportaudio64bit.dll`, in addition to --portaudio / built-app lookup.
  No backend DLL is committed/copied as source. Offline build/tests/startup need no physical backend stream.
- **CI provisioning for M0-3:** acquire Qt separately and set its environment prefix; build a pinned upstream
  PortAudio revision (or obtain it from a pinned package-manager baseline) for Windows packaging and set
  WIRESCAPE_PORTAUDIO to the resulting DLL. Pin revision/architecture/compiler/licence notices in that CI task.
  No revision or installed backend version is invented here; no download was needed locally.
- ABI2 reference WAVs are generated during offline tests; their frozen SHA256 values remain in the public
  fixture manifest and are checked without shipping WAV files. The evidence tool reads the relocated history
  documents and a separately labelled M0 clean-copy source manifest. It does not rewrite the original ABI2
  preservation snapshot or pretend the cleanup made no source changes.

## Verification in wirescape_clean

The external existing Qt SDK was selected via WIRESCAPE_QT_DIR; it was not copied into the clean source.
All checks kept audio off, with no MIDI device or LAN socket opened.

| Check | Result | Ignored local evidence |
|---|---|---|
| python tools/dev.py doctor | READY | doctor-20261027T010628Z.json |
| python tools/dev.py build | PASS, fresh MSVC Release UI+engine build | build-20261027T010703Z.json |
| python -m unittest discover -s tools/foundation/tests -q | PASS,28 tests | Console summary; includes Qt discovery regression |
| python tools/dev.py test | PASS,28/28 CTest suites | test-20261027T010753Z.json |
| python tools/dev.py ui-test | PASS,1400×900 and1000×650 | ui-test-20261027T010756Z.json |
| python tools/dev.py launch | PASS,normal window ready,audio_started=false | launch-20261027T010757Z.json |
| Debug abi2_engine_tests + python tools/bench/abi2_foundation.py | PASS functional,hash-only WAV references and Debug CRT | build_reports/abi2/summary.json |
| Temporary Git init/add/status privacy check | PASS;429 candidate files,zero private/path/credential matches | Local audit inventory only; temporary .git removed |

New GUI was launched with no --local-font and remains a normal audio-off window. No existing app was closed.
Local generated test reports contain paths and remain ignored; none is a candidate for the public first commit.
The original ignored report directories were not copied. The old mono performance miss remains a separate
M1 blocker (clean-copy measurement ratio 1.861× ABI1); cleanup does not resolve or hide it.
No ARM/Linux/Daisy, clean-Windows, output capture, sustained audio or public CI validation is claimed.

## File count / size

- Final public-source candidate: **430 files**, **2,810,517 bytes** (2.68MiB), including this report.
- Local folder including verification-generated ignored build/reports/caches: approximately **2,008 files**,
  **65,081,343 bytes** (62.07MiB) at the audit snapshot. These generated artifacts are not copied
  private data and are excluded by .gitignore. There are no font files, user_data, external SDK/tool caches or backup directories in the clean copy; native/sdk contains the intended public SDK source.
- No real Git history/remote exists in wirescape_clean. The only Git init was the temporary source audit;
  its marker and resolved path were checked before deleting only its .git directory.

## Open questions / planner inconsistencies (left unchanged as instructed)

1. M1-1 is written as a new foundation build, but an isolated foundation already exists. Review/iterate it,
   especially performance/overflow/timestamp/latency semantics, rather than duplicating it. ABI1 app remains separate.
2. CONTRIBUTING still names `docs/SDK_AUTHOR_GUIDE.md` and says the SDK template becomes available after M1.
   The guide is now `docs/history/SDK_AUTHOR_GUIDE.md`; the experimental template already exists.
   Planner-owned root files were preserved verbatim, so this needs a planner-approved wording/link correction.
3. README's ARM headless command is a recipe, not verified Pi/ARM evidence. STATUS records that distinction.
4. Newly supplied PLANNER_HANDOFF includes user workflow, subscription and device context. It was copied
   verbatim as directed; review whether to keep that context public before M0-2.
5. Public CI dependency pins and any codec/backend licence/version inventory remain M0-3 work. No binary vendoring.

## Local handoff

**Status:** done / handoff-ready. **Branch/PR:** none (local-only preparation).
**Last original-repo commit:** `b6b4f6c12948518bae697df196df9355e4279486`. **Clean-repo commit:** none.
**Next:** user reviews privacy audit/report, then Copilot executes docs/tasks/M0-2_GITHUB_PUSH.md.
**Tests:** table above. **Gotchas:** planner wording/link issues and existing ABI2 performance gate.
**Follow-ups not done:** push, labels/milestones/issues, CI, app ABI migration and new features.

STOP — M0-2 was not started.
