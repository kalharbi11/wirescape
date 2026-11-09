<!-- SPDX-License-Identifier: GPL-3.0-only -->
# T293 — Qt-free audio route identity and Settings (#293)

**Tier:** T2. **Branch:** `task/293-2cc`. **Owner:** independent builder.
Read `AGENTS.md`, #293 and its latest Codex Lead handoff, `docs/tasks/P2_REMAINING.md`,
#200 / `docs/tasks/STUDIO_AUDIO_RECOVERY.md`, `native/io/audio/DESIGN.md`,
`docs/specs/ENGINE_PROTOCOL.md`, and `apps/studio/AGENTS.md` before editing.

## Scope and contract

- Keep graph `essentials.output` distinct from the physical audio route.
- Show the selected physical route from **status**, not from the first enumerated
  device. If a remembered ID is missing or ambiguous, show a stale/unavailable
  selection and require an explicit user choice; never silently select a new
  default or open a different device.
- Follow Keel's 2026-11-09 technical ruling on #293 (comment 6735450311):
  device entries can add optional backend, outputChannels, supported and
  unsupportedReason without changing id/name/kind/index. List mono outputs but
  disable them and refuse before open (no mono downmix).
- Preserve existing device-off fake/null testing and session/generation rules.
  Advertise only implemented optional sample-rate and buffer capabilities.
  Validate rates {44100,48000,96000}, buffers {64,128,256,512,1024};
  stop the stream, reprepare the current graph, restart, roll back on refusal,
  retain the document/MIDI selection and persist only after success.
  The control-thread reply precedes the next new-state status push.
- Do not change ABI, IDs, wire format, dependencies, thresholds or protected
  spec paths without explicit authorization. Real device access is owner-only.

## Focused checks

```acceptance
tier: T2
size: <= 650 changed lines, <= 10 files
must_pass:
  - ctest --test-dir build/native -R "audio|sidecar_" --output-on-failure --no-tests=error
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser="$(command -v chromium || command -v chromium-browser || command -v google-chrome)" && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npx playwright test tests/system.spec.ts --grep "audio route"'
  - pi-run: ctest --test-dir build/native -R "audio|sidecar_" --output-on-failure --no-tests=error
stops: [ABI header edit, ID/format change, new dependency, real devices in tests, threshold loosening]
```

Before completion: verify fake mono/stale/duplicate identities, explicit
selection and refusal, supported-capability UI, stream-only restart with
unchanged graph/MIDI, and exact-head private Pi fake-route lifecycle evidence.
Require independent Claude T2 COMMENT review and all required hosted checks.
