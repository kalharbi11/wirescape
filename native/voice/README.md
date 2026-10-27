# Portable voice allocator

`allocator.h` and `allocator.cpp` are C++17, have no engine/SDK dependency, and use no dynamic memory, locks, I/O or exceptions. One owner calls the allocator at a time. Construct or `configure()` it before rendering; all subsequent operations are bounded. `tests.cpp` is a standalone assertion program (not linked into the library).

## Contract

`Config::max_voices` is 1–16. `Mono` always exposes one voice. The other modes expose exactly `max_voices` snapshots, including released voices so an envelope can render its tail. `active_count()` counts raised gates; `release_count()` counts assigned voices with lowered gates. On reuse, the old release tail ends. Call `clear_triggers()` after the downstream envelope has consumed `trigger`/`retrigger`; a new note-on sets them. A mono legato transition while a gate remains high changes pitch without setting either trigger. A non-legato transition sets both if the voice was previously assigned. The trigger flags are event information, not a timed CV pulse; the host generates any sample-rate-specific pulse.

- **Rotate:** start searching for a gate-low voice at the cursor following the last assignment, wrapping at the configured voice count. If all gates are high, steal the oldest start order; equal order uses the lowest index. The next cursor follows the chosen voice.
- **Reset:** choose the lowest-index gate-low voice. If all are high, steal the oldest active voice. The mode does not preserve a round-robin cursor.
- **Reuse:** first reuse a gate-low voice last assigned to the same source, channel and key. Otherwise use Reset's lowest-free/oldest-steal rule. A different source does not get affinity from another source's note.
- **Mono:** choose among held notes by last-started, lowest key or highest key. Equal-key ties choose the newest note. Releasing the selected note falls back to the next eligible held note. Releasing a non-selected note leaves the current voice alone. Legato changes pitch without retrigger while a gate is already high; non-legato retriggers. Mono sustain-held notes participate in priority selection.

All modes steal **oldest** (not quietest). A stolen note is removed from the held registry, so a later note-off for it returns `NotFound` and cannot release its replacement. A gate-low voice can be reused even if an external envelope is still releasing.

`note_id >= 0` matches by `(source_id, note_id)`; a repeated explicit ID replaces the previous note for that source. With `note_id == -1`, note-off matches the newest `(source_id, channel, key)` anonymous note. `source_id` is never ignored. Key is MIDI 0–127, channel 0–15, velocity 0–1. Pitch output is `(key − 60)/12` in 1 V/oct units. Pressure/timbre default to zero and are updated by `expression()` with values 0–1. Note-on velocity defaults to 1. Sustain is per source: a note-off while the pedal is down clears physical-down state but keeps gate high; pedal-up releases all pedal-held notes for that source. `release_all_from_source()` clears the source's notes and sustain state while preserving other sources.

Capacity is fixed at 128 held notes and 32 simultaneous sustain-down sources. `note_on()` returns `NoteCapacity` without changing state when full; `sustain(source, true)` returns `SourceCapacity` without changing state when full. Repeated note-on with the same explicit ID can replace a slot even at full capacity. `configure()` validates before changing state, then resets. `reset()` clears voices, held notes, sustain, cursors and ordering for reproducibility.

The interface only covers note identity and per-note pressure/timbre. **MPE is a documented stub:** a later adapter must define member-channel ownership, pitch bend, zone handling and teardown before claiming MPE support. No automatic or hidden MPE channel assignment is done here.

## Integration

Compile `allocator.cpp` into the host and include `allocator.h`. Call `note_on`, `note_off`, `sustain`, and source release from the host's ordered event stream. Sample `voice(i)` to fill per-channel pitch, gate, velocity, pressure and timbre signals; emit `trigger`/`retrigger` to poly ADSR on the same event frame. This layer intentionally does not depend on `module2.h` or choose a host event representation.
