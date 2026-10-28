<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Scale/Offset (ABI 2)
Output is input times the double scale ramp plus the double offset ramp, preserving operation order. Missing input still outputs the smoothed offset; reset snaps both ramps to their user targets.
Run `abi2_scale_offset_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
