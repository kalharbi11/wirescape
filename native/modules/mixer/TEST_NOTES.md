<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Mixer: implementation and test notes

## ABI and behavior

- C++17 module using the provided `wirescape/module.h` ABI, one C entry point (`ws_module_entry`), and the standard `wirescape_add_module(ws_mixer)` helper.
- Audio input IDs `a` and `b`; audio output ID `audio`. The two gain parameters are numeric IDs 0 (`Level A`) and 1 (`Level B`), range 0–2, initial value 0.5 each.
- Each output sample is `(a ? a[i] : 0) * level_a + (b ? b[i] : 0) * level_b`. No clipping, normalization or other gain processing.
- `prepare` stores the supplied sample rate and maximum block size (and rejects a missing/undersized configuration, nonpositive rate or zero max frames). `reset` preserves gain values. The bounded audio loop writes exactly `context.frames` samples and does no allocation or blocking work. Parameter events update their corresponding gains; all other event types are ignored.
- The host is responsible for supplying valid processing buffers, a prepared instance, frame counts up to the prepared maximum, and parameter values within the declared 0–2 range.

## Executed tests

**Environment:** Linux x86-64, GNU g++ 14.2.0, C++17. Tests used a temporary standalone descriptor-level C++ harness and a copy of the supplied ABI header under the include path `wirescape/module.h`. That harness and compiled binaries are intentionally excluded from this four-file deliverable.

**Compile command (temporary harness):**

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
  -I test_support/include \
  native/modules/mixer/module.cpp test_support/test_mixer.cpp \
  -o test_support/test_mixer
./test_support/test_mixer
```

The harness called the exported descriptor's `create`, `prepare`, `event`, `process`, `reset`, and `destroy` callbacks directly. **All checks passed:**

| Check | Inputs / action | Expected output or assertion |
| --- | --- | --- |
| Metadata | Read exported descriptor | ABI v1, `essentials.mixer`, 2 mono inputs, 1 mono output, 2 correctly labeled/ranged parameters |
| Both disconnected | `a = null`, `b = null`; 4 frames | `[0, 0, 0, 0]` |
| Only A | `a = [1, -2, 0.5, -0.5]`, `b = null` | `[0.5, -1, 0.25, -0.25]` |
| Only B | `a = null`, `b = [2, 1, -1, -0.25]` | `[1, 0.5, -0.5, -0.125]` |
| Default summed gain | Both preceding connected arrays | `[1.5, -0.5, -0.25, -0.375]` |
| Changed gain, negative input, no clipping | Set A=2, B=0.25; `a = [1, -2, 0.5, -0.5, 3]`, `b = [2, 1, -1, -0.25, 4]` | `[2.5, -3.75, 0.75, -1.0625, 7]` |
| Zero gain | Set A=0, leave B=0.25 | `[0.5, 0.25, -0.25]` |
| Ignored events | Send gate, note-on and unknown parameter-ID event | Existing coefficients and samples unchanged |
| Reset preserves params | Reset after A=0, B=0.25 | `[0.5, 0.25, -0.25]` still |
| Frame partition invariance | Set A=1.25, B=0.75; process 8 frames as one call and as 3 + 0 + 2 + 3 | Results identical |
| Exact output extent | Initialize 5 guard samples after each requested region to `-1234.0`; process varying counts including 0 | Guard region unchanged on every call |

**Additional checks (passed):**

- Built as a shared library with `-std=c++17 -O2 -Wall -Wextra -Werror -fvisibility=hidden -fPIC -shared`; `nm -D --defined-only` showed precisely one defined dynamic symbol: `ws_module_entry`.
- Parsed `module.json` with Python's standard `json` library, checked the exact manifest field set and `schema_version`/`abi_version` against the attached VCA example, and checked all mixer IDs, port kinds, parameter mappings and library name.

No host integration, physical audio-interface, Raspberry Pi, Windows or performance/latency benchmarks were run.
