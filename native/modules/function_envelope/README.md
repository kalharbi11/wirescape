# Function Envelope — Wirescape Essentials

Module ID: `essentials.function_envelope` | Library: `ws_function_envelope` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | Rise | 0.0001–30 s | 0.01 |
| 1 | Fall | 0.0001–30 s | 0.1 |
| 2 | Curve | -1–1  | 0 |

Input ports:
- `gate` — **CONTROL**

Output ports:
- `envelope` — **CONTROL**

## Signal behavior

CONTROL `gate` input >=0.5 is high; unpatched gate=0. CONTROL `envelope` output is normally between 0 and 1. Rise and Fall are 0.0001–30 s (defaults 0.01 and 0.1 s); Curve is -1 to +1 (default 0). A rising gate edge rises from the current level to 1 and holds at 1 while high; a falling edge descends from the current level to 0 and holds at 0 while low. A repeated high is not a retrigger. Reset sets gate memory low and level zero without changing parameter targets.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_function_envelope)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/function_envelope` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
