// SPDX-License-Identifier: GPL-3.0-only
import type { PatchDoc } from '../patch/PatchDoc';
import type { Telemetry } from '../engine/EngineClient';

/** A small UI-side matcher for native/io/osc/pattern.h. Wildcards never cross '/'. */
const patterns = new Map<string, RegExp | null>();
const escaped = (c: string): string => /[\\^$.*+?()[\]{}|]/.test(c) ? '\\' + c : c;
const classChar = (c: string): string => /[\\\]\-^]/.test(c) ? '\\' + c : c;

function compile(pattern: string): RegExp | null {
  if (new TextEncoder().encode(pattern).length > 255) return null;
  let i = 0;
  const group = (nested: boolean, depth: number): string => {
    const choices: string[] = [];
    let part = '';
    while (i < pattern.length) {
      const c = pattern[i++];
      if (c === '}') {
        if (!nested) throw Error('unpaired }');
        choices.push(part);
        return '(?:' + choices.join('|') + ')';
      }
      if (c === ',' && nested) { choices.push(part); part = ''; continue; }
      if (c === '{') {
        if (depth >= 4) throw Error('nested alternatives');
        part += group(true, depth + 1);
      } else if (c === '*') part += '[^/]*';
      else if (c === '?') part += '[^/]';
      else if (c === '[') {
        const negated = pattern[i] === '!';
        if (negated) i++;
        let content = '';
        while (i < pattern.length && pattern[i] !== ']') {
          const lo = pattern[i++];
          if (pattern[i] === '-' && i + 1 < pattern.length && pattern[i + 1] !== ']') {
            i++;
            const hi = pattern[i++];
            if (hi.charCodeAt(0) < lo.charCodeAt(0)) throw Error('invalid range');
            content += classChar(lo) + '-' + classChar(hi);
          } else content += classChar(lo);
        }
        if (!content || pattern[i] !== ']') throw Error('invalid class');
        i++;
        part += negated ? '[^/' + content + ']' : '(?=[^/])[' + content + ']';
      } else if (c === ']') throw Error('unpaired ]');
      else if (c === '\\') {
        if (i === pattern.length) throw Error('trailing escape');
        part += escaped(pattern[i++]);
      } else part += escaped(c);
    }
    if (nested) throw Error('unclosed {');
    return part;
  };
  try { return new RegExp('^(?:' + group(false, 0) + ')(?![\\s\\S])'); }
  catch { return null; }
}

/** False for invalid patterns, as in native Pattern::compile/match. */
export function matchOscAddress(pattern: string, address: string): boolean {
  if (new TextEncoder().encode(address).length > 255) return false;
  if (!patterns.has(pattern)) {
    if (patterns.size >= 128) patterns.clear();
    patterns.set(pattern, compile(pattern));
  }
  return patterns.get(pattern)?.test(address) ?? false;
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
