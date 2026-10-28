<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Writing a Wirescape module (ABI 2 SDK guide, v2)

This guide is for people and AI agents. Follow it top to bottom for a new module. It replaces the terse v1 notes (their content is kept in §13 "Reference").

**Start from a worked example, not a blank file:**

| You are building | Copy | It shows |
|---|---|---|
| A signal processor (filter, gain, shaper) | `native/sdk/template/` | The smallest module: one input, one output, one parameter, CONSTANT fast path |
| A signal source (random, LFO, noise, S&H) | `native/sdk/examples/random_source/` | Clock input with `ws::Schmitt`, seeded `ws::Random` per voice, the output channel count set by a parameter, smoothing, CONSTANT path, versioned state |
| Anything that plays notes (sequencer, arpeggiator, chord tool) | `native/sdk/examples/step_sequencer/` | Clock/reset/run, pitch + gate CV **and** note events via `b.emit`, reset-wins rule, ties, never stranding a note-off, versioned state |

Real ported modules live in `native/modules2/` (for example `noise`, `slew`, `vca`). Read one only if the examples don't cover your case.

---

## 1. Folder layout and files
```
native/modules2/<name>/          (or native/sdk/examples/<name>/ for MIT examples)
  dsp.h        the module class (all the code)
  module.cpp   two lines: #include "dsp.h" and WS_REGISTER_MODULE(Class)
  module.json  metadata the host and patch migration read
  tests.cpp    callback-level tests (CTest)
  README.md    what it does, ports and parameters table, units
  DESIGN.md    design notes, provenance, third-party sources with pinned revisions and licences
  LICENSE      only if it differs from the folder's default
```
- Every new file starts with an SPDX header: `// SPDX-License-Identifier: GPL-3.0-only` for Essentials and app code, `MIT` for SDK examples. Your own modules may use any licence (the SDK is MIT); code derived from GPL sources stays GPL.
- `module.json`: `schema_version` 2, `abi_version` 2, `id`, `name`, `library` (the CMake target, prefix `ws2_`), `parameters` mapping each parameter ID to its **slot index** (0, 1, 2… in the order of `params`; not a default value), `inputs`/`outputs` mapping port IDs to `"signal"` or `"event"`, optional `description` and `license`.
- Build wiring: add a `ws2_<name>` SHARED library and an `abi2_<name>_tests` executable + `add_test` in `native/engine2/CMakeLists.txt` (see how `guide_ringmod` and the examples are wired).

## 2. IDs never change
- Module ID: `<library>.<name>`, for example `essentials.vcf`, `midi.to_cv`, `ports.macro_oscillator`, or your own prefix (`kal.random`).
- Port and parameter IDs are short lowercase strings (`pitch`, `gate`, `cutoff`, `step3`).
- Saved patches refer to these IDs. **Once a module is merged, never rename, renumber or reuse an ID.** Add new ports/parameters at the end; mark retired ones `WS2_HIDDEN` instead of deleting them.
- `ws::Info{id, name, version}`: bump `version` when behaviour changes; the ID stays.

## 3. Ports, parameters and events: which is which
| Use | When | Declared with |
|---|---|---|
| **Input/output signal port** | Anything that can be patched with a cable: audio, pitch, CV, gates, clocks | `ws::port(id, name, hint, poly, unpatched, stereo_pair, WS2_SIGNAL)` |
| **Parameter** | A knob or switch on the panel. The host smooths it, adds cable modulation and hands you a signal in plain units | `ws::param(id, name, min, max, default, unit, flags, taper, legacy, curve, step)` |
| **Event port** | Discrete things with a sample offset: notes, MIDI, OSC messages | `ws::port(id, name, WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)` |

- Any knob can be modulated by a cable (the host does it, in normalized space, per voice). Don't add a separate "cutoff CV" input unless the module needs special CV behaviour.
- Parameter flags: the default is `WS2_MODULATABLE|WS2_AUTOMATABLE`. Switches and counts use `WS2_STEPPED|WS2_AUTOMATABLE` with `step = 1` (no smoothing, integer values). Add `WS2_PER_VOICE` when each voice may get its own modulation.
- Tapers: `WS2_LINEAR` (default), `WS2_LOG` (needs `min > 0`, good for frequency/time), `WS2_EXP` with `curve`.
- Hints (`WS2_AUDIO`, `WS2_PITCH`, `WS2_CV_BI`, `WS2_CV_UNI`, `WS2_GATE`, `WS2_TRIGGER`, `WS2_EV_*`) colour the UI and pick defaults. They **never block** a connection: any signal may go anywhere.

## 4. Units (one convention everywhere)
- Audio: ±1 nominal.
- Pitch: **1.0 per octave, 0 = C4** (MIDI 60). Frequency = `261.6256 * 2^pitch`.
- CV: bipolar ±1 or unipolar 0..1.
- Gates and triggers: 0 or 1. Read them with `ws::Schmitt` (high above 0.5, low below 0.25). Emit triggers of at least 1 ms with `ws::Trigger`.
- Time parameters in seconds, frequencies in Hz, levels linear. Put the unit string in `param(..., unit)`.

## 5. Polyphony (channels are voices)
- A cable carries 1–16 channels. **Channels are voices, never left/right.** Stereo is two ports linked with `stereo_pair`.
- **The source sets the count.** A processor sets each output to `b.channels()` (the host computes it from the inputs and the port's poly policy). A source (random, sequencer, MIDI→CV) chooses its own count, usually from a `voices` parameter, and calls `b.out(i).set_channels(n)` every block.
- Poly policy per input: `WS2_POLY` (per voice; a mono cable broadcasts to all voices), `WS2_SUM` (voices are summed, e.g. an output module), `WS2_FIRST` (only channel 0, e.g. a sequencer clock).
- `b.in(i).ch(c)` already maps any channel to 0 for a 1-channel cable, so mono inputs "just work" in a poly module.
- Keep per-voice state in fixed `std::array<Voice, 16>`, never in vectors grown at runtime.

## 6. CONSTANT: the cheap path
A signal flagged CONSTANT has the same value for the whole block; only sample 0 is valid.
- Inputs: `b.in(i).constant()`; read `ch(c)[0]`.
- Outputs: if every input you depend on is constant **and** your state doesn't move, call `mark_constant(true)` and write only sample 0. Otherwise `mark_constant(false)` and write every sample.
- Running oscillators and noise are never constant. Smoothers become constant only once settled (snap to the target).
- Correctness first: a wrong CONSTANT flag is a bug; a missing one is only slower.

## 7. Memory and lifecycle
| Call | Thread | May allocate? | Do |
|---|---|---|---|
| `memory_requirements(config)` (static, optional) | control | no | Report extra bytes you will request in `prepare` (`fast_bytes`, `large_bytes`) |
| `prepare(const ws::Setup&)` | control, engine quiescent | **yes**, only via `s.alloc(bytes, align, WS2_FAST/WS2_LARGE)` | Size buffers for `s.max_block()`, `s.max_channels()`, `s.sample_rate()`. Return `false` if `alloc` returns null |
| `reset()` | audio or control | no | Return to the deterministic start state. Cannot emit events |
| `process(ws::Block&)` | audio | **no** | The work |
| `save_state` / `load_state` (optional) | control | no | Versioned, portable state |
| `latency_frames()` (optional) | control | no | True path latency, if any |

`LARGE` is for big buffers (delay lines, comb filters, wavetables); `FAST` for hot state.

## 8. Realtime rules (checklist for `process`, `reset` and event handling)
- [ ] No `new`, `malloc`, `std::vector` growth, `std::string`, `std::function` captures that allocate.
- [ ] No locks, atomics waits, sleeps, file or network I/O, `printf`/logging.
- [ ] No exceptions thrown (the wrapper catches them and silences the output, but that's a failure).
- [ ] Bounded work per sample and per event; no loops that depend on unbounded input.
- [ ] Never assume a block size: any `frames` from 1 to `max_block()` must give the same result as one big block.
- [ ] Outputs always finite (the wrapper rejects NaN/Inf and silences the module). Guard feedback paths and divisions.
- [ ] Randomness from `ws::Random` seeded by `config.seed` + voice, so renders are reproducible.
- [ ] No Qt, OS or platform intrinsics; use `ws::simd::f4` for SIMD (scalar fallback included).

## 9. Process, step by step
```cpp
void process(ws::Block& b) {
    const uint32_t voices = b.channels();          // or your own count for a source
    b.out(0).set_channels(voices);
    const bool constant = b.in(0).constant() && b.param(0).constant();
    b.out(0).mark_constant(constant);
    const uint32_t n = constant ? 1 : b.frames();
    for (uint32_t c = 0; c < voices; ++c) {
        auto in = b.in(0).ch(c);                   // constant-aware view, index with in[i]
        auto gain = b.param(0).ch(c);
        float* out = b.out(0).ch(c);               // only outputs give a writable pointer
        for (uint32_t i = 0; i < n; ++i) out[i] = in[i] * gain[i];
    }
}
```
- `auto x = b.in(i).ch(c)` is a value view (handles CONSTANT and unpatched for you), not a `const float*`.
- For tight loops or SIMD, get a plain pointer with `b.in(i).row(c, scratch, frames)`: it aliases a varying signal or fills `scratch` for a constant one. Reserve one scratch row per simultaneously live input in `prepare` and count it in `memory_requirements` (see `native/sdk/template/module.cpp`).
- Parameters arrive already smoothed and modulated, in plain units, through `b.param(i)` (same view API as inputs).

## 10. Events (notes, MIDI, messages)
- Read: `for (const auto& e : b.events_in(port))` or, for sample accuracy, `b.split(range_fn, event_fn)`: it calls `range_fn(first, end)` for the samples between events and `event_fn(e)` at each event, in order. Offsets are relative to this block.
- Write: fill a `ws2_event` completely (`struct_size = sizeof(ws2_event)`, `type`, `offset`, `port`, `note_id`, `channel`, `key`, `value`; zero the rest) and call `b.emit(output_port_index, e)`. It returns `false` when the bounded queue is full.
- **Never strand a note-off.** If `emit` fails for a note-off, keep it pending and retry at the next sample or block. Never spin.
- Note IDs: give each new note a fresh `note_id`; send the matching NOTE_OFF with the same `note_id` and `key`. Stealing a voice: NOTE_CHOKE for the old note before NOTE_ON for the new one, at the same offset.
- Same-frame clock and reset: **reset wins** (the clock then plays step 1). Stop or reset releases any sounding note at that sample.
- Don't keep pointers to event payloads after `process` returns. `b.transport()` may be null.
- For note → pitch/gate/velocity across voices, use the shared allocator in `native/voice/` rather than writing your own.

## 11. State save/load
- Only state the parameters don't already hold (the host saves parameters): playback position, held values, RNG state.
- Write a version number first, then portable fields with `w.u32(...)` / `w.f32(...)` (little-endian, fixed). Don't use `value<T>` for structs in shared files.
- `load_state`: read everything into a staged copy, validate, then commit. Unknown version → `WS2_UNSUPPORTED` (the host keeps the bytes). Short or garbage data → `WS2_INVALID` with the current state unchanged.
- RNG: `ws::Random::save()` / `restore()` give portable state (see the random source example).
- Patch compatibility matters more than exact DSP continuity: never break loading of an older version.

## 12. Tests (copy from the examples) and definition of done
Tests call the real C callbacks through the wrapper (`ws::Module<Class>::descriptor()`), not just the class. Every module covers:
1. **Frame matrix:** one block vs blocks of 1, 17, 64 and 256 frames give bit-identical output (and identical events at absolute frames).
2. **Rate matrix:** 44.1, 48 and 96 kHz prepare and behave (times scale with rate).
3. **Channel matrix:** 1, 4 and 16 voices; voices are independent; mono input broadcasts.
4. **Reset determinism:** reset → same output as a fresh instance.
5. **State round trip:** save mid-run, load into a fresh instance, continue → identical. Unknown version and truncated data rejected.
6. **CONSTANT:** set when expected, never on running audio.
7. **Extremes:** min/max parameters and hot inputs stay finite.
8. **No allocation in process** (`native/tests2/allocation_watch.h`).
9. Behaviour tests for what the module is for (independent reference math, not the same formula copied).

**Definition of done**
- [ ] Folder complete (§1), SPDX headers, `module.json` slot indices match `params` order.
- [ ] IDs final and documented in `README.md` (ports, parameters, units, ranges).
- [ ] Realtime checklist (§8) passes by reading your own `process`.
- [ ] `python tools/dev.py build --headless` and `python tools/dev.py test` pass locally.
- [ ] The module appears in the app catalogue (one line at the catalogue registration point; see `docs/ARCHITECTURE.md` once J1 lands) and plays in a patch.
- [ ] Tried on the Pi if it is heavy (`docs/PI_SETUP.md`, after J3): `wirescape play` with your patch.
- [ ] Third-party code pinned and recorded in `DESIGN.md` with its licence.

**Common mistakes**
- Storing `b.in(i).ch(c)` as `const float*` (it's a view; use `auto`).
- Forgetting `set_channels` on a source, so the output stays at 1 voice or a stale count.
- Marking a smoother CONSTANT before it has settled, or an oscillator CONSTANT at all.
- Allocating in `reset()` or resizing a container in `process`.
- Using `std::rand`, time or addresses as seeds (renders become irreproducible).
- Reading a gate with `> 0` instead of `ws::Schmitt` (chatter on noisy CV).
- Emitting NOTE_ON without a guaranteed NOTE_OFF path on stop, reset, run-off and capacity-full.
- Treating channels as left/right.
- Writing `module.json` parameter values as defaults instead of slot indices.

## 13. Reference (SDK API in one place)
- `ws::port(id, name, hint=WS2_ANY, poly=WS2_POLY, unpatched=0, stereo_pair=nullptr, kind=WS2_SIGNAL)`. `unpatched` is the value an unconnected input reads.
- `ws::param(id, name, min, max, default, unit="", flags=MODULATABLE|AUTOMATABLE, taper=LINEAR, legacy_index=0, curve=4, step=0)`. `legacy_index` keeps ABI 1 meaning for ported modules.
- Class shape: `static constexpr ws::Info info`, `static constexpr std::array inputs, outputs, params` (empty: `std::array<ws2_port,0>` / `std::array<ws2_param,0>`), `prepare`, `reset`, `process`; optional `memory_requirements`, `save_state`, `load_state`, `latency_frames`, `query_ports(config, ports)` (config-dependent ports with stable IDs). End with `WS_REGISTER_MODULE(Class)`; the wrapper generates the C ABI, validation and the exception barrier.
- `ws::Setup`: `sample_rate()`, `max_channels()`, `max_block()`, `config.seed`, `alloc(bytes, alignment=32, hint=WS2_FAST)`.
- `ws::Block`: `frames()`, `channels()`, `sample_rate()`, `frame_position()`, `in(i)`, `param(i)`, `out(i)`, `events()`, `events_in(port)`, `split(range, event)`, `emit(port, e)`, `transport()`.
- Signal view: `channels()`, `constant()`, `connected()`, `ch(c)[i]`, `value(c, i)`, `row(c, scratch, frames)`. Output: `set_channels(n)`, `ch(c)` (writable), `mark_constant(bool)`, `channels()`.
- Helpers (`native/sdk/signals.h`): `ws::Schmitt`, `ws::Trigger` (≥1 ms), `ws::Random` (`seed(instance, channel)`, `next()`, `bipolar()`, `save()`, `restore()`), `ws::LinearRamp`. `native/sdk/simd.h`: `ws::simd::f4` (load/store, `+ - *`). `native/sdk/fastmath.h`: `tan_small` only for |x| ≤ 0.25 rad. No global fast-math flags; keep `double` only where precision needs it.
- Limits: 16 channels, 32 ports, 32 parameters, event payload ≤ 1,472 bytes.
- **Compatibility promise (from J-R2):** additive SDK changes only; a breaking change needs a migration note here and in the PR.

## 14. Where to look next
- The contract and its reasons: `docs/specs/ABI2_PLAN.md` (D1–D16).
- How the app and engine fit together: `docs/ARCHITECTURE.md`.
- Example of a guide-only authoring exercise and its findings: `native/tests2/guide_ringmod/README.md`.
