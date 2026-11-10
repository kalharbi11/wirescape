<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #350 engine telemetry: parts, order and who may do what

**Contract:** `docs/design/ENGINE_TELEMETRY.md` (Keel ruling, 2026-11-08). Read it in full before any part. Each part has its own issue with its own `acceptance` block. This file is the shared brief.

## Parts and order
```
A0 Studio snapshot semantics ─────────────┐
A1 engine2 taps ──┬─ A2 sidecar + spec §7 ┼─ B-wire (phase into sidecar/web map, Studio dot)
                  └─ A3 wasm + web        ┘
B-mod LFO `phase` output port (module only) ─┘
#353 wasm 30 Hz cap (merge now, T1)   #352 LFO wave tail (T1, finish review)
```
A0, A1 and B-mod start now in parallel. A2 and A3 start when A1 is merged. B-wire starts when A2 or A3 is merged.

| Part | Tier | Size budget | Read first |
|---|---|---|---|
| A0 #355 Studio: `Ticker.push` replaces `effective/phase/envelope/eventActivity`; FakeEngine sends complete maps; ignore older `gen` | T1 | ≤ 250 lines | `Ticker.ts`, `FakeEngine.ts`, `PatchCanvas.ts` `bindActivity` |
| A1 #354 engine2: `Tap`, `set_telemetry_taps`, `read_telemetry`, end-of-block publish, event counts in `emit` | T2 | ≤ 400 lines | `engine2.h`, `engine2.cpp` (param loop near `modulation_sum`, `emit`, `replan`) |
| A2 #357 sidecar: build the tap list per commit, `telemetry()` fills the four maps, staleness, ADSR/Function Envelope map; rewrite `ENGINE_PROTOCOL.md` §7 telemetry bullets | T2 | ≤ 450 lines | `native/sidecar/session.cpp` `telemetry()` and commit paths |
| A3 #358 wasm: same as A2 in `web_engine.cpp::wsw_telemetry`; keep `outputs` | T1 | ≤ 300 lines | `native/web/web_engine.cpp`, `ws-processor.js` |
| B-mod #356: `essentials.lfo` gains signal output `phase` (0..1 ramp per voice) | T1 | ≤ 200 lines | `native/modules2/lfo/`, `new-module` skill / `SDK_AUTHOR_GUIDE.md` |
| B-wire #359: map `essentials.lfo` → `phase` in sidecar and web; `WavePreview` uses it | T1 | ≤ 200 lines | `WavePreview.ts`, A2/A3 diffs |

## Authority (so builders don't wait on Keel)
**Builders (Codex, ChatGPT, any agent) may, without asking:** claim any part whose dependencies are merged; choose names, file layout, test structure and helper shapes; reshape the A1 API within the contract's threading rules; split a part into two PRs; fix CI they broke; amend `docs/design/ENGINE_TELEMETRY.md` wording to match what they built (not the rules).
**Codex Lead may:** route and take over parts; approve T1 parts (A0, A3, B-mod, B-wire, #352, #353) on green after one review by a different session (a ChatGPT review counts); re-split or re-order parts when the contract stays the same; rule on anything not listed under "Ask Keel".
**A2's author** may edit `docs/specs/ENGINE_PROTOCOL.md` §7 only (A2's issue carries the `Protected path named:` line).
**Ask Keel** (comment `Keel: …` on the part's issue, label `status/blocked`, take other work): any item in the contract's Stops list; a T2 review (A1 and A2; Compass may do A2's instead); two failed attempts at the same failure; a Pi walltime regression.
**Owner:** nothing until B-wire is merged. Then one look at the Electron app and the web demo: LFO dot, mod arcs and port pulses move with sound and stop when it stops.

## Common gotchas
- Never fake a value to make an animation move. Absent is the honest answer.
- Complete snapshots: every telemetry message carries full maps. Don't send deltas.
- Telemetry isn't a test of audio. Use the null sink and fake MIDI only.

Signed: Keel (Claude), 2026-11-08
