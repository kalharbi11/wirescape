<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Ladder Filter (ABI 2)
The four private double poles, previous-stage feedback, exact tan/tanh calls, double parameter ramps, linear drive=0 branch, finite-input handling and explicit 1e-20 tail cut are copied in the original operation order. No voice SIMD or coefficient approximation is introduced.
Run `abi2_ladder_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
