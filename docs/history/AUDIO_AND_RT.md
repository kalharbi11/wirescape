# Audio I/O and realtime constraints

## Implemented audio topology

- One stereo device stream per Wirescape host (`sounddevice.OutputStream` or optional `sounddevice.Stream` with stereo input and output).
- 48,000 samples/sec and 512-frame blocks (10.67 milliseconds of audio per callback).
- Every CHAOS module hosts two Surge XT engines with independent state. Any number of CHAOS instances allowed by the graph can feed a common Audio I/O node by connecting their corresponding L and R ports.
- Audio cables carry one-channel float32 blocks. Inter-module multi-input summing is performed by the graph executor; the final device output is two float32 channels, not one stream per module.
- Optional hardware input channels are exposed by the Audio I/O plugin as `extL`/`extR` outputs; no permission is requested or device opened until the user presses START AUDIO.
- The output uses smoothed gain and a bounded `tanh` safety transfer, not a lookahead mastering limiter. Levels above ~0 dBFS are distorted/compressed; reduce individual module gains for clean sums.
- A browser does **not** render/process audio. Audio callbacks are scheduled by PortAudio, controlled by a thread-safe command queue, and publish lightweight peak/RMS/CPU status snapshots for the editor.

## Voice count and processing overhead

Within one CHAOS instance, polyphony is controlled by Surge. The original CHAOS initialization specifies `Polyphony Limit=32`, but this is not a promise that your Raspberry Pi will process 32 simultaneous notes. Some unison modes can multiply oscillator work. Two CHAOS instances mean **four independent Surge engines**. With more voices/effects, callbacks can exceed the ~10.67ms real-time deadline and cause crackles.

A real load test should inspect `p95_ms` and `realtime_headroom` in `build_reports/verify.json` after `python tools/verify.py --real`. Target much more than 1× headroom (ideally 2–3× under expected polyphonic load). Some callbacks may be slower than the p95 statistic, so also track device-reported xruns during physical playback. Change block size only alongside the host render-planning logic and Surge's 32-sample block requirement.

## Current technical risks

- Python is not hard real-time; GIL scheduling, allocation, garbage collection, SciPy routines and the original CHAOS per-block param setters can stall the callback.
- Creating a NEW CHAOS instance or updating a large graph creates Surge objects on the host control thread, but Python execution/GIL pressure can still glitch the audio device. Avoid live topology edits during critical recording until tested.
- The embedded Modulator is control-rate (one sample of modulation per block). It cannot drive oscillator-frequency phase modulation without a separate audio-rate control processor; do not describe it as audio-rate FM.
- Built-in Wirescape Audio I/O is a virtual graph destination; it cannot guarantee low-latency hardware performance independently of your driver, CPU, or interface.
- The audio device must accept 48 kHz float stereo; if not, opening the stream fails visibly rather than quietly resampling. Some interfaces require exclusive or ASIO configuration.
- Duplex mode uses the default OS input paired with the selected output; they need a stable shared clock. For two unrelated devices, use a host system such as JACK/PipeWire or implement asynchronous resampling in a later version.
- No automatic device hot-swap recovery or channel-layout negotiation yet.
- Server controls are dispatched on a local WebSocket. Their timestamps are not sample accurate. Musical events are consumed at the next 512-frame callback boundary; MIDI latency/precision will need improvement.

## Production architecture recommended for future Pi 5

1. Profile real Surge with a 4/8/16-note chord under simultaneous modulation, before adding more nodes.
2. Add a configurable block size and explicit driver negotiation. Support JACK/PipeWire/ALSA on Pi 5, and WASAPI/ASIO on Windows.
3. Replace dynamic allocations and blocking locks on audio callback hot paths; use preallocated buffers and lock-free control messages in a native Rust/C++ DSP graph. Retain the JSON patch schema and browser design independently of processing engine.
4. Implement deterministic modulation update-rate contracts and per-edge gain/attenuverters.
5. Make physical MIDI clock/notes and hardware audio an explicit host-level interface, not ad hoc logic in individual modules.
6. Introduce deterministic latency reporting, dry/wet latency compensation, module tail handling, safe graph swap/crossfade, and explicit delay-based feedback scheduling.
7. Add repeatable sustained stress tests on Raspberry Pi 5 and Windows, including hot-plug behavior and 20-minute thermal loads.

Passing current Python mock tests proves API correctness and basic routing, not a production-grade realtime guarantee.
