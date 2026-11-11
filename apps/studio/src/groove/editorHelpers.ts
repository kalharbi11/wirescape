// SPDX-License-Identifier: GPL-3.0-only
/** G2b authoring helpers. No engine data is changed by view-only scale state. */
import {
  setKind, sourceOf, stepAt, stepFor, trackLength,
  type GrooveConfig, type Kind, type Step,
} from './pattern';
import { notesOf } from './chords';

export type Scale = 'Chromatic' | 'Major' | 'Minor' | 'Pentatonic';
export const SCALES: Record<Scale, readonly number[]> = {
  Chromatic: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  Major: [0, 2, 4, 5, 7, 9, 11],
  Minor: [0, 2, 3, 5, 7, 8, 10],
  Pentatonic: [0, 2, 4, 7, 9],
};
export const ROOTS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'] as const;
export const inScale = (key: number, root: number, scale: Scale): boolean =>
  SCALES[scale].includes(((key - root) % 12 + 12) % 12);

/** Advance by scale degrees, not semitones; a non-scale starting note stays until an edit. */
export function movePitch(key: number, degrees: number, root: number, scale: Scale): number {
  let result = key;
  const direction = Math.sign(degrees);
  for (let i = 0; i < Math.abs(degrees); i++) {
    while (result + direction >= 0 && result + direction <= 127) {
      result += direction;
      if (inScale(result, root, scale)) break;
    }
  }
  return result;
}

const copy = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T;
const FIELDS = ['kind', 'key', 'keys', 'velocity', 'gate', 'cond', 'mod1', 'mod2'] as const;
export function barStart(step: number): number { return Math.floor(step / 16) * 16; }
export function copyBar(cfg: GrooveConfig, track: number, step: number): Step[] {
  const start = barStart(step), end = Math.min(start + 16, trackLength(cfg, track));
  return Array.from({ length: end - start }, (_, n) => copy(stepAt(cfg, track, start + n)));
}
/** Writes only step-authoring fields; unrelated future keys at the target survive. */
export function pasteBar(cfg: GrooveConfig, track: number, step: number, clipboard: readonly Step[]): void {
  const start = barStart(step), end = Math.min(start + 16, trackLength(cfg, track));
  for (let i = start; i < end && i - start < clipboard.length; i++) {
    const target = stepFor(cfg, track, i);
    for (const key of FIELDS) delete (target as Record<string, unknown>)[key];
    const src = clipboard[i - start]!;
    for (const key of FIELDS) if (src[key] !== undefined) {
      (target as Record<string, unknown>)[key] = copy(src[key]);
    }
  }
}
export function clearBar(cfg: GrooveConfig, track: number, step: number): void {
  const start = barStart(step), end = Math.min(start + 16, trackLength(cfg, track));
  for (let i = start; i < end; i++) {
    const target = stepFor(cfg, track, i);
    setKind(target, 'rest');
    delete target.gate;
    delete target.mod1;
    delete target.mod2;
  }
}

/** Per-pitch roll projection: all voices of a step share its start and ties. */
export function rollNotes(cfg: GrooveConfig, track: number): Map<number, Map<number, Kind>> {
  const result = new Map<number, Map<number, Kind>>();
  for (let i = 0; i < trackLength(cfg, track); i++) {
    const s = stepAt(cfg, track, i), src = sourceOf(cfg, track, i);
    if (src < 0 || (s.kind !== 'note' && s.kind !== 'tie')) continue;
    for (const key of notesOf(stepAt(cfg, track, src))) {
      if (!result.has(key)) result.set(key, new Map());
      result.get(key)!.set(i, s.kind);
    }
  }
  return result;
}
