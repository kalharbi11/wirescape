<!-- SPDX-License-Identifier: GPL-3.0-only -->
# P2C-a (#291): bounded audio-device loss recovery

The Qt-free sidecar reports `device-lost`, then `reconnecting`, after a lost output, keeps the patch,
MIDI choices and remembered output identity, and retries on the control thread
with bounded backoff. It never substitutes a default or another output. All
regressions use fake backends; physical devices and direct Pi access are forbidden.

Read first: `native/sidecar/DESIGN.md`, `native/io/audio/DESIGN.md`,
`native/io/audio/portaudio_service.*`, and ENGINE_PROTOCOL audio status.
Own audio recovery functions only in the shared session implementation; MIDI
hot-plug is #288. Preserve callback ownership and quarantine on unsafe shutdown.

```acceptance
tier: T2
size: <= 500 changed lines, <= 8 files
must_pass:
  - ctest --test-dir build/native -R "audio|sidecar_" --output-on-failure --no-tests=error
  - pi: fake-device audio-loss/recovery regressions on pi5 at exact head
stops: [ABI header edit, ID/format change, new dependency, real devices in tests, threshold loosening]
```
