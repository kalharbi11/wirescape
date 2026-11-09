<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi setup review and preparation evidence

Parent #436. This session independently reviewed the existing builders' work;
it did not take over their source files, install anything on a Pi, or test
hardware. Windows #439 remains a separate parked task.

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

## Independent findings sent to the existing PR owners

[#450 COMMENT review](https://github.com/kalharbi11/wirescape/pull/450#pullrequestreview-5821811049):

- P2: `preflight.py:112–120` accepts a same-source aarch64 cache with
  `WIRESCAPE_BUILD_UI:BOOL=ON` and a different generator if Qt path values are
  absent. Temporary-fixture `inspect()` reproduction returned PASS. Keel's
  explicit own-cache ruling still needs the UI/generator checks and regressions.
- P2: first `--check-only` execution imports `helpers` and creates
  `tools/foundation/__pycache__`. A fresh temporary copy of the real scripts
  started with no cache directory and gained one even when preflight exited
  blocked. Disable bytecode writes before local imports and test a fresh
  subprocess; the current in-process snapshot misses the first import.

[#452 COMMENT review](https://github.com/kalharbi11/wirescape/pull/452#pullrequestreview-5821811415):

- P1 / High: `main.ts:49–55` validates only the first `--settings` and only a
  leaf symlink. Two settings flags (inside, then outside) are accepted although
  the engine uses the last; an inside path through a symlinked parent is also
  accepted. Both were reproduced against the compiled startup code with mocked
  Electron and a real temporary filesystem. No engine or device was launched.
  Reject ambiguous arguments and validate normalized real parent paths. Portable
  negative regressions must run outside the Pi-only test. #452 marked blocked.
- P2: `app_smoke.py:36` indexes a short ELF header. A four-byte ELF magic input
  raises `IndexError`, bypassing normal failed/blocked JSON reporting. Require a
  complete header and regress truncated artifacts.

The reviewers did not change implementation files while their builders own
these fixes. PRs were retargeted to main under Keel's explicit ruling after
#449 merged; builders still need to merge current main normally and run final
acceptance. No history rewrite, merge approval or required-check override.

## Actual Pi evidence and next steps

The [earlier Pi acceptance result](https://github.com/kalharbi11/wirescape/pull/450#issuecomment-6732414300)
is for `51eee4d`, not either reviewed final head. It failed Node LTS and cache
checks. [Keel's ruling](https://github.com/kalharbi11/wirescape/pull/450#issuecomment-6732522458)
says the own-cache failure is a detector false positive; fix code instead of
moving the runner cache. Node remains a correct owner-managed environment
blocker: install/select Node 22 LTS for the actual runner account, matching CI.
Do not weaken accepted majors or change the Pi environment through an agent.

1. Existing builders fix review findings and merge current main normally.
2. Run all local contracts and hosted checks on their new exact heads. Keep
   `ci/pi` and `ci/pi-accept`; inspect current `pi5` and `pi5/accept` reports.
3. Retain missing prerequisites/display as blocked. Only Codex Lead may apply
   Keel's narrowly delegated Node-environment exception to merge decisions;
   it is not a passing preflight or installation certificate.
4. Integrate both features at one source SHA and collect actual ARM64 app
   smoke with isolated profiles before filling the accepted SHA in the
   [owner runbook](../PI_FRESH_INSTALL.md).
5. Owner installation and sound/MIDI/display checks follow the Windows test
   and explicit owner go-ahead. #274 performance, #296 soak and #386 extended
   semantic app tests remain separate.

No current combined Pi install pass, device result, package or release claimed.
Signed: Codex (Pi readiness reviewer and owner-guide builder), 2026-11-08 America/New_York.
