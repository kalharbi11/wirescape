<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Shared-patch offline audio smoke (#557)

T0 test-only change. The public patch library must not contain a patch
that fails production-engine loading or renders all-zero/non-finite
stereo audio. The existing native `wirescape_render` renderer is a
device-free ABI2 `AppEngine` + runtime patch-v2 loader.

**Scope:** CTest invokes `native/tools/render/library_smoke.py` for every
`apps/studio/patches/*.json` on the current checkout, including CHAOS.
Render 2 seconds at 48 kHz / 128 frames with each patch's seed;
script note on/off for `host.keyboard` only, without connecting
hardware MIDI or audio. Assert the existing evidence threshold
`SILENT_PEAK=0.0001` is exceeded on at least one stereo bus, all
samples are finite, and existing render-event counters are zero.
Report per-patch RMS/peak and each failing filename. The full two-channel
balance, timbre and note-release quality still require listening.

**Stops:** Keep any failing patch in place for diagnosis. Removing a
patch, raising a tolerance or modifying the engine needs a separate
issue/approval and explicit evidence. No new DSP or build dependency.

```acceptance
tier: T0
size: <= 250 changed lines, <= 4 files
must_pass:
  - python tools/patch_library.py check apps/studio/patches --shared
  - bash -lc "python tools/dev.py build --headless && ctest --test-dir build/native -R render_evidence_library_smoke --verbose --output-on-failure --no-tests=error"
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [new DSP path, new audio thresholds, relaxed test, module/ID change, devices, CI policy change]
```

Signed: ChatGPT (T0 builder), 2026-11-10
