<!-- SPDX-License-Identifier: GPL-3.0-only -->
# fx.reverb: design and provenance

**Original code**, GPL-3.0-only. No third-party source code or Clouds/Mutable Instruments code is copied.

The network uses the classic lossless feedback-delay-network construction and Schroeder input allpasses. Textbook references:
- Jot and Chaigne, *Digital Delay Networks for Designing Artificial Reverberators*, AES 1991: lossless feedback mixing, line-dependent damping and time constants.
- Stautner and Puckette, *Designing Multi-Channel Reverberators*, Computer Music Journal 6(1), 1982: multichannel delay-network mixing.
- Schroeder, *Natural Sounding Artificial Reverberation*, Journal of the AES 10(3), 1962: allpass diffusion.

## Topology and decisions

Eight mutually distinct prime lengths (48 kHz bases: 1499, 1741, 1999, 2281, 2617, 3011, 3469, 4789 samples) are recalculated for the rate and `size` every 64 absolute samples. A 50 ms one-pole glide plus fractional linear taps avoids discontinuous jumps. At size 1 the longest line is ~100 ms. The orthonormal 8×8 normalized Hadamard butterfly preserves energy when frozen. The normal-feedback coefficient per line is `exp(-ln(1000) × line_samples / (rate × RT60))`, so *amplitude* falls by 60 dB over RT60 irrespective of size. One-pole feedback damping is removed in freeze.

Two allpass sections per input side and separate up-to-250 ms predelays precede alternate-line excitation. Input is attenuated as freeze approaches, the feedback gain approaches 1 over 20 ms, and motion is likewise suspended. Two fixed-phase, slow sine LFOs modulate the fractional taps deterministically. The two wet projections are orthogonal before optional mid/side width collapse. Wet/dry gains use cosine/sine equal power; at Mix=0 output bypasses the math to preserve exact dry bits. A conservative ±8 internal safety ceiling and ±2 final output ceiling are present only for pathological signal levels.

Line, predelay and allpass memory uses `WS2_LARGE` allocated in `prepare`; no state allocation, I/O, locks, exceptions, or random seed in the callback. State only contains a version word: loading it resets the delay buffers intentionally, rather than promising audio-tail continuation.

**Listening note:** No physical-audio listening was performed by the builder. Review room-to-hall colour, stereo width at fully wet, and freeze on/off transitions after a sustained chord on the owner device.
