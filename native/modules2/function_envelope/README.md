<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Function Envelope (ABI 2)
Double held/start/end levels, edge-latched curve and exact expm1 shaping are retained. Gate changes start a rounded sample-count segment from the current level, including falling from a held nonzero level. No note-event or transport semantics are added.
Run `abi2_function_envelope_tests` for exact references, the rate/block/channel matrix, state and allocation checks.
