<!-- SPDX-License-Identifier: MIT -->
# Example Step Sequencer (`example.step_sequencer`)

A worked SDK example: an eight-step sequencer that drives pitch, gate and velocity CV and also
emits note events. It shows edge detection, a small state machine, event output with bounded
capacity, large parameter sets and versioned state. Library `ws2_example_step_sequencer`, test
`abi2_example_step_sequencer_tests`. No audio device is opened.

Each rising `clock` edge advances one step (wrapping at `length`). A note step starts a note,
a rest step silences, a tie step lets the previous note keep sounding. Falling clock edges end the
note unless the next step is a tie. A rising `reset` edge makes the next clock play step 1.

| Kind | ID | Hint / range | Notes |
|---|---|---|---|
| Input | `clock` | `WS2_TRIGGER`, first | Rising edge advances; falling edge ends the gate. Schmitt 0.5 / 0.25. |
| Input | `reset` | `WS2_TRIGGER`, first | Rising edge arms step 1 and releases the note. If clock rises on the same sample, reset wins: that clock plays step 1. |
| Output | `pitch` | `WS2_PITCH` | 1 V/oct, 0 = C4 (MIDI 60). Quantised to semitones so it equals the note key. Holds after the note ends. |
| Output | `gate` | `WS2_GATE` | 1 while a note sounds. |
| Output | `velocity` | `WS2_CV_UNI` | Velocity of the last note, holds after it ends. |
| Output | `notes` | event, `WS2_EV_NOTES` | `NOTE_ON` (value = velocity) and `NOTE_OFF` on channel 0, `note_id` counting up from 0. Offsets are sample indices in the block. |
| Param | `run` | 0..1, default 1, stepped | 0 releases the note and stops advancing; the position is kept. |
| Param | `length` | 1..8, default 8, stepped | Number of steps played. |
| Param | `pitch1`..`pitch8` | -2..2 oct, default 0 | Key = 60 + round(pitch * 12). |
| Param | `gate1`..`gate8` | 0..2, default 1, stepped | 0 rest, 1 note, 2 tie. |
| Param | `vel1`..`vel8` | 0..1, default 0.8 | Note-on velocity. |

26 parameters in all (host limit 32). IDs are stable API and must never change. The pattern lives
in the parameters; the module state holds only the playback position.

`reset()` cannot emit events: the host chokes notes when it resets a module. Run it with
`ctest --test-dir build -R example_step_sequencer`.
