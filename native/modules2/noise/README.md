<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Noise (ABI 2)
Xorshift32 advances even while muted. Channel 0 uses 0x12345678 exactly; channel c uses 0x12345678 XOR (c * 0x9e3779b9), unsigned wrap, replacing zero with one. No engine seed is read. Upper 24 bits and double 0.2 default/ramp math match ABI1.
Run `abi2_noise_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
