<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design and provenance

Original Wirescape adapter code, GPL-3.0-only, no upstream DSP source.
The allocator and note core in `native/voice/` are MIT and are the voice-policy
oracle. Device acquisition is exclusively in `native/io/midi/`, not in process.
All DSP storage is fixed-size, state restoration resets sounding notes, and no
allocation, locks, logging or OS calls occur in process/event/reset.

Source identity and UMP protocol are preserved; MIDI 1 zero-velocity Note On is
normalized only by MIDI to CV, never globally applied to MIDI 2 Note On. Group
zero is converted to CV; other groups remain available on the raw MIDI port.
State is a little-endian version marker; parameter/configuration state belongs
to the patch document, while held activations and controller acquisition history
are deliberately not resurrected on restore.
