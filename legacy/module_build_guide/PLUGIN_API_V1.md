# Plugin contract — Wirescape 0.5 / API v1

## Discovery and explicit trust

The host reads `modules/<id>/module.json`. Folder name and manifest `id` must match and be lowercase identifiers matching `[a-z][a-z0-9_]{1,48}`. `api_version` must equal `1`. Only `entry: "plugin:register"` is currently supported. A JSON patch **never loads Python code**; modules are enabled explicitly and then recorded in `user_data/enabled_modules.json`. A Python plugin has the same filesystem permissions as Wirescape; only install trusted code.

## Required registration

```python
from wirescape.schema import Port
from wirescape.plugins import register_audio_module

def register():
    register_audio_module('example_saturator', {
        'title': 'SATURATOR', 'desc': 'Stereo saturation',
        'kind': 'PROCESSOR', 'code': 'P / 100',
        'w': 520, 'h': 370,
        'ins': [Port('inL', 'in', 'audio'), Port('inR', 'in', 'audio')],
        'outs': [Port('outL', 'out', 'audio'), Port('outR', 'out', 'audio')],
        'controls': {'LEVEL': 50, 'DRIVE': 20, 'MIX': 100},
        'modes': [],
    }, SaturatorProcessor)
```

`register_audio_module(id, definition, processor_class, *, default_state=None, validate_state=None)` adds a processor type. `register()` must register its own manifest ID, exactly once. A processor is instantiated **per module instance** as `processor_class(sample_rate)`. Each audio callback invokes `process(frames, controls, inputs)` and expects a `dict` with output values keyed by declared output ports. The returned audio arrays must be mono `numpy.float32` with shape `(frames,)`.

**Controls:** exposed standard knobs have values `0..100`, stored and normalized by the host. Use `modes: ["A", "B"]` for discrete modes. Do not treat an enum as a continuous knob. Mode selection is available as an index on processor instances that define a writable `.mode` attribute. To react to parameter changes, read the `controls` dict each call; do not reach into the browser or host directly.

### Signal types today

| Kind | Input to process | Output from process | Notes |
|---|---|---|---|
| `audio` | mono float32 NumPy array length N | mono float32 NumPy array length N | Stereo uses L/R ports. Missing input is silence; upstream audio signals sum. |
| `mod` | float clamped to -1..+1 | finite float clamped to -1..+1 | One control value per block. Multiple sources sum then clamp. |
| `gate` | boolean | boolean | Trigger state at block rate; inputs OR together. |
| `clock` | boolean | boolean | Gate output can patch to clock input. Detect rising edges yourself. |
| `notes` | list of `(source_module_id, (pitch, is_on))` events | list of `(pitch, is_on)` | MIDI pitch 0..127; on/off are strict booleans, maximum 128 output events per block. All events currently scheduled at a block boundary. No velocity/MPE/sample offsets. |

A patch is a **directed acyclic graph**. Feedback is rejected unless a future explicit feedback-delay mechanism is added. All plugin code executes in the Python realtime callback—**do not allocate large buffers, access files/network, block, spawn threads, print continuously, or import large libraries per callback**. Keep processing bounded and write a CPU benchmark. Python does not provide hard realtime guarantees.

### Stateful sequencers, sequencers and complex editors

For complex JSON state, provide:

```python
register_audio_module(
    'sequencer', definition, SequencerProcessor,
    default_state=default_pattern,        # callable returns JSON-compatible data
    validate_state=validate_pattern       # callable validates/canonicalizes it
)
```

State resides in each node as `module_state` in the Wirescape patch. On a patch load or `module_state` message, the host calls `processor.configure(validated_state)` on the audio thread. The data validator runs before the callback receives state. **Do not execute or import from state content**. Cap payload size and complexity; reject NaN/Infinity, unexpected fields, oversized arrays, arbitrary file paths and names. The sequencer demonstrates a 16-step validated state.

An editor updates `module_state` and calls the provided `changed()` callback. State is stored in exported patches and restored by the host. **API v1 has no per-module custom WebSocket message types**; use the generic state channel and standard controls.

### Timing, polyphony and portability

Current host reference: 48 kHz, 512-sample blocks (~10.67 ms). A sequencer can schedule sub-block times internally but downstream events are currently dispatched at block boundaries. Polyphonic NOTES can address CHAOS; all voices share one stereo device output. The host uses Python/NumPy, not hard realtime C++; Raspberry Pi 5 (ARM64) and Windows 3.12 binary compatibility must be considered. Surge XT is a CHAOS dependency and not required for ordinary processor modules.

### Failure contract

The host validates output shapes, missing kinds and note payloads. Plugin exceptions during `process()` can interrupt audio. Fail safely inside the processor, test disconnections and malformed state, and keep all output finite and bounded. Version your `module_state` format if it may evolve; existing saved patches must continue to load or have an explicit migration.
