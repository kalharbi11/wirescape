# Clock — Wirescape Essentials

Module ID: `essentials.clock` | Library: `ws_clock` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | BPM | 20–300 BPM | 120 |
| 1 | Ticks per beat | 1–24  | 1 |
| 2 | Duty | 0.01–0.99  | 0.5 |
| 3 | Run | 0–1  | 1 |

Input ports:
- `reset` — **CONTROL**

Output ports:
- `clock` — **CONTROL**

## Signal behavior

CONTROL `reset` input (unpatched=0) has a rising-edge threshold >=0.5. CONTROL `clock` output is binary 0/1. BPM: 20–300, default 120. Ticks per beat: numeric 1–24, default 1, rounded to nearest integer with ties upward. Duty: 0.01–0.99, default 0.5. Run: 0–1, default 1, high when >=0.5. Paused clock outputs zero and freezes phase; input reset edges still establish phase zero. Host reset zeros phase and edge memory, retaining targets. It is not MIDI clock, a note-event source or a global transport.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_clock)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/clock` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
