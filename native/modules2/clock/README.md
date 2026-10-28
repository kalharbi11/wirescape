<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Clock (ABI 2)
Free-running double phase with the ABI1 epsilon at duty boundaries. BPM retains the 5 ms ramp; tick lengths round with floor(value + 0.5), run thresholds at 0.5, and reset changes phase on a rising >=0.5 edge only. No transport sync is added.
Run `abi2_clock_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
