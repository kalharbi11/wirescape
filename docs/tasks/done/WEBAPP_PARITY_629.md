<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #629 — Independent non-OSC real-WASM module matrix

Scope: only the remaining non-OSC patch matrix from issue #629, distinct from
the already-owned CHAOS/reverb/output regression PR #632 and Groove browser PR
#626. This task is a tests-only subset, not full #629 acceptance.

- Exercise real AudioWorklet/WASM loadGraph acknowledgement, audible output,
  and Studio document save/reload for representative committed patch fixtures:
  VCO + ADSR + Delay + Reverb, random + Quantizer + Resonator, Step Sequencer
  + filter + ADSR, macro oscillator, and CHAOS.
- Verify the merged Chord's event route separately. If Notes→CV is not in
  the live Web catalogue, record that dependency as not implemented; do not
  claim an audible Chord→CV path. When it becomes available, exercise it.
- Do not modify Groove UI, any host capability gate, or another agent's PR.
- No production behavior changes, device access, LAN, CI policy or thresholds.

```acceptance
tier: T1
size: <= 400 changed lines, <= 6 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: web-demo engine/site green and guard, linux-arm64, windows-x64, sanitizers green
stops: [SDK/ABI or protocol change, any device/socket access, CI policy or threshold changes, unsupported-host behavior presented as tested]
```

Signed: ChatGPT (GPT-6, non-OSC builder), 2026-11-11
