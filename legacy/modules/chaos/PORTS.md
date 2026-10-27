# CHAOS module boundary: v0.1

This file defines a **future integration contract**, not a finished rack or patch UI.

## Inputs (audio-thread only)

- `Engine.render(n=512, input_block=None)` takes optional **stereo float32** array of shape `(2,n)`.
- `EXT_MIX` controls addition of this input to the combined generator bus **before the resonant body**.
- There is no dependency on GUTTR, GUTTUR, Node.js, vocal parameters, or the old MERGE processor.
- Both audio engines must agree on sample rate and callback framing; resampling and clock sync are not implemented.

## Outputs (audio-thread only)

- `Engine.render` returns output stereo float32, after looper and safety output processing.
- `Engine.last_stems` contains `(2,n)` arrays `A`, `B`, and `CHAOS` (pre-looper/master, for inspection or future internal routing).
- These arrays are owned by the audio engine and must not be read concurrently without synchronization.

## Control plane

- `Engine.set(name,value)`, `set_many(mapping)`, `note_on`, `note_off`, `panic` enqueue commands.
- `macros.SPEC` is the documented parameter schema (48 parameters in this version, including modulation slots); each key is bounded.
- `modlab.ModulationMatrix` is a four-route control-rate modulation system. Four independently selectable sources target existing macro names; targets can sum and clamp.
- The existing Surge-side modulation paths are also available through A/B raw panel parameters. The host matrix does not replace native Surge routing.
- `ROUTE_MODE`: 0 parallel generator layers; 1 ring/AM audio interaction; 2 modulated-delay interaction.
- This is **not audio-rate inter-oscillator phase/FM inside Surge**. It is explicit cross-bus processing, and the distinction matters.

## Future algorithms

An algorithm definition should eventually name: generators, intermediate buses, connections, per-edge depth, processors, macro mappings, and state migration/crossfade rules. Save a separate algorithm ID and parameter snapshot; do **not** conflate algorithms with preset values.

Candidate architectures after auditioning CHAOS independently: coupled oscillator cluster, filterbank/comb swarm, frequency-shifted feedback, modulated diffuser, low-end sub/inharmonic cluster. Implement them incrementally and benchmark each on Windows and Pi 5 before designing the permanent UI.

## Routing constraints

Feedback cap in ResonantBody is 0.85; output is bounded by a -1 dBFS ceiling. Audio loops through any external input are not internally created. Live parameter changes to topology/delay lengths are currently block-stepped, and may click. Transport, external CV, audio-to-control patching across modules, and shared modulation buses are future work.
