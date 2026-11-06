// SPDX-License-Identifier: GPL-3.0-only
/** Recent two-cycle LFO trace, with the newest phase sample at the right edge. */
const CYCLES = 2;

export function lfoTraceValue(phase: number, u: number, depth: number, offset: number): number {
  const amp = Math.max(-1, Math.min(1, depth));
  return offset + amp * Math.sin((phase - (1 - u) * CYCLES) * Math.PI * 2);
}
