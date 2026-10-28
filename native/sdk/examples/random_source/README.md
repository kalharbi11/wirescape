<!-- SPDX-License-Identifier: MIT -->
# Example Random Source (`example.random_source`)

A worked SDK example: a clocked random sample & hold with a smoothed twin output. It shows a
source module that sets its own channel count, per-voice state, deterministic seeding, a
`CONSTANT` fast path and versioned state. Library `ws2_example_random_source`, test
`abi2_example_random_source_tests`. No audio device is opened.

Each rising edge of `clock` draws a fresh random value in [-1, 1) per voice and holds it on `step`.
`smooth` is a one-pole lowpass of `step`. Both outputs are multiplied by `range`.

| Kind | ID | Hint / range | Notes |
|---|---|---|---|
| Input | `clock` | `WS2_TRIGGER`, poly | Rising edge (Schmitt, above 0.5 on / below 0.25 off). A mono clock drives every voice. |
| Output | `step` | `WS2_CV_BI` | Held random value times `range`, in [-range, range). |
| Output | `smooth` | `WS2_CV_BI` | One-pole lowpass of the held value, times `range`. |
| Param | `voices` | 1..16, default 1, step 1 | Output channel count. Stepped, automatable, not modulatable. Read once per block. |
| Param | `smooth` | 0..1 s, default 0.05 | Time for `smooth` to reach 63.2% of a new value. 0 means no smoothing. |
| Param | `range` | 0..1, default 1 | Output scale (unitless). |

Voices are seeded from `config.seed` and the voice index, so the same seed gives identical output
and different voices give different sequences. A voice that becomes active later starts from its
seeded state and sees a high clock as a rising edge.

Run it with `ctest --test-dir build -R example_random_source`.
