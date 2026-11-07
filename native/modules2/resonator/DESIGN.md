<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Resonator design and provenance

Original Wirescape code, GPL-3.0-only. The fractional-delay comb topology,
saturator `x/(1+abs(x))`, and per-tap one-pole damping were copied and adapted
from our own `native/modules2/chaos_lite/comb_body.h`. The original CHAOS-lite
directory is untouched. No Surge, SST, third-party code or new dependencies.

Each tap's feedback coefficient is `exp(ln(0.001) * tap_period / (T60 * rate))`;
unlike one shared gain across combs, this gives every tap approximately the
same amplitude T60 in the small-signal, damping=0 regime. The rational soft
clip is kept **inside** each loop to bound hot excitation. The internal noise
burst is deliberately low-amplitude so that loop saturation doesn't dominate
the user-chosen T60; wet output makeup gain restores playable level. Weighted
tap output (0.6, 0.25, 0.15) keeps the tuned fundamental prominent. The
deterministic one-pole-filtered burst has a five-millisecond linear envelope.

Fractional delay is linearly interpolated and slewed with a 12 ms one-pole.
The three structures interpolate delay targets, avoiding hard retunes.
Each voice owns three independent histories, RNG, trigger Schmitt state and
DC-block state. Comb storage is requested once in `prepare` via WS2_LARGE,
sized for 20 Hz at 96 kHz; eight-voice memory is below 0.5 MiB. Reset and
load discard DSP history. A three-pole radiation lowpass tracks 0.8 × fundamental pitch to make the
fundamental dominant in Harmonic mode even with damping at zero. It acts only
after the feedback loops, preserving the per-tap T60 law. The full output uses
a DC blocker (pole 0.995) and
a ±2 safety clamp; no CONSTANT flag is emitted for active comb processing.

Background: textbook digital waveguide/feedback comb, fractional-delay linear
interpolation, and first-order DC high-pass difference equation. Independent
spectral, decay, frame/rate and poly tests exercise the ABI2 callback boundary.
