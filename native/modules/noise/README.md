# Noise — Wirescape Essentials

Module ID: `essentials.noise` | Library: `ws_noise` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | Level | 0–1  | 0.2 |

Input ports:
- None

Output ports:
- `audio` — **AUDIO**

## Signal behavior

There are no inputs. AUDIO `audio` emits bipolar white noise in the interval [-Level, Level); Level defaults to 0.2 and has range 0–1. No host gain is applied. Unpatched input is not applicable. Reset reseeds the generator to `0x12345678` and snaps Level to the latest target. Reprepare also reseeds. A Level of zero emits exact silence after the 5 ms ramp has settled.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_noise)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/noise` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
