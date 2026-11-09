<!-- SPDX-License-Identifier: MIT -->
# Step Sequencer design notes

Provenance: original code written for Wirescape, MIT.

- **Order of work per sample:** reset edge, clock edge, run switch, then reset action, clock action. A reset rising edge sets `armed` (next clock plays step 1) and releases the note. A clock on the same sample then plays step 1, so reset wins and nothing is skipped.
- **Falling edge and ties.** At a falling clock edge the module looks at the next step (step 1 if armed). If it is a tie the note keeps sounding, otherwise it is released. A tie with nothing sounding behaves like a rest. If `length`, `gate` or `reset` change between the fall and the next rise, the note is simply released at the next rise or reset instead.
- **CV and events agree by construction.** `gate` is `sounding.active`; `pitch` is the quantised key of the last note-on. Only `start_note` and `release` change that state, and they are the only places events are emitted. When a retriggered note follows a release on the same sample, the events are NOTE_OFF then NOTE_ON at the same offset while gate stays high (no gap sample).
- **Never strand a note-off.** `b.emit` returns false when the bounded queue is full. The released note moves to a single `pending` slot and `flush` retries it at the start of the next block and before any new note. While it is stuck no new note starts (skipped, never overlapping). There is no retry loop, so the work per block stays bounded. The note-off may therefore arrive late: gate is already low while the event is outstanding.
- **reset() and state.** `reset()` cannot emit; the host chokes notes on reset, so it only clears state. `load_state` restores the position and edge flags, and if a note was sounding it becomes `pending` so the next `process` releases it. State v1 holds: step, armed, running, clock/reset levels, the sounding or pending note, next note id, last key and velocity (needed so held CV outputs continue identically). Validation uses a plain staged commit: all fields are read and checked before any member changes.
- **Parameters.** The 24 step parameters are generated with a macro so IDs stay readable (`pitch1`, `gate1`, `vel1`, ...). Values are read at the sample where they are used, so they can be automated safely.
- **Outputs are never CONSTANT:** the module always fills every sample; the cost is trivial and the code stays simple.
- No allocation, locks, I/O or exceptions in `process`/`reset`.
