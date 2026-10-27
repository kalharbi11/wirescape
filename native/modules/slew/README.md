# Slew — Wirescape Essentials

Module ID: `essentials.slew` | Library: `ws_slew` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | Rise | 0–10 s/unit | 0.1 |
| 1 | Fall | 0–10 s/unit | 0.1 |

Input ports:
- `control` — **CONTROL**

Output ports:
- `control` — **CONTROL**

## Signal behavior

CONTROL `control` input (unpatched=0) is continuously rate-limited into CONTROL `control` output. Rise and Fall each span 0–10 seconds per unit; both default to 0.1 s/unit. Values outside [0,1], including negatives, remain legal and unclipped. A zero direction time snaps to the input in one sample. Reset zeros the held output but retains both configured rates.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_slew)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/slew` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
