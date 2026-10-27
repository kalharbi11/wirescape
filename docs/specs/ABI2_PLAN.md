# Wirescape ABI 2: module contract plan

**Status:** approved by the user, October 26, 2026. This is the source of truth for every decision that affects how modules are written. It is scheduled in `docs/PLAN.md` (milestones M1–M3 and M8). Individual tasks are GitHub issues that point to files in `docs/tasks/`. The companion file `docs/specs/COMMUNICATION_MODULES_PLAN.md` covers MIDI, OSC, UDP and Monitor and depends on this one.

## 0. Read me first

**Why this exists.** ABI 1 (`native/include/wirescape/module.h`) has mono float buffers, numeric-only parameters, host-to-module events only, and a graph without cycles. Polyphony, cable modulation of parameters, event outputs, feedback, large-memory effects and future MIDI/OSC/DMX all need changes to the module contract. If those changes come after many modules exist, every module has to be rewritten. So the contract changes first. The 13 Essentials get ported once, with tests proving their sound is unchanged.

**What stays.** Everything in `AGENTS.md` still applies: audio off in tests and CI, the realtime rules, the privacy rules, and two attempts then leave evidence. `legacy/` is read-only reference.

Also preserved:
- The C ABI as the binary boundary.
- Quiescent create/prepare/destroy.
- Bounded, allocation-free processing.
- Absolute frame time, the generation/panic concept, offline determinism.
- Stable `essentials.*` module IDs and parameter meanings.
- The `WS_MODULE_ENTRY_NAME` static-link pattern.
- Staged graph replacement in the controller.

**Order of tracks:**
- **Track A (independent):** the unattended audio capture/analysis harness in `docs/specs/AUDIO_CAPTURE_HARNESS.md`. It's test infrastructure and doesn't depend on ABI 2.
- **Track B:** this plan, phases B1–B6.
- **Track C:** communication modules (separate file), starting after B3.
- **Then:** sequencers, subpatches and macros, the CHAOS rebuild, and export/adapters.

**Working rules for this plan:**
- One phase per issue, branch and PR. Each phase ends with `python tools/dev.py check --package` green locally, CI green on the PR, and a handoff comment on the issue. `docs/STATUS.md` is updated when a milestone completes.
- No phase may change the sound of an existing Essentials patch unless the change is listed in this file and covered by a test.
- The draft ABI 2 header is experimental until phase B4 finishes. Don't publish or freeze it.
- When this file says "decide", record the decision in section 9 with a one-line reason.

## 1. Goals

1. One module source runs unchanged in Wirescape, a CLAP/VST3 plugin and a Daisy Seed firmware. Only a thin adapter differs.
2. Module authors (people or AI) write a plain C++ class against a small header. The SDK handles validation, smoothing, modulation, channel broadcasting and the C ABI.
3. Polyphony, audio-rate modulation, feedback patching and large-memory effects (reverb, looper, tape) work at high quality with low CPU.
4. MIDI, OSC, UDP, DMX (Art-Net/sACN), serial/Arduino and scripting can be added later as host services and ordinary modules, with no further ABI change.

## 2. Decisions

Each decision gives the rule, then how it maps to plugins (CLAP/VST3) and to Daisy, then the acceptance tests.

### D1. Signals: what a cable carries
- Every signal cable carries **float32 samples at the audio rate**, in blocks of 1..`max_block_frames` frames. Modules must handle any frame count, including 1 and odd sizes.
- A signal has **1–16 channels**. Layout is planar: `ch[c][frame]`, and each channel's pointer is 16-byte aligned (32 recommended).
- Each signal carries a **CONSTANT flag**. When it's set, every sample in the block equals `ch[c][0]`. The host sets it for unpatched inputs and unmodulated parameters. A module may set it on outputs it knows are constant. Modules use it to skip per-sample coefficient math (for example, the VCF's `tan` call).
- **Plugins:** plugin buffers map directly. **Daisy:** same code, with the channel maximum fixed at build time.
- **Tests:** frame counts 1, 17, max. Channel counts 1, 4, 16. The constant flag is honoured and never wrongly set.

### D2. Polyphony: channels on the cable (VCV Rack model)
- The **source sets the channel count**, and it spreads downstream. Rules:
  - A poly module's output count = the maximum channel count among its poly inputs, unless the module declares otherwise.
  - **Mono into poly:** a 1-channel signal is broadcast to every channel. The SDK does this, not the module.
  - **Poly into mono:** the port declares `SUM`, `FIRST` or `POLY` (no conversion).
  - A 0-channel cable behaves as unpatched.
- The **channel count may change between blocks.** Buffers are preallocated for `max_channels` at prepare time. A channel-count change never allocates.
- There is **one voice allocator, in portable code** (`native/voice/`), shared by MIDI In, sequencers and the on-screen keyboard.
  - It outputs poly `pitch`, `gate`, `velocity`, `pressure` and `timbre` signals, plus note events with `note_id`.
  - Modes are explicit and saved in the patch: Rotate, Reuse, Reset, MPE, and Mono with last/low/high priority.
  - Steal policy (oldest, quietest, lowest priority) and max voices are explicit.
  - **No automatic channel assignment that the user can't see or change.**
- **Voice-heavy modules** (oscillator banks, filters, envelopes) process channels in groups of 4 or 8 through the SDK SIMD type. Simple modules (VCA, mixer) just loop and let the compiler vectorize.
- **Plugins:** max channels = plugin voice count. CLAP `note_id` maps to channels through the allocator. CLAP polyphonic parameter modulation becomes per-channel modulation (see D7). **Daisy:** compile-time max (4–8 recommended), with the same allocator code.
- **Tests:**
  - Broadcast, sum and first behaviour.
  - Channel count changes without allocating (use an allocation-counting hook in test builds).
  - All allocator modes against reference note sequences, including steal and MPE.

### D3. Stereo
- Stereo is **two ports** (`left` and `right`) linked by a `stereo_pair` hint. The UI draws them as one cable and can split them. **Channels always mean voices, never L/R.**
- The engine gets **N output channels** (at least stereo). This replaces the single mono output node and port (`engine.cpp:266`). PortAudio stops duplicating mono.
- **Plugins:** the plugin's L/R bus. **Daisy:** the codec's L/R.
- **Tests:** a stereo output render, a mono source into a stereo pair, and a saved patch still opening.

### D4. Units and signal hints (one convention everywhere)

| Hint | Meaning | Hardware adapter (Eurorack-style) |
|---|---|---|
| `audio` | ±1.0 = digital full scale | ×5 V |
| `pitch` | 1.0 per octave; 0.0 = C4 = 261.6256 Hz; MIDI note n → (n−60)/12 | ×1 V (1 V/oct) |
| `cv_bipolar` | −1..+1 | ×5 V |
| `cv_unipolar` | 0..1 | ×5 V (the adapter may choose 8–10 V per jack) |
| `gate` | 0 or 1. Inputs use the SDK Schmitt trigger (on above 0.5, off below 0.25) | ×5 V |
| `trigger` | Rising edge. Sources emit 1.0 for ≥1 ms, independent of sample rate | ×5 V |
| `any` | No assumption | — |

- These are the "digital" equivalents of VCV's voltage standards. That means plugin and codec outputs need no rescaling, and only hardware CV jacks are scaled, using the port's hint. The UI may display volts.
- **Hints never block a connection** (see D5). They drive cable colour, warnings and hardware/plugin scaling.

### D5. Connection rules (creative freedom first)
- **Remove the port-kind rejection** at `engine.cpp:250–251`. Audio into CV (audio-rate FM) is allowed. Mismatched hints show a soft warning colour only.
- **Signal inputs:** one source per input (keep the current rule). Mixing uses a Mixer, or modulation (D7) for parameters.
- **Event inputs:** **many sources allowed.** Events are merged by frame, with a stable order on ties (Pd-style fan-in).
- **Fan-out:** any output to any number of inputs, for both signals and events.

### D6. Parameters (rich, stable descriptors)
Each parameter declares:
- `id`: a stable string such as `"cutoff"`, plus the existing numeric index for ABI 1 migration.
- `name`, `unit`, `min`, `max`, `default`.
- `taper`: `linear`, `log` or `exp`, with a curve value.
- `step`, or named `options` for enums.
- Display formatting (decimals, or a value-to-text function).
- `flags`:
  - `MODULATABLE`
  - `PER_VOICE`
  - `AUDIO_RATE_OK` (the module handles a per-sample changing value well)
  - `STEPPED` (no smoothing)
  - `AUTOMATABLE`
  - `HIDDEN`

Further rules:
- **Smoothing moves into the SDK/host:** a 5 ms linear ramp on base value changes, sample-counted, matching today's `LinearRamp`. Per-module ramp copies go away during the port. `STEPPED` parameters jump.
- **Modules receive each parameter as a signal** (D1) in plain units, already smoothed, modulated and tapered. When it's constant it costs one value. Authors read parameters the same way they read inputs.
- **Plugins:** this maps one-to-one onto CLAP/VST3 parameter info (stable IDs, ranges, flags, value-to-text). **Daisy:** knobs set base values.

### D7. CV → parameter modulation (host-side, non-destructive)
- **Any signal cable can connect to any `MODULATABLE` parameter.** The cable carries an **amount** from −1..+1, shown on the cable as an attenuverter. A parameter may have several modulation cables.
- **The math happens in normalized space:**
  - `effective = clamp(base_norm + Σ amount_i × cv_i, 0, 1)`, then the taper maps it to plain units.
  - `cv_i` is read as bipolar. Unipolar sources simply give 0..amount.
  - Poly modulation makes the parameter signal poly. That gives per-voice cutoff and similar for free.
- **Modulation never overwrites the base value.** The knob shows its base value plus a live ring for the modulated value. This removes the CHAOS-style ownership and precedence problem: macros are just modulation sources.
- **Exact musical pitch stays an explicit port.** A VCO keeps a dedicated `pitch` (1 V/oct) input and an FM input with its own semantics, because normalized parameter modulation can't track 1 V/oct exactly.
- **Cost control:** unmodulated, non-ramping parameters are CONSTANT. A modulated parameter costs one fused multiply-add per sample per cable.
  - Modules without `AUDIO_RATE_OK` may sample the value every 16–32 frames, using an SDK helper.
  - Modules with it (for example, filter FM) compute per sample when the input is non-constant.
- **Plugins:** this equals CLAP parameter modulation, including per-note modulation. VST3 gets automation, and modulation stays internal. **Daisy:** knob = base value, CV jack = modulation cable with an amount.
- **Tests:**
  - Modulated and unmodulated renders.
  - Amount sign and clamping.
  - Poly modulation of a mono-declared parameter.
  - Zero CPU change when unmodulated (benchmark).

### D8. Events: in, out, and messages
- **Per-block event lists, sorted, with sample offsets** (the CLAP model). The engine **stops splitting the whole graph per event** (removes the `engine.cpp:430–447` spans and the 64-span overload). Modules that need sample accuracy split internally with an SDK helper.
- **Event ports** are a real port kind: inputs and **outputs**. Output events go to downstream modules in the same block (topological order) or to host egress (D13).
- **Event types:**
  - `NOTE_ON`, `NOTE_OFF`, `NOTE_CHOKE`: `note_id`, `channel` (voice), `key`, `velocity` as float 0..1, `source_id`.
  - `NOTE_EXPRESSION`: per-note pitch bend, pressure, timbre, pan, as float.
  - `PARAM`: set a base value. `PARAM_MOD`: a per-note modulation offset.
  - `TRANSPORT`: tempo/position/play changes.
  - `MIDI`: stored **UMP-style** (32-bit resolution fields, group plus channel), so MIDI 2.0 later is no rewrite.
  - `MESSAGE`: kind (`osc`, `udp`, `text`, `bytes`, `struct`), plus a size and a pointer into a **per-block arena owned by the engine**. Bounded at 1472 bytes payload by default. Never stored in a float buffer.
- **Budgets degrade instead of silencing:**
  - The per-block event budget defaults to 1024. On overflow, coalesce `PARAM` (latest wins), then drop the excess `MESSAGE` and `NOTE_EXPRESSION` events with counters.
  - **Never drop `NOTE_OFF` or `NOTE_CHOKE`:** reserve capacity for them.
  - This **replaces** the ABI 1 rule "overload silences the whole block", which is too harsh for live MIDI and OSC. Record this change in section 9.
- **Plugins:** this is CLAP's input/output event lists directly. **Daisy:** UART/USB MIDI handlers produce the same events.
- **Tests:**
  - Ordering and stable ties.
  - Fan-in merge.
  - Event output to a downstream module in the same block.
  - Overflow policy (note-offs survive).
  - Arena bounds.
  - A 10,000 events/s stress test with no allocation.

### D9. Process context and transport
- Every process call gets: `frames`, `frame_position`, `sample_rate`, and `transport` (tempo in BPM, beat position in quarter notes, bar start, time signature, playing/recording flags, loop range). The transport comes from an internal clock, MIDI clock (Track C), or the DAW in a plugin.
- `transport` may be absent in hosts without one. Modules must handle that case.
- **Tests:** tempo-synced LFO phase across block sizes, and start/stop/locate.

### D10. Feedback loops
- **Cycles are allowed.**
  - At prepare, the engine finds strongly connected components (Tarjan's algorithm) and picks back-edges deterministically.
  - It runs each loop in **sub-blocks of `feedback_block` frames** (default 32, range 1..block, saved per patch). Each back-edge gets exactly that delay.
  - Nodes outside loops run at the full block size.
- The UI marks back-edge cables and shows their delay.
- Sample-exact feedback (filter resonance, FM feedback, Karplus-Strong) belongs **inside** modules.
- **Plugins and Daisy:** identical code.
- **Tests:**
  - A loop is accepted. Delay equals the setting.
  - The result is deterministic and independent of node insertion order.
  - `feedback_block=1` gives a single-sample delay.
  - Loop-free patches render bit-identically to before.

### D11. Memory and resources
- **Modules never call `new`, `malloc` or the STL allocators**, except inside the SDK wrapper's `create`.
- At prepare, a module requests memory from the host allocator: `alloc(bytes, align, hint)`, where `hint` is `FAST` (small, hot state) or `LARGE` (delay lines, loop and sample memory).
- A module can also report `memory_requirements(sample_rate, max_channels, max_block)` without allocating, so a future exporter can budget a patch before building.
- **Plugins:** normal heap. **Daisy:** FAST → internal SRAM, LARGE → the SDRAM arena.
- **Tests:**
  - The allocation-count hook shows zero allocations in process, event and reset.
  - Reported requirements ≥ actual usage.
  - Re-prepare at a different sample rate.

### D12. State, latency and configuration-dependent ports
- **State:** `save_state(writer)` / `load_state(reader)` with a module-defined version number. Used for sequencer patterns, loop buffers (optional, size-capped), scene snapshots and any non-numeric config (device names, OSC address tables). Called quiescently.
- **Latency:** a module reports `latency_frames`. The engine sums it along the path to the output and reports it. Compensating latency inside the graph is a later feature, but the field exists now. In plugins it maps to plugin latency.
- **Config-dependent ports:** a module may compute its port list from its saved config (for example, the rows of a MIDI CC map table, or a script's signature) through `query_ports(config)` at prepare time. Changing them uses the existing staged graph replacement. Existing cables are kept by port ID.
- **Tests:**
  - State round-trip is byte-exact.
  - An unknown future state version is preserved, not discarded.
  - Port-list changes keep matching cables and report dropped ones.

### D13. Threads, ingress and egress (needed by Track C)
- **Ingress:** a fixed registry of **SPSC rings, one per producer context** (UI thread, each MIDI input callback context, the network thread, the script worker). They're registered quiescently, up to 32.
  - At the start of each block, the audio thread merges them by timestamp → frame, within a bounded budget, into the per-block lists (D8).
  - This **replaces** the single-enqueue-owner restriction without a lock-based MPSC queue.
- **Timestamps:**
  - Producers stamp with a monotonic nanosecond clock.
  - The audio callback publishes `(frame_position, callback_time_ns)` anchors. Mapping uses a drift-tracking filter that extends the existing `ProviderBridge` anchor arithmetic.
  - There are two scheduling modes per patch: **Immediate** (next block, lowest latency, up to one block of jitter) and **Timestamped** (fixed one-block offset, no jitter). Default: Timestamped.
- **Egress:** module output events addressed to host sinks go into one SPSC egress ring → an I/O dispatcher thread (MIDI Out, OSC Out, UDP Send, Monitor). Each event carries its frame and a mapped wall-clock time, so outputs can be scheduled.
- **Applied-generation acknowledgement:** add `applied_generation` to the meter, written after the reset is actually applied in render. This closes the gap noted in `docs/specs/PROVIDER_BRIDGE_CONTRACT.md`.
- **Tests:**
  - Interleaving several fake producers keeps per-producer order.
  - Ring-full counters.
  - Panic → ack → resume.
  - Shutdown order (stop producers → drain → stop render → destroy) with no use-after-free (ASan/Debug CRT).

### D14. SDK wrapper, language and licence
- **The C ABI 2 header** (`native/include/wirescape/module2.h`) is the binary contract. Any language that can export C can target it (C++, Rust, Zig, Faust-generated C++).
- **The C++ SDK** (`native/sdk/wirescape_sdk.h`, header-only, C++17) lets an author write a plain struct. See the sketch in section 4.
  - The wrapper generates the descriptor, validation (null, struct_size, ranges, NaN/Inf), parameter signals, broadcast/sum, event dispatch, state helpers and the exception barrier.
- **The DSP class has no dependency** on Wirescape types beyond `ws::dsp` value types: no Qt, no OS, no STL containers in process.
- **Adapters, all thin and later:** `wirescape` (now), `clap` (experiment, B6), `daisy` (experiment, B6).
- **Licence decision for the user:**
  - Recommended: `module2.h`, the SDK and the adapters under **MIT**, so module authors can choose their own licence. The app and current Essentials stay GPL-3.0.
  - Modules derived from GPL code (Surge, sst-basic-blocks, current CHAOS) stay GPL.
  - The VST3 SDK has been MIT since 3.8 (October 2025), and CLAP is MIT, so plugin adapters have no licence conflict.
- **C++ standard:** engine, SDK and DSP stay **C++17** (embedded toolchains). Host-only services may use C++20 (for example libremidi, Track C).

### D15. Numeric and performance policy
- Use **float32 by default.** Use double only for documented state (phase accumulators, low-cutoff/high-Q IIR state) with a comment.
- **Denormals:** the audio thread sets flush-to-zero/denormals-are-zero (MXCSR on x86, FPCR on ARM64; the Daisy adapter sets the Cortex-M7 FPSCR FZ bit). Per-sample denormal hacks like `vcf/module.cpp:27` can then go (only with an equivalence test).
- **No `-ffast-math` or `/fp:fast` globally.** It breaks `std::isfinite` and NaN guards. Allowed instead: `-fno-math-errno`, `-fno-trapping-math`, and per-file opt-ins with tests.
- **SIMD:** one SDK type `ws::simd::f4` / `f8` with a **scalar fallback**, because the Cortex-M7 has no float SIMD. The backend can be xsimd, Google Highway or SIMDe; decide, and keep it swappable for C++26 `std::simd` later.
  - Evidence: simple gain loops auto-vectorize fine. Multiply-accumulate filters gain roughly 3.5–14× from explicit SIMD versus about 1.1× from auto-vectorization (GCC 16 study).
- **Fast-math library** (`ws::fastmath`): `tan`, `exp2`, `tanh` and `sin` approximations, each with a documented maximum error and unit tests. Opt-in per call site. Never silently replace exact math in an existing module without an equivalence or listening note.
- **Builds:** Release builds with LTO. PGO is optional later. Profile with Tracy (BSD) or Superluminal. Benchmarks report the worst-case block time, not just the average.
- **Randomness:** an SDK deterministic PRNG with a **per-instance, per-channel seed saved in the patch**. Renders are reproducible, which the capture harness and offline tests need.

### D16. Host features that need no ABI change (plan now, build later)
- **Subpatches:** a saved patch shown as one module with a front panel (exposed parameters and ports). The host flattens it into the graph at prepare, so it has zero runtime cost.
- **Macros:** one knob drives a modulation source with an amount on many parameters (D7).
- **Snapshots/scenes:** saved base values for a subpatch.
- **Export bundle** (spec only, phase B6): patch + module sources + version hashes + `memory_requirements` + latency + target sample rate, block and channel count + a hardware control map. A later `wirescape export --target daisy` can generate a fixed-order C++ `main` from it.
- **Script modules** (future): see section 7.

## 3. CHAOS as the design check (future rebuild, by features)

Every CHAOS feature must be expressible with D1–D16. If one isn't, fix the contract, not CHAOS.

| CHAOS feature | Built from |
|---|---|
| MORPH, CHAOS, FOLD, MOTION macros | D16 macros → D7 modulation. No ownership rules |
| CLUSTER (1–5 notes per key), unison, DETUNE | A generic **Channel Expander** module: notes × unison → channels, with spread per channel (D2) |
| JITTER, DRIFT, ROUGH, FALL | Random/drift/slope modulation sources with per-channel seeds (D15), patched to pitch and AM |
| Generators A/B, SUB, FM | Poly oscillator modules (D2) with explicit pitch/FM ports (D7) |
| BODY resonator, COUPLE, FEEDBACK | Sample-exact feedback inside modules. Coupling between modules through D10 |
| TAPE, SPACE, LOOP | `LARGE` memory (D11), stereo pairs (D3), loop state (D12), latency (D12) |
| Internal LFO and mod matrix | Cables to parameter modulation (D7) |
| Scenes and presets | Snapshots (D16) and state (D12) |
| The CHAOS front panel | A subpatch with a panel (D16) |
| CHAOS on Daisy | The export bundle budget picks a "CHAOS-lite" subset |

## 4. ABI 2 sketch (starting point; Codex refines in B1)

```c
/* module2.h, sketch only. Every struct starts with struct_size for forward compatibility. */
#define WS_ABI2_VERSION 2u
enum ws2_port_kind { WS2_SIGNAL = 1, WS2_EVENT = 2 };
enum ws2_hint { WS2_ANY, WS2_AUDIO, WS2_PITCH, WS2_CV_BI, WS2_CV_UNI, WS2_GATE, WS2_TRIGGER,
                WS2_EV_NOTES, WS2_EV_MIDI, WS2_EV_MESSAGE };
enum ws2_poly { WS2_POLY = 0, WS2_SUM = 1, WS2_FIRST = 2 };
enum ws2_signal_flags { WS2_CONSTANT = 1u };

typedef struct ws2_signal     { const float *const *ch; uint32_t channels, flags; } ws2_signal;
typedef struct ws2_signal_out { float *const *ch; uint32_t channels, flags; uint32_t max_channels; } ws2_signal_out;

typedef struct ws2_port { uint32_t struct_size; const char *id, *name; uint32_t kind, hint, poly;
                          const char *stereo_pair; float unpatched_value; } ws2_port;
typedef struct ws2_param { uint32_t struct_size; const char *id, *name, *unit;
                           float min, max, def; uint32_t taper; float curve, step;
                           uint32_t flags, option_count; const char *const *options; } ws2_param;

typedef struct ws2_event { uint32_t size, type; uint32_t offset; uint16_t port, flags; } ws2_event; /* header */
typedef struct ws2_note  { ws2_event h; int32_t note_id; int16_t channel, key; float velocity; uint32_t source_id; } ws2_note;
typedef struct ws2_message { ws2_event h; uint32_t kind, bytes; const uint8_t *data; } ws2_message;
typedef struct ws2_event_list { uint32_t count; const ws2_event *const *items; } ws2_event_list;
typedef struct ws2_event_out  { void *ctx; int (*push)(void *ctx, const ws2_event *); } ws2_event_out; /* returns 0 if full */

typedef struct ws2_transport { uint32_t struct_size, flags; double bpm, beat, bar_start;
                               uint16_t ts_num, ts_den; double loop_start, loop_end; } ws2_transport;

typedef struct ws2_host { uint32_t struct_size;
    void *(*alloc)(void *ctx, size_t bytes, size_t align, uint32_t hint); /* prepare only */
    void *ctx; } ws2_host;

typedef struct ws2_process { uint32_t struct_size, frames; uint64_t frame_position; double sample_rate;
    const ws2_signal *inputs; ws2_signal_out *outputs; const ws2_signal *params;
    const ws2_event_list *event_inputs; const ws2_event_out *event_outputs;
    const ws2_transport *transport; /* may be NULL */ } ws2_process;

/* descriptor: id, name, version, ports (static or via query_ports(config)), params,
   create(host), destroy, query_ports, memory_requirements, prepare(config, host), reset,
   process(ws2_process*), save_state, load_state, latency_frames. */
```

SDK author view (target ergonomics):

```cpp
struct Vcf : ws::Module<Vcf> {
  static constexpr ws::Info info{"essentials.vcf", "VCF", 2};
  static constexpr ws::Port inputs[]  {{"audio", "Audio", ws::Audio, ws::Poly}};
  static constexpr ws::Port outputs[] {{"low12", "Low12", ws::Audio}, {"low24", "Low24", ws::Audio}};
  static constexpr ws::Param params[] {{"cutoff", "Cutoff", 20, 20000, 1000, "Hz", ws::Log, ws::Modulatable | ws::AudioRateOk},
                                       {"resonance", "Resonance", 0, 0.95f, 0}};
  void prepare(const ws::Setup &s);              // allocate via s.alloc(); sample rate, max channels
  void process(ws::Block &b);                    // b.in(0).ch(c)[i], b.param(0) (constant-aware), b.out(1)
  void reset();
};
WS_REGISTER_MODULE(Vcf)
```

## 5. Phases (Track B)

Each phase lists its deliverables, then the **acceptance** criteria.

**B1: spec and draft header (no behaviour change).**
- Deliverables:
  - `docs/specs/ABI2_SPEC.md` (normative wording for D1–D15).
  - Draft `module2.h`.
  - Draft `wirescape_sdk.h` with only the declarations.
  - One example module compiled against the draft.
- **Acceptance:**
  - The spec covers every D-item and its tests.
  - The header compiles as C and as C++17 under MSVC and GCC/Clang (the Linux build can be CI-only).
  - Section 9 decisions are recorded.

**B2: SDK and engine 2 core, with ABI 1 kept working.**
- Deliverables:
  - Engine accepts ABI 1 and ABI 2 modules. ABI 1 modules run through an internal shim (1 channel, no events out).
  - D1, D5, D6, D7 and D8 (input lists only) implemented.
  - Multi-channel output (D3).
  - Port VCO and VCA to ABI 2.
- **Acceptance:**
  - ABI 1 demo patches render **bit-identically** to the B0 baseline WAVs. Store reference WAVs and hashes under `native/tests/fixtures/abi2/`.
  - Ported VCO and VCA match ABI 1 output at max abs error ≤ 1e-6 when mono and unmodulated, or the difference is explained (for example, smoothing moved into the SDK).
  - Modulation tests pass.
  - The zero-allocation hook passes.
  - Benchmark: unmodulated patches are no slower than ABI 1 (±5%).

**B3: events out, ingress/egress, transport, feedback, memory, state.**
- Deliverables: D8 outputs and arena, D9, D10, D11, D12 and D13. The ingress registry and egress ring come with fake producers and a fake sink.
- **Acceptance:** every test listed under those D-items; the shutdown-order test under ASan (Clang) or the Debug CRT; a deterministic loop render.
- **Track C may start after B3.**

**B4: port the remaining 11 Essentials, and patch document v2.**
- Deliverables:
  - All Essentials on ABI 2.
  - Per-module CV via D7, plus explicit pitch/FM/gate ports where musical (VCO pitch input, ADSR gate input with a documented precedence between event and cable gates).
  - **Patch format v2:** string parameter IDs (from each `module.json`), modulation cables with amounts, event cables, stereo pairs, feedback setting, seeds, state blobs (base64), host mappings (Track C), devices referenced by name.
  - Lossless v1 → v2 migration on load. Save always writes v2. Unknown fields are preserved.
- **Acceptance:**
  - Every existing module test passes or has a documented, approved delta.
  - Each module is tested at 44.1/48/96 kHz, block 1/17/max, and channels 1/4/16.
  - `Six_Essentials_Demo.json` loads from v1, saves as v2, reloads, and renders identically.
  - Legacy fixtures in `native/tests/fixtures/legacy_documents/` behave as before.

**B5: UI for the new contract.**
- Deliverables:
  - Poly cable thickness and channel badge.
  - Per-channel scope peek.
  - Modulation cables drop onto knobs, with an amount handle on the cable.
  - Knob base plus a modulation ring.
  - Back-edge cable marker and delay.
  - Stereo pair cable with split.
  - Soft hint-mismatch colour.
- **Acceptance:** the UI smoke tests at both sizes are extended; a modulation drag-to-knob flow is covered by `ui_tests.cpp`; audio stays off.

**B6: portability proofs (small experiments, no product claims).**
- Deliverables:
  - (a) A `clap` adapter that builds the VCF and VCO as a CLAP plugin (clap-wrapper can produce VST3 later).
  - (b) A Daisy adapter that compiles VCO→VCF→VCA statically with the ARM toolchain (build only. Run on hardware only if the user provides it).
  - (c) The export bundle spec, `docs/EXPORT_BUNDLE.md`.
- **Acceptance:** the same module sources compile unchanged in all three. Offline reference vectors match between the Wirescape and CLAP adapters. A Daisy build size and memory report. **No "Daisy support" claim.**

## 6. Rules every new module must follow (this becomes the build guide core)

1. Use the SDK. Don't hand-write the C ABI.
2. No allocation, locks, I/O, exceptions or logging in process, event or reset. Memory comes from `Setup::alloc` at prepare.
3. Handle any frame count (1..max) and any channel count (1..max). Don't assume a block size.
4. Treat CONSTANT signals as the fast path. Don't recompute coefficients for unchanged inputs.
5. float32 by default. Document any double. No platform intrinsics outside `ws::simd`.
6. No globals or static mutable state (this keeps the door open for multi-core voices later).
7. Deterministic: seeded randomness only, and reset returns to a reproducible state.
8. Declare latency and memory requirements truthfully.
9. Provide tests: impulse, sine and sweep references, the sample-rate/block/channel matrix, reset determinism, state round-trip, an extreme-input guard, and a zero-allocation check.
10. Ship `module.json`, `README.md`, `DESIGN.md` (algorithm, limits, sources, licence) and `tests.cpp`, in the folder layout already used under `native/modules/`.

## 7. Future: script modules (not scheduled)

- **Python Script (control rate).**
  - Ports are generated from the function signature and type hints, for example `def process(rate: cv, gate: gate) -> cv`. This uses D12 config-dependent ports, like Grasshopper's Python component.
  - Runs in a **worker thread/process, never the audio thread**, at 100–1000 Hz or when events arrive.
  - Outputs are smoothed through ingress (D13). A slow or crashed script shows a counter, and audio is unaffected.
  - Use CPython 3.14: the free-threaded build helps when many scripts run, and the JIT gives little.
  - Patches containing scripts require a trust prompt.
- **Audio-rate quick ideas:** a **Faust** module (libfaust JIT or interpreter). The same Faust code can export C++ for Daisy (`faust2daisy`), plugins and VCV, so prototypes stay portable.
- **Graduation:** a script's signature becomes an SDK descriptor, and an AI converts the body to C++ using the build guide.
- Precedent: VCV Prototype (Lua/JavaScript, 6 inputs/outputs, `frameDivider` of 32 by default, buffering recommended for audio rate, a security warning for shared scripts).

## 8. Out of scope for Track B

MIDI, OSC, UDP and Monitor implementation (Track C), sequencers, subpatch UI, the CHAOS rebuild, multi-core rendering, marketplace and signing.

## 9. Decision log (implementers add rows during B1)

| Item | Decision | Reason | Date |
|---|---|---|---|
| SIMD backend (D15) | Own minimal `ws::simd::f4` with SSE2 (x64), NEON (ARM64) and scalar (Cortex-M7) paths. No external library yet | Zero dependencies; swappable later for xsimd, Highway or `std::simd` | 2026-10-26 |
| Default `feedback_block` (D10) | 32 frames, range 1..block, saved per patch | Low CPU, under 1 ms at 48 kHz | 2026-10-26 |
| Event budget and overflow policy replacing silence (D8) | Approved: 1024 per block, coalesce/drop with counters, note-off reserve | Live input robustness | 2026-10-26 |
| Licence split (D14) | Approved: `module2.h`, SDK and adapters MIT; app and Essentials GPL-3.0 | Module authors choose their own licence | 2026-10-26 |
| Max channels default (D2) | 16 desktop | VCV-compatible mental model | 2026-10-26 |
| Network bind default (Track C) | Approved: localhost by default, explicit "All interfaces (LAN)" toggle per module | Safe default; one firewall prompt when enabled | 2026-10-26 |
| First implementation step | `docs/tasks/M1-1_FOUNDATION_SLICE.md` comes first, then **stop for an external review** before B2–B6 port anything else | Review the base before it spreads | 2026-10-26 |
| M1-R 1: release emergency | Keep finite release reserve and counted fan-out; global reset/choke, with Activity counters in M4 | Bounded safety; add source-scoped emergency only after measurement | 2026-10-27 |
| M1-R 2: timestamps | Prepared max_block lookahead and bounded ±1000 ppm EWMA; no sample-accurate claim before real MIDI measurements | Fake anchors prove scheduling, not device timing | 2026-10-27 |
| M1-R 3: PER_VOICE | Global parameters stay mono, poly modulation uses channel 0; adapters resolve note_id to channel | Avoid splitting global controls per voice | 2026-10-27 |
| M1-R 4: optimisation order | Cache scheduling/views, remove copies, validate producers once, then row helpers and time-vector VCA; voice-SIMD deferred | Remove framework overhead before changing DSP strategy | 2026-10-27 |
| M1-R 5: latency (M2) | Count output-reaching signal paths per bus; show feedback delay only on back-edge markers | Separate forward latency from recirculation | 2026-10-27 |
| M1-R 6: adversarial gates | Require panic/egress TSan stress, mixed SCC permutations and callback-boundary event tests before Track C | Exercise scheduling and generation boundaries | 2026-10-27 |
| M1-R 7: caps | Keep 1472-byte payload, 1 MiB state, 128 MiB/allocation and 64 allocations/node; exporters use memory_requirements; revisit CHAOS tape in M7 | Explicit DSP budgets separate from desktop queues | 2026-10-27 |
| M1-R 8: queries | Allow query before cabling on created/unprepared instances; queries cheap and side-effect-free; create never allocates LARGE memory | Configuration-dependent ports without expensive construction | 2026-10-27 |
| M1-R 9: M4 entry | Fake-sink dispatch, ±1 ms jitter/±500 ppm drift, producer shutdown and hardware-free libremidi loopback precede authorised hardware | Prove ownership and timing before devices | 2026-10-27 |
| M1-R 10: ARM and adapters | GCC 13 aarch64 build/tests proven; Pi 5 timing via optional runner in M2; CLAP first in M8; scalar Cortex-M7 Daisy build-only in M8 | Keep measured platform proof distinct from future portability | 2026-10-27 |
| M1-R 11: memory ownership | Host frees allocations after destroy; rate/block change creates and prepares a new instance then loads state; future free callback trails ws2_host | Keep ownership explicit without changing the ABI | 2026-10-27 |
| M1-R 12: finite validation | Hosts/adapters deliver finite inputs and parameters; SDK validates producer outputs; engine2 validates ABI 1 shim outputs | Single-point checks preserve finite guards without redundant scans | 2026-10-27 |

## Sources

- VCV Rack polyphony and voltage standards: https://vcvrack.com/manual/Polyphony · https://vcvrack.com/manual/VoltageStandards
- CLAP (MIT; per-note/polyphonic modulation, event lists, thread pool): https://u-he.com/fwd/clap/
- VST 3.8 SDK under MIT (October 2025): https://sonicstate.com/news/2025/10/30/vst-3-now-available-under-mit-license · Steinberg press release https://ocl-steinberg-live.steinberg.net/_storage/asset/819253/storage/master/Press%20Release%20-%202025-10-29%20-%20VST%203.8%20-%20EN.pdf
- sst-basic-blocks (mostly GPL-3.0; elliptic BLEP, Lanczos, Hilbert headers MIT): https://github.com/surge-synthesizer/sst-basic-blocks
- GCC 16 `std::simd` vs auto-vectorization for audio DSP: https://dev.classmethod.jp/articles/gcc-16-cpp26-std-simd-audio-dsp/
- Python 3.14 performance (about 27% over 3.13; JIT made little difference; free-threading 3.09× with 4 threads): https://simonwillison.net/2025/Oct/8/python-314-is-here-how-fast-is-it/
- VCV Prototype scripting module: https://vcvrack.com/Prototype · Faust with VCV: https://faustdoc.grame.fr/workshops/2020-11-21-faust-vcvrack/
- Daisy memory and audio callback references (already in `docs/specs/EMBEDDED_PORTABILITY.md`): https://daisy.audio/pages/memory-what-is-the-difference · https://github.com/electro-smith/libDaisy/blob/master/doc/md/_a3_Getting-Started-Audio.md
- Further reading for high-quality, embedded-friendly DSP structure: Mutable Instruments stmlib/Plaits/Clouds (MIT, STM32, block-based) and DaisySP (MIT). Verify licences and pin revisions before copying any code.
