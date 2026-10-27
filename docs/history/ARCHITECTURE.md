# WIRESCAPE 0.2 — architecture

## One graph, independently hosted modules

```text
 Browser editor (web/index.html + web/client.js)
     │     patch JSON, control edits, notes and status (local WebSocket only)
     ▼
 aiohttp host (wirescape/server.py)
     │     validate, instantiate modules, queue plan/control changes
     ▼
 Typed graph executor (wirescape/schema.py + wirescape/graph.py)
     │  generator, modulation, and effect blocks (one callback at a time)
     ├── MODULE CHAOS #1 [dual Surge XT synth engines; polyphonic]
     ├── MODULE CHAOS #2 [independent dual Surge engines, if instantiated]
     ├── MODULE MODULATOR [control-rate signals]
     ├── MODULE GAIN / SHAPE [typed stereo utility effect]
     └── MODULE AUDIO I/O [sums all patched stereo destinations]
     │
     ▼
 PortAudio / sounddevice (ONE stereo output stream; optional duplex input)
     ▼
 Physical audio interface / loudspeakers
```

The graph topology is data. Modules are instances. A cable is a typed edge `(output node:port) → (input node:port)`. **The canvas layout is not the audio graph**: moving a node changes only coordinates; connecting a cable changes the graph topology.

## Thread boundaries

- Browser main thread: DOM, knobs, patch serialization, selection, module inspector.
- Async Python host: validates typed patch, creates module objects and snapshots the plan.
- PortAudio callback: swaps prepared plan and consumes queued controls at a block boundary, executes the topological plan, sums outputs into two final float32 buffers.
- The audio callback **never calls a WebSocket** or updates the DOM. Host does not ask a browser to produce samples.
- Existing CHAOS `Engine.render()` is owned by the callback and owns its two independent Surge instances. The active graph's render plan is reused when only positions, knobs or simple cable shape change.

A fast prototype needs this separation to avoid browser rendering causing audio glitches. **The callback still runs Python**, which is not hard realtime. The long-term production host should expose a native Rust/C++ audio graph while preserving schema/ports and the browser UI.

## Deterministic routing rules

Signal contracts in beta:

| Domain | Value passed per callback | Mixing | Frequency |
|---|---|---|---|
| `audio` | `np.ndarray`, mono, `float32`, length `frames` | Sum incoming signals | Audio rate |
| `mod` | `float` in `[-1,1]` | Sum + clip to ±1 | Block rate |
| `gate` | `bool` | Logical OR | Block rate |
| `clock` | `bool` | Logical OR; `gate → clock` accepted | Block rate |

Stereo is **two explicit mono ports**, not an ambiguous audio blob. CHAOS's `audio` external input is a mono CV-like stereo-duplicated channel for now; its native engine accepts a `(2,frames)` buffer. Stereo input into effects uses `inL`/`inR` separately.

Every connection is validated before an execution plan is installed. Connections with incompatible port types, missing modules, duplicate identical cables, self-recursion or a directed cycle are rejected. Multi-source mixing at one input is supported, so many polyphonic generators can feed a single Audio I/O module or future mixer. Controlled feedback will require an explicit non-zero-delay feedback node and an architecture capable of processing strongly connected components; cycles must not silently execute via recursion.

**Audio I/O**: hardware input becomes `extL/extR` only when a 2×2 duplex device is enabled. Multiple `interface` destination modules sum into the same PortAudio stream. Each output interface smoothly scales its stereo inputs; a final `tanh` guard keeps floats within ±1. Inter-module mixing is currently unity sum; users should attenuate high-energy buses before the final interface to retain headroom.

**MIDI/polyphony**: keyboard presses generate MIDI-like note events addressed to a selected CHAOS node, not a browser tone generator. Each CHAOS instance is independently polyphonic via two Surge XT engines, with the original legacy polyphony limit (up to 32 configured). The host itself currently does not accept hardware MIDI devices; external MIDI and voice allocation policies are future work.

## Patch persistence

`format: "wirescape.patch", version: 1`. A saved module has `id`, `type`, `x`,`y`, `values`, `mode`, `advanced` overrides and `raw` overrides. Connections use typed stable names such as `node-2:left` and `node-3:inL`. Node identity is stable after moving and cable changes, which allows the audio graph to **reuse the same synth engine** rather than clearing held notes on each edit. Unknown future module types can be kept in a deferred browser patch until their installed host definition arrives.

The original HTML proof-of-concept had localStorage with `wirescape.visual.study`, `version:4`; the host accepts its compatible node/connection representation and upgrades exported patches to the v1 Wirescape shape.

## Existing limitations

- Only audio/mod/gate/clock domains have runtime processors. Data/image/video **need additional typed scheduler domains and display modules**.
- Many frontend module descriptors are generated from the host, but CHAOS, Modulator and Audio I/O still have specialized views in the imported design. New simple DSP modules get a generic knob/port face. More complex plugins can gain custom views later without changing the host protocol.
- Mono/stereo only at host boundary. Surround/multichannel/polyphonic CV ports require negotiated channel layouts.
- JS UI graph is not authoritative: the host re-validates every submitted patch and rejects invalid cycles and types.
- No DAW plugin hosting, external MIDI input, JACK/PipeWire transport sync, collaborative patching, hot-plug recovery or sample-accurate automation in this beta.
- No background filesystem scanning, arbitrary script loading from patch documents or unsandboxed code execution via imported JSON.

## Source organization

- `wirescape/schema.py`: authoritative module/port manifest and validation.
- `wirescape/plugins.py`: explicit processor registration and reference GAIN plugin.
- `wirescape/modulation.py`: deterministic control generators.
- `wirescape/mapping.py`: curated six-control CHAOS front-panel mapping.
- `wirescape/graph.py`: compiled execution plan, queues, audio routing, controls.
- `wirescape/audio.py`: hardware stream lifecycle, optional stereo input, meters.
- `wirescape/server.py`: local-only WebSocket/HTTP endpoint and declarative UI schema.
- `web/index.html`: the supplied Studio source layout, adapted with host controls and deep editor.
- `web/client.js`: browser-to-host commands, device panel, keyboard, inspector, plugin UI registration.
- `modules/chaos/`: standalone original CHAOS DSP and all original macro/oscillator raw controls, with minimal fixes for persistent raw overrides and more efficient revoicing.
- `tests/`, `tools/verify.py`, `examples/`: no-hardware automated verification and example patches.

## API v1 folder-module host (Wirescape 0.5)

The Wirescape 0.5 extension boundary is `wirescape/extensions.py` + `wirescape/plugins.py`. Discovery reads `modules/<id>/module.json` (data only). User-opted-in modules load `plugin.py:register()` while the audio service is stopped. Module definitions generate the module browser, generic front panel and typed graph ports; module-owned `editor.js`/`editor.css` may provide a specialized editor without host HTML edits. A plugin processor runs in the existing single PortAudio callback as `process(n, controls, typed_inputs)`. Optional validated `module_state` is queued and applied in the callback via `configure()`.

`notes` is now a typed polyphonic event port with source ownership and a CHAOS `NOTES IN` destination. The block engine cannot make sample-accurate MIDI timing guarantees. The plugin loader deliberately does not hot-replace imported Python code or install remote updates; a restart is required after changing the module folder of an already loaded plugin. User activation of arbitrary Python is a trust boundary, not sandboxed code. See `docs/MODULE_API.md` and the separate `module_build_guide/` kit.
