<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Output (ABI 2)
Transparent per-voice audio output; a missing input is zero. It opens no device. Engine2 output(node, port, bus_channel) mixes the voice channels into the chosen bus once; this module does not pre-sum voices or introduce a second bus reduction.
Run `abi2_output_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
