<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Chord (`essentials.chord`)

Connect an event-note source (`host.keyboard.notes` or a sequencer `notes`) to Chord's `notes` input. Its `notes` output emits the incoming root plus up to four enabled signed semitone shifts, at the **same sample offset and velocity**. Defaults: root, +4 and +7 (major triad). `enable1..4` select additional voices; `shift1..4` each span -24..+24 semitones. Shifted notes outside MIDI 0..127 are omitted; duplicate pitches deliberately remain independent voices.

Chords are events, **not CV**. Route Chord's output into `essentials.notes_to_cv.notes` (#421), then its poly pitch/gate/velocity outputs into a voice/instrument. Raw `midi.to_cv.midi` is not a compatible Chord destination. Changing shifts while a note is held affects future note-ons only. Release, expression fan-out, and source-specific choke follow the stored voice IDs. The module holds at most 16 active input activations and 80 emitted/pending voices; when full, it declines new notes. Chord has no strum, scale, arpeggiator, pattern or clock. Module-to-module note identity uses the source plus a tag derived from graph instance ID modulo 2048 (separate identity namespaces only while instances don't share that tag).

Real MIDI devices and audio outputs are not used by module tests. Studio and WASM availability requires the corresponding catalogue/host build.

Reusing a live explicit `(source_id, channel, note_id)` is a producer error. Last wins: the new on releases the old chord, the next matching off releases the current chord, and another off is ignored. Anonymous repeated keys use LIFO release.

## Design and provenance

Original GPL-3.0-only C++ event transformer, with no third-party DSP or copied legacy chord/sequencer code. The public contract is issue #403, Coda's 2026-11-10 ruling, `docs/tasks/done/CHORD_MODULE_403.md`, and `docs/design/CHORD_GENERATOR_403.md`.

Output note IDs retain the original `source_id` and combine an 11-bit graph-instance tag with a 20-bit non-colliding sequence. Instance IDs differing by multiples of 2048 can alias at downstream fan-in; producers must avoid that configuration until a broader tag policy is approved. Input IDs of zero are valid. Reused explicit identities release their previous activation, but late identical note-offs cannot be distinguished from the replacement's off. Source choke emits only the corresponding owned note-offs, never a broad downstream choke affecting other generators; pending offs retry before new attacks.

Only a state version is persisted: active notes, pending releases and sequence counters are transient and reset silently. The native/document control adapter passes the graph instance ID as four little-endian configuration bytes, and `prepare` refuses missing or invalid instance configuration to prevent output identity collisions.
