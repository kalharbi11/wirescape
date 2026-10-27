# Unattended audio capture and analysis harness (Track A)

**Status:** required by the user, October 26, 2026. It's independent of ABI 2. Scheduled as M1-2 in `docs/PLAN.md`.

## Why
The user is often away. Future real-device audio tests must record and analyse the output automatically, without asking anyone to listen. The existing `wirescape_audio_session` probe only runs a device stream and records counters. It is **not** a recorder or analyser, and must never be described as loopback validation.

## Requirements
- **One reusable harness, invoked explicitly** (`--confirm-audio`). It's kept separate from `check`, CTest and CI. It never starts automatically, never runs endlessly, never force-closes apps with unsaved work, and makes no fake "perceptual" certification.
- **Capture:**
  - Record the tested Windows output endpoint or session through an appropriate loopback path (research and choose WASAPI loopback, or process loopback where available). Prefer isolating Wirescape's own output. No microphone by default.
  - Detect and report unrelated mixed-in audio or an ambiguous capture, rather than falsely passing.
  - If a backend or endpoint can't be captured, report "unavailable" as its own result.
- **Reference:**
  - Render deterministic reference WAVs and vectors offline from the same patch, parameters, sample rate and scheduled events.
  - Save the captured WAV plus endpoint, config and provenance, and JSON/Markdown findings, under `build_reports/` (git-ignored). Never save into user presets.
- **Alignment:**
  - Handle startup latency, correlation alignment, channel mapping and possible sample-rate conversion explicitly.
  - Never demand byte equality between a resampled or system-processed endpoint recording and raw engine samples.
- **Analysis:**
  - non-silence, RMS, peak, clipping, DC
  - dropouts and discontinuities
  - clock and envelope timing
  - spectral and response behaviour
  - similarity to the reference, with justified tolerances

  Results must separate four kinds of failure: application faults, transport/device counters, capture failure and signal mismatch.
- **Stimuli:**
  - known tones, pulses and sweeps, plus module-specific test patches, alongside the combined Noise demo (random noise alone is weak evidence for filters or timing)
  - **negative self-tests** proving that silence, clipping and mismatch are detected
- **Bounds:** limit duration, storage and CPU. Use low output levels. Stop cleanly on failure.

## Open research (record the answers in the PR)
- The minimal-dependency Windows capture API, and per-process vs endpoint capture availability.
- Safe ownership of start and stop.
- An honest statement of what is measured: the endpoint stream, not physical speakers.

## Acceptance
- The self-tests (silence, clip, mismatch) pass offline using synthetic captures.
- One explicit, user-authorized run produces a report.
- Docs say exactly what was and wasn't verified.
