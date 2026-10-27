# CHAOS: sound-first architectural decision record

**Version 0.1 — October 2026**

## Decision: what constitutes the first module?

Keep two separate Surge XT synth generators rather than treating the entire earlier GUTTR as one large voice. Keep raw synthesis controls, the paper's macro parameter mappings, native Surge modulation, varispeed looper, standalone effects, and randomization. Deliberately remove every compulsory dependence on the vocal model. CHAOS must be independently playable with B muted and every experimental effect bypassed.

## Decision: prototype routing before final UI and algorithm selection

The code separates **generators**, **generator interaction**, **resonant processor**, **modulation**, **time/space processing**, and **output**. The first two generators are complete Surge synths, not minimal test oscillators. New graph-like connections live in `chaos_dsp.py`; a narrow scalar macro API lives in `macros.py`. This creates seams where future algorithms can change connectivity without rewriting the instrument's UI.

Three **prototype routing modes** are intentionally conservative:

1. **Parallel**: both Surge generators sum to one stereo bus, with independent levels/mutes/pans. This is the baseline, clean usable synth.
2. **Ring/AM bus interaction**: B acts as the bipolar amplitude multiplier of A, mixed with a B component; dry/wet returns continuously to the parallel bus. This is **post-oscillator** processing, not inside-Surge FM.
3. **Delay bend**: B changes the delay time of A with a smoothed audio signal, with a 3.5-ms center and ~2.5-ms swing. Delay reads are strictly into the past, preserving callback safety.

The **resonant body** adds three short feedback combs with damping and saturation. The feedback is capped, and a DC blocker/limiter protects the output. The main system remains usable with every processor at zero mix.

## Decision: two modulation systems with different strengths

- Keep **Surge's native** two LFO controls + five raw destinations per generator for internal timbral modulation, which is superior to synthesizing fake audio-rate modulation in Python.
- Keep the existing self-modulating host LFO and its legacy direct target, plus a **4-slot additive matrix** for macro-level interactions. Sources include LFO, S&H, smooth random, and previous-output envelope; bipolar depths are scaled to destination bounds. Source processing remains in the audio engine, not UI callbacks.
- Do not claim that 48 kHz CV patching or arbitrary feedback between Surge internal oscillators is supported by this matrix. It isn't.

## Decision: external module interface must be small and explicit

A future vocal engine sends an audio block to `Engine.render(..., input_block=...)` at a matching sample rate. `EXT_MIX` determines whether it enters the bus before the resonator. A/B/CHAOS pre-master stems are available to the audio caller only. Cross-oscillator phase modulation *across engines* is not available without embedding a new generator. A common modular audio backend/graph scheduler is needed before genuine cross-engine feedback.

## Algorithms vs presets

- **Algorithm**: signal graph, generator roles, processor choice/order, parameter interpretation, and feedback topology.
- **Preset**: parameter values, raw synth parameters, modulation mappings, and selected algorithm identifier.
- **Macro**: an expressive control mapping across several DSP parameters.

This version does **not** ship final algorithm modes. Routing modes provide small, testable variations within the first architecture. Maintain preset backward compatibility by preserving default values and ignoring unknown legacy fields.

## Future candidate algorithm directions (not implemented)

A. **Coupled cluster** — native/custom oscillator phase or frequency interaction; requires new DSP oscillator core or support from Surge, not just bus FX.

B. **Comb constellation** — dynamically tuned resonator swarms, feedback injection, saturation and frequency tracking; starts by replacing fixed body delays with smooth parameterized delay lengths.

C. **Shifting feedback** — SSB frequency shifter inside bounded stereo delay feedback (not ordinary pitch shift). Test aliasing and complex feedback gain stability.

D. **Dispersive space** — allpass diffusors and modulated delay lines; place the loop VCA before or after the processor per algorithm.

E. **Sample-loop oscillator** — derive an oscillator waveform from looper content, with deterministic capture and phase interpolation; inspired by Oneiroi's buffer-wavetable linkage.

F. **Vocal-coupled CHAOS** — only after independent vocal module is stable: mouth-shape control envelope and formant tracking as routed sources, with strict channel/transport/frame agreement.

**Next audition checkpoint**: build/run real Surge on Windows, record 5 full-engine clips, compare timbral variety and coherence, profile CPU at 48 kHz / 512 frames, test moderate/high matrix activity. Then pick which architecture genuinely merits an algorithm slot. **Only after this** design the permanent panel around the modes that survive.

## References

- [Forge TME, VHIKK X](https://forge-tme.com/vhikk-x/), [Firmware 0006 algorithm reference](https://forge-tme.com/vhikk-x/algos_06.html) — generator/filter/delay-processor structure, per-algorithm modulation mappings, parameter and topology morphing.
- [Befaco, Oneiroi](https://www.befaco.org/oneiroi/), [official code](https://github.com/Befaco/Oneiroi) — oscillator/looper/effects pipeline, self modulation, randomizer and buffer-derived wavetable; architectural inspiration only.
- *How Music Learned to Be Scary* study guide, supplied with GUTTR — preserved Surge macro mappings for nonlinearity, roughness, unstable tuning and tape decay. Not included in this repository.
