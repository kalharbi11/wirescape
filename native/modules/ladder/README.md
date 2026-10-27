# Ladder Filter — Wirescape Essentials

Module ID: `essentials.ladder` | Library: `ws_ladder` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | Cutoff | 20–20000 Hz | 1000 |
| 1 | Resonance | 0–1  | 0 |
| 2 | Drive | 0–2  | 0 |

Input ports:
- `audio` — **AUDIO**

Output ports:
- `audio` — **AUDIO**

## Signal behavior

Mono AUDIO `audio` (unpatched=0) passes through one AUDIO `audio` output. Cutoff: 20–20000 Hz (default 1000), Resonance 0–1 (default 0), Drive 0–2 (default 0). All three use the shared 5 ms LinearRamp. Effective cutoff is limited to 0.45*sampleRate. Reset zeroes four pole states and prior feedback output, and snaps all parameter targets. There is no automatic output gain or final hard clip.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_ladder)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/ladder` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
