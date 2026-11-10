<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Chord (`essentials.chord`)

Connect an event-note source (`host.keyboard.notes` or a sequencer `notes`) to Chord's `notes` input. Its `notes` output emits the incoming root plus up to four enabled signed semitone shifts, at the **same sample offset and velocity**. Defaults: root, +4 and +7 (major triad). `enable1..4` select additional voices; `shift1..4` each span -24..+24 semitones. Shifted notes outside MIDI 0..127 are omitted; duplicate pitches deliberately remain independent voices.

Chords are events, **not CV**. Route Chord's output into `essentials.notes_to_cv.notes` (#421), then its poly pitch/gate/velocity outputs into a voice/instrument. Raw `midi.to_cv.midi` is not a compatible Chord destination. Changing shifts while a note is held affects future note-ons only. Release, expression fan-out, and source-specific choke follow the stored voice IDs. The module holds at most 16 active input activations and 80 emitted/pending voices; when full, it declines new notes. Chord has no strum, scale, arpeggiator, pattern or clock. Module-to-module note identity uses the source plus a tag derived from graph instance ID modulo 2048 (separate identity namespaces only while instances don't share that tag).

Real MIDI devices and audio outputs are not used by module tests. Studio and WASM availability requires the corresponding catalogue/host build.
