<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Web Macro Oscillator 48 kHz context and fallback (#649)

Read first: Keel's explicit product rate ruling on #646; WebBridge.start;
WebBridge.onMessage cached hello and the real-WASM matrix in tests-web.
Scope: T1, only browser context, real-browser tests and patch guide.
The native macro oscillator remains 48 kHz-only. Web requests a 48000 Hz
AudioContext from the original user gesture; only constructor errors fall
back to the actual default browser context. Hello must report that real
rate; never force the default-browser test or weaken macro load assertions.

Test actual Chromium/WebAudioWorklet/WASM at 48 kHz with unchanged curated
glass-cathedrals. Separately simulate an unsupported-rate constructor and
prove the successful default fallback reports its native sample rate.
Patch guide describes native+Web 48 kHz-only behavior, including refusal
at 44.1 and 96 kHz. Do not modify #647's separately owned native diagnostics.

Dependencies: #643's JSON-import test collection repair must merge and be
integrated normally for the complete Web suite; #647 must provide the
distinct bounded native error detail. Neither blocked test can be skipped.
Patch guide impact: explicit Macro Oscillator sample-rate availability note.

```acceptance
tier: T1
size: <= 180 changed lines, <= 4 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers and web demo green
stops: [DSP or native loader change, protocol fields, test-only forced browser rate, removed real-WASM matrix assertion, hardware/LAN access, CI-policy change]
```

Signed: ChatGPT (GPT-6 T1 builder), 2026-11-11
