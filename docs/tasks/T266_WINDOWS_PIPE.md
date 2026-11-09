<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #266: Windows sidecar stdio pipe stall — T2 protocol repair

Keel's ruling in issue #266 authorizes removing the production iostream transport and using native OS pipes instead. This task file is authorized by Codex Lead's `Protected path named: \`docs/tasks/T266_WINDOWS_PIPE.md\`` comment on #266 (2026-11-08). Implementation is draft PR #439, branch `task/266-windows-raw-pipe`.

## Read first
`AGENTS.md`; issue #266's Keel ruling and latest handoff; `docs/PLAN.md` current decisions; `docs/specs/ENGINE_PROTOCOL.md`; `native/sidecar/protocol.h`, `protocol.cpp`, `main.cpp`, `protocol_unit_tests.cpp`, `protocol_tests.py`, `golden_tests.py`; and `native/CMakeLists.txt` sidecar CTest registration. Do not read legacy or third-party code.

## Scope
- Replace production stdin/stdout `std::cin`/`std::cout` transport with Windows `GetStdHandle` + `ReadFile`/`WriteFile`, and POSIX `read`/`write` with `EINTR` handling. Keep an isolated stream adapter for unit tests.
- Read bounded chunks and preserve NDJSON framing, UTF-8/JSON validation, `LineLimit`, busy/duplicate/error/EOF behavior, reply ordering and the 5-second write watchdog. Keep all logs on stderr.
- Never open real audio/MIDI devices or LAN sockets in tests. `--null-audio` / fake MIDI remain the test path. Do not change ABI, SDK, realtime callbacks, watchdog tolerances or protocol data.
- **Remove the Windows 77 early-return/CTest skip** for `abi2_sidecar_protocol_tests` and `sidecar_golden`. A skipped test is not passing evidence; both must actually execute and pass on Windows.
- Add bounded regressions covering chunks split across 64 KiB boundaries, multiple messages and over-limit refusal. Do not weaken existing golden/protocol tests.

## Acceptance and reporting
- Run targeted native and full hosted device-off checks. The actual `windows-x64` CI job must be **executed** on the exact head, not absent because the PR is draft; maintain the `ci/windows` label for this PR. Report counts and any skips.
- An independent **Claude/Keel T2 COMMENT review** is required on the tested head because the author is from the GPT/Codex model family. The author must not self-approve or merge.
- If native `WriteFile` still hangs, record which call and Windows thread stacks with Keel's requested diagnostics, then stop for another ruling. Do not increase the timeout or hide test failures.
- Pi device-free evidence is relevant to the sidecar, with no real hardware probes in hosted CI; request the private Pi runner per `docs/PI_RUNNER.md` if directed by Keel. Real Windows playback remains an owner check and belongs to #436.

```acceptance
tier: T2
size: <= 400 changed lines, <= 8 files
must_pass:
  - ctest --test-dir build/native --output-on-failure --no-tests=error -R '^(abi2_sidecar_unit_tests|abi2_sidecar_protocol_tests|sidecar_golden)$'
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
stops: [new dependency, changed watchdog, changed protocol/ABI, device access in tests, test skip or suppression]
```

Signed: ChatGPT (GPT-6, Windows protocol acceptance continuation), 2026-11-08
