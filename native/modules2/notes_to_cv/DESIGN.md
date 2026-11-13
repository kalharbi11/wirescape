<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Notes→CV design and provenance

Original GPL-3.0-only module wrapper around the repository's existing MIT `native/voice/NoteToCv` and `Allocator`. No third-party DSP, new dependency, SDK/ABI header or engine event-routing change. Approved surface and semantics: #421 / Coda's 2026-11-10 ruling, `docs/tasks/done/NOTES_TO_CV_MODULE_421.md`.

An input NOTE_ON enters the shared allocator with its original (source_id, channel, note_id/key) identity. The allocator returns independently assigned poly lanes, so several notes from one channel sound simultaneously. The signal generator uses a bounded 16-voice snapshot per sample with pitch from the exact allocator MIDI-note-to-V/oct convention, gate and velocity. A stolen voice emits a one-sample gate dip before the replacement to retrigger an ADSR; this is the established host.keyboard gate convention. No note-event forwarding output exists. Changes to the voices control reconfigure/release the held allocator state, and reset/load never resurrects notes.
