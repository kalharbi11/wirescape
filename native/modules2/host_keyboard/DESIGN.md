<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Design

Original Wirescape code, GPL-3.0-only, no upstream source. The shared note adapter
`native/voice/note_to_cv.h` is MIT and reuses the fixed-capacity voice allocator.
Anonymous repeated-key releases use FIFO. Per-source/channel internal identities
isolate sustain; outbound events retain the original source and assigned voice.
Stealing emits CHOKE before replacement ON at the same sample offset.

All storage is fixed. No allocation, locks, logging or I/O in process/reset.
Gate has a one-sample low retrigger when reusing an assigned voice, matching the
existing allocator/reference adapter; mono legato is available to shared-core
clients but the keyboard's four modes retain allocator defaults.
