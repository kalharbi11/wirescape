<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Web macro rate/refusal diagnosis (#646)

Read first: issue #646; native/web/DESIGN.md; macro_oscillator/dsp.h prepare.
Diagnostic portion only. Preserve the default AudioContext and the existing
curated real-WASM matrix assertions. Keel decides any rate/availability policy.

- Bound loader refusal text to 240 printable bytes and include actual engine rate.
- Fresh-process native CTests exercise the same minimal macro patch at 44100
  and 48000 with saved sample_rate=48000; check refusal leaves the old graph.
- Real-WASM browser diagnostic attaches actual hello rate and the unchanged
  glass-cathedrals load reply. Keep the original matrix success assertion.
- No DSP, ID, SDK, protocol field, dependency or rate-policy changes.
Patch guide impact: none — diagnostics only; availability and wiring unchanged.
Keep #646 open until the original default-browser matrix succeeds or Keel
explicitly disposes of it; this packet cannot certify a product fix.

```acceptance
tier: T2
size: <= 180 changed lines, <= 5 files
must_pass:
  - bash -c "cmake -S native/web -B /tmp/ws-web-macro && cmake --build /tmp/ws-web-macro -j2 && ctest --test-dir /tmp/ws-web-macro --output-on-failure"
  - ci: Web demo native rate CTests and full real-WASM suite green
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [rate policy change, forced browser rate, removed matrix assertion, new dependency]
```
