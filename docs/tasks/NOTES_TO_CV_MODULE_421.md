<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #421: Notes-to-CV production allocator (build task)

Issue #421. Builds the module that makes chords audible. Pairs with
`docs/tasks/CHORD_MODULE_403.md`; the two can be built in parallel (they only
share the catalogue registration lines). Contract ruled by Coda on #421 under
the owner's 2026-11-10 instruction; no ABI, SDK header, spec or protocol change.

## What to build
A shipped module **`essentials.notes_to_cv`** ("Notes→CV") in
`native/modules2/notes_to_cv/` that turns `WS2_NOTE_ON/OFF/CHOKE/EXPRESSION`
events into poly CV, exactly like `midi.to_cv` does for raw MIDI.
- Input: `notes` (`WS2_EVENT`, `WS2_EV_NOTES`, `WS2_POLY`).
- Outputs, in this order: `pitch` (V/oct, 0 V = C4), `gate`, `velocity`, all
  `WS2_POLY` with one channel per voice. Mirror `midi.to_cv`'s output hints,
  units and channel-count behaviour; read `native/modules2/midi_to_cv/` first.
- Params: `voices` 1–16 (default 4, stepped), `mode` poly-steal policy only if
  `midi.to_cv` already exposes one (same ID and values); nothing else.
- **Fresh allocation:** voices are allocated by the module, ignoring incoming
  `channel` as a voice index. Identity comes from `native/voice/NoteToCv`:
  explicit `note_id >= 0` keyed by (source_id, channel, note_id); anonymous
  (negative) IDs keyed by (source_id, channel, key), released FIFO (oldest first, Coda ruling on #512). Reuse
  `NoteToCv`/`voice::Allocator`; don't write a second allocator. The
  test-only `native/tests2/test_modules.h::NotesToCv` is a reference only.
- Steal, reset, choke (`source_id == UINT32_MAX` = all) and expression
  behaviour = `NoteToCv`'s existing semantics. Reset never emits events.
- Realtime: fixed storage sized for 16 voices; no allocation/locks/I/O.

## Wiring it in
`native/control/module_catalog.inc` (one `WS_APP_MODULE` line), the modules2
CMake, the generated Studio/WASM catalogue (`apps/studio/public/wirescape_modules.json`
via the repo's generator; never hand-edit) and FakeEngine support if modules
need it. The WASM host allows it (it isn't `midi.*`).

## Tests
- Native `abi2_notes_to_cv_tests`: triad 60/64/67 and seventh 60/64/67/71 on
  one source → 3/4 distinct voices, correct pitch (V/oct), gate and velocity;
  independent offs; repeated root; two sources with the same note_id don't
  cross-release; anonymous IDs; steal at `voices=4` with a 5th note; choke
  targeted and wildcard; 1/17/128/max frames; zero allocations in process.
- Engine graph test: `host.keyboard.notes` → Notes→CV → VCO/ADSR/VCA → Output
  renders finite non-zero audio for a held triad (null sink).
- Studio: catalogue lists the module; save/reload round-trip.
- Patch guide (`docs/PATCH_DESIGNER_GUIDE.md`): replace the "Chord or
  Notes→CV don't exist" limitation for Notes→CV with the real ID, ports and a
  wiring example, in this PR.

```acceptance
tier: T1
size: <= 900 changed lines, <= 18 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'notes_to_cv|ui_patch_conformance|sidecar' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - pi-run: ctest --test-dir build/native -R notes_to_cv --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [ABI/SDK header change, changing midi.to_cv IDs or behaviour, new dependency, engine2 routing change]
```

Signed: Coda (Claude), 2026-11-10
