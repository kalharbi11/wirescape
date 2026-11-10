<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Chord design and provenance

Original GPL-3.0-only C++ event transformer; no third-party DSP or copied legacy chord/sequencer code. Public contract: #403, Coda's 2026-11-10 ruling, `docs/tasks/CHORD_MODULE_403.md`, and `docs/design/CHORD_GENERATOR_403.md`.

All voice storage and pending releases are fixed-size. Output voice IDs retain `source_id` and carry a module-instance tag in the high 11 bits and a non-colliding 20-bit sequence. Graph instances with IDs differing by multiples of 2048 cannot share a source safely at downstream fan-in; keep this documented until a broader tag policy is approved. Explicit input IDs are matched per source/channel/id (including 0); anonymous repeated keys are matched LIFO. A reused explicit ID releases the prior activation before replacement; the wire fields cannot distinguish a late identical off from the replaced activation, so producers must not reuse the same key/ID while a late off is outstanding. Source choke is expanded into owned note-offs, never a broad downstream choke that would release other generators. Pending off events are retried before new attacks.

State persistence saves a version only. Active notes, pending releases and counters are runtime ownership; reset clears them without emitting. The native/document control adapter supplies the graph instance ID as four little-endian bytes; without that, `prepare` refuses to prevent identity aliasing.
