<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #403: Chord module (build task)

Issue #403. Product surface confirmed by Codex Lead (#406 comment 6722798879);
the technical identity contract is ruled by Coda on #403 under the owner's
2026-11-10 instruction. No ABI, SDK header, spec or protocol change. The full
design note is `docs/design/CHORD_GENERATOR_403.md` on branch
`task/403-chord-contract` (closed draft #406): read it, but where it lists a
"Keel stop" use the rulings below. Don't read or copy `legacy/`.

## Module
`essentials.chord` ("Chord") in `native/modules2/chord/`.
- Ports: input `notes`, output `notes` (`WS2_EVENT`, `WS2_EV_NOTES`, `WS2_POLY`).
- Params, stable order: `enable1, shift1, enable2, shift2, enable3, shift3,
  enable4, shift4`. Enable 0/1, shift integer −24..+24 semitones. Defaults
  (1,+4), (1,+7), (0,+12), (0,−12) → a major triad. Root always sounds.
- A NOTE_ON at offset t emits the root plus each enabled shift at the same t
  with the input velocity and channel. Out-of-range keys (outside 0..127) are
  omitted. Duplicate shifts make separate voices (no dedupe).
- Param changes affect later NOTE_ONs only; each activation stores the
  voices it actually emitted and releases exactly those.

## Rulings (replace the design's "Keel stop" items)
1. **Output identity:** keep the input `source_id`. Output `note_id` is
   `(instance_tag << 20) | seq`, where `instance_tag` = graph instance ID
   mod 2048 (11 bits) and `seq` a 20-bit counter that skips any ID still
   active or pending. Get the instance ID through the optional
   `runtime_module_config(type, value, instance_id)` adapter argument, as
   Groove G1 (#496/#505) does; no SDK field. Two Chords fed by the same
   source then never collide downstream (for graph IDs < 2048, documented).
2. **Input matching:** explicit `note_id >= 0` (0 is valid) by
   (source_id, channel, note_id); a NOTE_ON that reuses an active explicit
   identity first releases the old activation (**last wins**, Keel 2026-11-10 on
   #511): an OFF with that tuple then releases the *current* activation.
   A stale OFF can't be told apart without a generation field, which would be an
   event-format change, and reusing a live explicit ID is a producer error. Document
   this in the patch guide. Anonymous IDs by (source_id, channel, key), LIFO.
   Unmatched offs are ignored (no wildcard release).
3. **Expression:** a per-note expression (note_id >= 0 matching an activation)
   is re-emitted for every voice of that activation with its output IDs.
   Channel-wide expressions (note_id < 0) pass through unchanged.
4. **Choke:** `source_id == UINT32_MAX` releases all; otherwise only that
   source's activations. Reset clears state and never emits.
5. **Capacity:** 16 activations × up to 5 voices; 80 pending-release slots.
   Releases flush before attacks and at the start of each block; a refused
   off stays pending. When activations or pending slots are full, a new
   NOTE_ON is declined. A partly emitted chord records only the accepted voices.

## Playable chain
`host.keyboard.notes` or Groove `t1_notes` → Chord → **`essentials.notes_to_cv`**
(#421, `docs/tasks/NOTES_TO_CV_MODULE_421.md`) → VCO/ADSR/VCA → Output.
Until #421 merges, the native tests use `native/tests2/test_modules.h::NotesToCv`.
`midi.to_cv` takes raw MIDI and must not be wired from Chord.

## Tests
Native `abi2_chord_tests` (callback sink): root 60 → 60/64/67 at identical offset
and velocity with distinct IDs; minor/custom/negative shifts and out-of-range
omission (1–5 voices); input ID 0; two sources; channels 0/15; repeated root;
explicit-ID reuse (last wins: the second OFF finds nothing); anonymous LIFO; held param edit releases the old pitches;
expression fan-out; choke targeted/wildcard; capacity refusal of on/off across
blocks and recovery; ID wrap skips live IDs; two Chord instances into one sink
don't collide; 1/17/128/max frames; zero allocations. Engine test: keyboard
→ Chord → (test NotesToCv or #421) renders a 3-note chord. Studio: catalogue
entry and save/reload. Patch guide updated in the same PR.

Size budget per Keel (2026-11-10) includes the review's regressions.

```acceptance
tier: T1
size: <= 1100 changed lines, <= 20 files
must_pass:
  - bash -lc "python tools/dev.py build --headless --config Release && ctest --test-dir build/native -R 'chord|ui_patch_conformance|sidecar' --output-on-failure --no-tests=error"
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - pi-run: ctest --test-dir build/native -R chord --output-on-failure --no-tests=error
  - ci: guard, linux-arm64, windows-x64, sanitizers and web green on the exact head
stops: [ABI/SDK header change, new public ID other than essentials.chord, strum/arp/scale features, engine2 routing change]
```

Signed: Coda (Claude), 2026-11-10
