# VCF — Wirescape Essentials

Module ID: `essentials.vcf` | Library: `ws_vcf` | Experimental C++17 ABI v1.

## Controls

| ID | Parameter | Range and unit | Default |
| ---: | --- | --- | ---: |
| 0 | Cutoff | 20–20000 Hz | 1000 |
| 1 | Resonance | 0–0.95  | 0 |

Input ports:
- `audio` — **AUDIO**

Output ports:
- `low12` — **AUDIO**
- `low24` — **AUDIO**

## Signal behavior

Mono AUDIO `audio` (unpatched=0) feeds two independent output buffers, AUDIO `low12` and `low24`. Cutoff: 20–20000 Hz, default 1000; Resonance: 0–0.95, default 0. The internal effective cutoff is min(target, 0.45*sampleRate). Both parameters transition over 5 ms through the supplied LinearRamp. Reset zeroes four pole states, snaps both parameter targets. No final gain or output hard clipping is applied.

## Integration

The ABI and supplied `wirescape/module.h`, `wirescape/linear_ramp.h`, offline engine, reference modules and root CMake are unchanged. `CMakeLists.txt` calls only `wirescape_add_module(ws_vcf)`; the inventory file `module.json` uses the reserved IDs and library name. Copy this entire folder as `native/modules/vcf` *after* local review and compile/retest there. No MIDI, host transport, Qt, files, sockets, locks or allocations occur in the sample processor. A valid prepare configuration is required before processing and the caller must pass spans <= configured max block size. Descriptor parameters reject invalid IDs, nonfinite values and out-of-range events. Audio/control input must normally be finite.

Original implementation, prepared specifically for this task. Distributed under the project GPLv3 license; see `LICENSE`. Shared `LinearRamp` and ABI header are **not bundled** with the returned module; they are supplied by the host repository.
