<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #386: real Electron sidecar evidence and device-loss state sequence

Keel's 2026-11-09 ruling in #386 authorizes this protected task path.
Keep the existing `apps/desktop/tests/app-e2e.spec.cjs` Linux/Xvfb harness:
real Electron, preload, production sidecar with `--null-audio` and fake MIDI.
Do not add a test-only sidecar protocol or device flag.

## Read first
- `AGENTS.md`, `docs/WORKFLOW.md`, #386 and PR #442 latest handoffs
- `apps/desktop/tests/app-e2e.spec.cjs`
- `apps/studio/tests/mock-bridge/{bridge.ts,real-engine.spec.ts}`
- `native/sidecar/audio_recovery_tests.cpp`

## Evidence contract
1. Real Electron: Save → quit → reopen preserves unknown fields and document
   bytes; a genuinely refused unknown-port edit leaves graph and generation
   unchanged, then a valid edit succeeds; device-free native ADSR and effective
   parameter telemetry have bounded, meaningful values.
2. Recovery is composed, not faked inside the sidecar. Native
   `abi2_sidecar_audio_recovery_tests` covers device-lost → reconnecting →
   running with a link-time fake audio service, note release and device push.
   Studio mock-bridge Playwright drives the existing `status.audio.state`
   running → device-lost → reconnecting → running plus a `devices` push through
   `RealEngine`. Assert truthful status labels, no `app.restartEngine`, no
   lost editable patch/unknown fields and resumed meter telemetry.
3. Report Linux/Xvfb as functional-only; Windows Electron e2e is **skipped /
   unavailable**, never passed. Screenshots and traces on failure only.
   No real audio/MIDI, LAN, SSH, owner-device or display-performance claims.

Keep PR draft until exact-head evidence and independent T1 COMMENT review.
No changes to protocol, ABI, production engine or CI workflows.

```acceptance
tier: T1
size: <= 400 changed lines, <= 5 files
must_pass:
  - ctest --test-dir build/native -R "^abi2_sidecar_audio_recovery_tests$" --output-on-failure --no-tests=error
  - ci: Web Linux/Xvfb runs apps/desktop npm test and Studio mock-bridge Playwright unskipped
  - ci: guard, linux-arm64, windows-x64, sanitizers on final head
stops: [device access, LAN, new test-only protocol, test weakening, skipped platform called passed]
```

Signed: ChatGPT (GPT-6, real-app evidence builder), 2026-11-09
