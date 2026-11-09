<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi setup review and preparation evidence

Parent #436. This session independently reviewed the existing builders' work;
it did not take over their source files, install anything on a Pi, or test
hardware. The owner's first Windows session is complete; #463 carries its runtime repair.

## 2026-11-09 current disposition

#463 merged `3fa0d3a`, #450 merged `fcf38b9`, and #452 merged `20aabc0`.
The earlier cache/read-only, profile/settings-path and malformed-ELF findings
below were repaired and independently reviewed before their merges.

Actual Pi evidence: [#450 `1074cfa` preflight28/28](https://github.com/kalharbi11/wirescape/pull/450#issuecomment-6741944378);
[#452 `e6df3d2` ARM64 Electron/native device-off smoke](https://github.com/kalharbi11/wirescape/pull/452#issuecomment-6746232670).
Owner confirmed Node22.23.4/npm10.9.9 and Xvfb/xauth. These are separate source
heads; they do not certify the combined main tree or a fresh installation.

#483 supplies the remaining acceptance on this existing #453 branch. Integrate
main with the #485/#486 combined-test compile repair, obtain actual preflight
and app smoke on one docs-only head, verify its non-doc tree matches its main
parent, then pin that full parent SHA under
[Keel's ruling](https://github.com/kalharbi11/wirescape/pull/453#issuecomment-6746264774).
The accepted-SHA placeholder stays until that result exists. The old PI_SETUP
first-run entry now points to the same guarded runbook; neither guide recommends
an unverified main ref or desktop-entry install during the isolated first run.

## Historical source checks (not current combined acceptance)

## Source and executed checks

| PR / source head | Executed here | Result and limit |
|---|---|---|
| #450 / `15ccb578c84eb2ebbf894670c082dc5acbbe44e3` | `python3 tools/foundation/tests/test_pi_preflight.py` | 11 passed, cloud mocks |
| same | `python3 -m unittest discover -s tools/foundation/tests -p test_pi_update.py` | 7 passed |
| same | `python3 -m unittest discover -s tools/foundation/tests -p test_build_parallel.py` | 1 passed |
| #452 / `53c1fe7ea77d1c1d4a5e5ff6f32b22516a912e04` | `python3 tools/foundation/tests/test_pi_app_smoke.py` | 7 passed, cloud mocks |
| same | pinned Studio/Desktop `npm ci`, then `npm --prefix apps/desktop run build` | Studio production build and desktop TypeScript passed; Electron binary download skipped for this static check |
| same | `node --check apps/desktop/tests/pi-profile.spec.cjs` | Syntax passed; no live Electron/ARM64 launch |

The first npm attempt failed because its default cache was outside the writable
workspace. Retrying with a checkout-local cache succeeded; no lockfile, hash or
TLS verification was relaxed. All source checkouts remained clean.

## Historical independent findings (repaired before merge)

[#450 COMMENT review](https://github.com/kalharbi11/wirescape/pull/450#pullrequestreview-5821811049):

- P2: `preflight.py:112–120` accepts a same-source aarch64 cache with
  `WIRESCAPE_BUILD_UI:BOOL=ON` and a different generator if Qt path values are
  absent. Temporary-fixture `inspect()` reproduction returned PASS. The merged repair adds
  the UI/generator checks and regressions.
- P2: first `--check-only` execution imports `helpers` and creates
  `tools/foundation/__pycache__`. A fresh temporary copy of the real scripts
  started with no cache directory and gained one even when preflight exited
  blocked. The merged repair disables bytecode writes before local imports and tests
  a fresh subprocess.

[#452 COMMENT review](https://github.com/kalharbi11/wirescape/pull/452#pullrequestreview-5821811415):

- P1 / High: `main.ts:49–55` validates only the first `--settings` and only a
  leaf symlink. Two settings flags (inside, then outside) are accepted although
  the engine uses the last; an inside path through a symlinked parent is also
  accepted. Both were reproduced against the compiled startup code with mocked
  Electron and a real temporary filesystem. No engine or device was launched.
  The merged repair rejects ambiguous arguments and validates real parent paths;
  portable negative regressions cover both escapes. This was a merge blocker.
- P2: `app_smoke.py:36` indexes a short ELF header. A four-byte ELF magic input
  raises `IndexError`, bypassing normal failed/blocked JSON reporting. The merged repair requires a complete header and regresses truncated artifacts.

The reviewers did not change implementation files while builders owned those
fixes. The PRs integrated main normally and later merged after required checks
and actual target results. Historical failed runs remain evidence of the defects;
no tolerance, required check or prerequisite was weakened.

## Remaining work

1. Integrate current main into this guide branch, including #485/#486.
2. Keep `Closes #483`, `ci/pi` and `ci/pi-accept`; inspect both exact-head
   preflight and app-smoke results, not only a queued/public build status.
3. Verify docs-only delta versus the main parent before transferring passing
   evidence to that full immutable parent SHA in the owner runbook.
4. Rerun final required hosted gates; owner sound/MIDI/display checks follow
   separately in #271. #274 performance and #296 soak stay separate.

No current combined Pi install pass, device result, package or release claimed.
Signed: Codex (Pi readiness reviewer and owner-guide builder), 2026-11-08 America/New_York.
Updated: Codex (delegated Pi guide repair cooperator), 2026-11-09.
