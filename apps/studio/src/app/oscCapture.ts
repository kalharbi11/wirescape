// SPDX-License-Identifier: GPL-3.0-only
import type { PatchDoc } from '../patch/PatchDoc';
import type { Telemetry } from '../engine/EngineClient';

/** UI counterpart of native/io/osc/pattern.h: bounded byte-level matching, no regex backtracking. */
type Token = { kind: 'literal' | 'any' | 'star' | 'class' | 'alt';
  byte?: number; mask?: Uint8Array; options?: Token[][] };
const patterns = new Map<string, Token[] | null>();
/** Address matching is independent of live values; cache bounded recent pairs. */
const results = new Map<string, boolean>();
const utf8 = new TextEncoder();

function compile(pattern: string): Token[] | null {
  const bytes = utf8.encode(pattern);
  if (bytes.length > 255) return null;
  let i = 0;
  const parts = (depth: number): Token[][] => {
    const choices: Token[][] = [[]];
    while (i < bytes.length) {
      const c = bytes[i++];
      if (c === 125) { // }
        if (!depth) throw Error('unmatched }');
        return choices;
      }
      if (c === 44) { // ,
        if (!depth) throw Error('comma outside alternatives');
        choices.push([]); continue;
      }
      let token: Token;
      if (c === 123) { // {
        if (depth >= 4) throw Error('alternatives nesting limit');
        token = { kind: 'alt', options: parts(depth + 1) };
      } else if (c === 42) token = { kind: 'star' };
      else if (c === 63) token = { kind: 'any' };
      else if (c === 91) { // [
        const negated = bytes[i] === 33; // !
        if (negated) i++;
        const mask = new Uint8Array(256);
        let any = false;
        while (i < bytes.length && bytes[i] !== 93) {
          const lo = bytes[i++];
          let hi = lo;
          if (bytes[i] === 45 && i + 1 < bytes.length && bytes[i + 1] !== 93) {
            i++; hi = bytes[i++];
            if (hi < lo) throw Error('reversed class range');
          }
          for (let n = lo; n <= hi; n++) mask[n] = 1;
          any = true;
        }
        if (!any || bytes[i] !== 93) throw Error('unclosed class');
        i++;
        if (negated) for (let n = 0; n < 256; n++) mask[n] ^= 1;
        mask[47] = 0; // Native classes cannot consume '/'.
        token = { kind: 'class', mask };
      } else {
        if (c === 93) throw Error('unmatched ]');
        let byte = c;
        if (c === 92) { // escaped literal
          if (i === bytes.length) throw Error('trailing escape');
          byte = bytes[i++];
        }
        token = { kind: 'literal', byte };
      }
      choices[choices.length - 1].push(token);
    }
    if (depth) throw Error('unclosed alternatives');
    return choices;
  };
  try { return parts(0)[0]; } catch { return null; }
}

/** All possible byte offsets after each token, at most 256 states per token. */
function walk(tokens: Token[], bytes: Uint8Array, starts: Set<number>): Set<number> {
  let positions = starts;
  for (const token of tokens) {
    const next = new Set<number>();
    if (token.kind === 'alt') {
      for (const option of token.options ?? [])
        for (const end of walk(option, bytes, positions)) next.add(end);
    } else if (token.kind === 'star') {
      // Prefix closure in each slash-separated segment: O(bytes), not O(bytes²).
      let reachable = false;
      for (let at = 0; at <= bytes.length; at++) {
        if (positions.has(at)) reachable = true;
        if (reachable) next.add(at);
        if (bytes[at] === 47) reachable = false;
      }
    } else {
      for (const at of positions) {
        if (at < bytes.length &&
          (token.kind === 'literal' && bytes[at] === token.byte ||
            token.kind === 'any' && bytes[at] !== 47 ||
            token.kind === 'class' && token.mask?.[bytes[at]] === 1)) next.add(at + 1);
      }
    }
    positions = next;
    if (!positions.size) break;
  }
  return positions;
}

/** False for invalid patterns, as in native Pattern::compile/match. */
export function matchOscAddress(pattern: string, address: string): boolean {
  const bytes = utf8.encode(address);
  if (bytes.length > 255) return false;
  if (!patterns.has(pattern)) {
    if (patterns.size >= 128) patterns.clear();
    patterns.set(pattern, compile(pattern));
  }
  const key = `${pattern.length}:${pattern}${address}`;
  const cached = results.get(key);
  if (cached !== undefined) return cached;
  const tokens = patterns.get(pattern);
  const matches = !!tokens && walk(tokens, bytes, new Set([0])).has(bytes.length);
  if (results.size >= 512) results.clear();
  results.set(key, matches);
  return matches;
}

export interface RawReading { value?: number; reason?: string }

/** Read only real, finite Monitor ingress observed directly from this osc.in. */
export function readOscRaw(doc: PatchDoc, t: Telemetry | null, source: number,
  row: { pattern: string; argument: number; enabled?: boolean }): RawReading {
  if (row.enabled === false) return { reason: 'Enable this row to capture.' };
  if (!Number.isInteger(row.argument) || row.argument < 0 || row.argument > 3)
    return { reason: 'Capture reads arguments 0–3.' };
  const monitors = doc.connections.filter(c => c.kind === 'event' &&
    c.from.module === source && c.from.port === 'messages' && c.to.port === 'events' &&
    doc.modules.get(c.to.module)?.module === 'monitor').map(c => c.to.module);
  if (!monitors.length) return { reason: 'Patch OSC In messages → Monitor events to capture.' };
  if (t?.monitorAvailable !== true) return { reason: 'Monitor data is not available from this engine.' };
  let newest = -Infinity;
  let value: number | undefined;
  for (const id of monitors) {
    for (const entry of t.monitor?.[id] ?? []) {
      const next = entry.args?.[row.argument];
      if (entry.kind !== 'osc' || !entry.address || !Number.isFinite(entry.t) ||
          (typeof next !== 'number' || !Number.isFinite(next)) || !matchOscAddress(row.pattern, entry.address)) continue;
      if (entry.t >= newest) { newest = entry.t; value = next; }
    }
  }
  return value === undefined ? { reason: 'Move the control first.' } : { value };
}
