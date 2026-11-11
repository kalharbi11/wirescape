<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #178: Studio stereo pair connection atomicity

Production Studio already exports descriptor-supplied stereo_pair metadata and
offers CONNECT BOTH / JUST THIS ONE. A paired gesture must create two ordinary
signal cables, preserve their patch-v2 save/reload identity, and remain one Undo
step. Existing occupied inputs are replaced only when *both* engine connections
succeed. Refusal of either edge leaves both the document and running FakeEngine
connections unchanged, including old cables and history. If a recovery connect
itself fails, show an explicit error rather than silently pretending the old
connection still runs.

This is the Studio/Electron implementation of issue #178, not an ABI, module,
protocol or legacy Qt change. Patch guide impact: none — connection gesture
behavior only; ports and saved representation remain unchanged.

```acceptance
tier: T1
size: <= 300 changed lines, <= 6 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers, web green
stops: [ABI or patch format edit, test weakening, new dependency, device access, unapproved module IDs]
```

Signed: ChatGPT (GPT-6, #178 continuation builder), 2026-11-10
