<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Slew (ABI 2)
The held level and default 0.1-second rates remain double. Rise/fall changes alter the step limit immediately, with no extra amplitude ramp. Missing input slews toward zero; nonfinite targets hold state. Zero time jumps directly to the target.
Run `abi2_slew_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
