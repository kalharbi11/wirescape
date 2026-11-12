# M1-2: Audio capture and analysis harness

Spec: `docs/specs/AUDIO_CAPTURE_HARNESS.md` (authoritative). Branch `task/<issue#>-capture-harness`, draft PR. Follow AGENTS.md.

- Independent of ABI 2. Don't modify the engine or the ABI headers. Put the harness under `tools/capture/` and, if needed, `native/tools/capture/`.
- Stage 1, required for review: research the API choice and record it in the PR. Build the offline analysis library and the self-tests (synthetic silence, clipping, mismatch and dropout captures) into CTest.
- Stage 2: the explicit `--confirm-audio` capture runner. **Don't run it on a real device.** The user will run it once and attach the report.
- The CI Windows job runs only the offline self-tests.
- Stop condition: a draft PR, CI green, a handoff comment, then `status/review`.
