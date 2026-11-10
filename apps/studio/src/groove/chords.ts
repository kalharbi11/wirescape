// SPDX-License-Identifier: GPL-3.0-only
/**
 * Groove chord steps (G4b, #514). Pure data: no DOM, no engine.
 *
 * A note step keeps `key` as the root. 2-5 notes are stored as `keys` with
 * `keys[0] === key` (the native G4a rule); a single note never writes `keys`.
 * No chord type is stored: names come from the keys, and a chord moves
 * diatonically only when its keys equal the scale stack on its root.
 */

export const MAX_KEYS = 5;
/** Pitch-class names for chord symbols (flats where a chord name reads better). */
export const PC_NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'] as const;
const NOTE_NAMES = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'] as const;
const pc = (k: number): number => ((k % 12) + 12) % 12;
const noteName = (k: number): string => `${NOTE_NAMES[pc(k)]}${Math.floor(k / 12) - 1}`;
const asc = (a: number, b: number): number => a - b;

/** A scale as the UI holds it: a root pitch class and its intervals from the root. */
export interface ScaleCtx { root: number; intervals: readonly number[] }
export const CHROMATIC: ScaleCtx = { root: 0, intervals: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] };

export type Quality = 'maj' | 'min' | 'dim' | 'aug' | 'sus2' | 'sus4' | '5'
  | 'maj7' | 'm7' | '7' | 'm7b5' | '6' | 'm6' | 'add9' | 'm9';
/** Picker order, symbol and intervals (from #521 `groove-r3-chords.html`). */
export const QUALITIES: readonly (readonly [Quality, string, readonly number[]])[] = [
  ['maj', '', [0, 4, 7]], ['min', 'm', [0, 3, 7]], ['dim', '°', [0, 3, 6]], ['aug', '+', [0, 4, 8]],
  ['sus2', 'sus2', [0, 2, 7]], ['sus4', 'sus4', [0, 5, 7]], ['5', '(5)', [0, 7]],
  ['maj7', 'maj7', [0, 4, 7, 11]], ['m7', 'm7', [0, 3, 7, 10]], ['7', '7', [0, 4, 7, 10]],
  ['m7b5', 'm7♭5', [0, 3, 6, 10]], ['6', '6', [0, 4, 7, 9]], ['m6', 'm6', [0, 3, 7, 9]],
  ['add9', 'add9', [0, 4, 7, 14]], ['m9', 'm9', [0, 3, 7, 10, 14]],
];
export const PICKER_GROUPS: readonly (readonly [string, readonly Quality[]])[] = [
  ['Triads', ['maj', 'min', 'dim', 'aug', 'sus2', 'sus4', '5']],
  ['Sevenths', ['maj7', 'm7', '7', 'm7b5', '6', 'm6']],
  ['Extended', ['add9', 'm9']],
];

export const inScale = (k: number, s: ScaleCtx): boolean => s.intervals.includes(pc(k - s.root));
/** Nearest scale note, preferring below on a tie (as in the design). */
export function snapKey(k: number, s: ScaleCtx): number {
  if (inScale(k, s)) return k;
  for (let d = 1; d < 12; d++) {
    if (k - d >= 0 && inScale(k - d, s)) return k - d;
    if (k + d <= 127 && inScale(k + d, s)) return k + d;
  }
  return k;
}
/** Move by scale degrees; returns null when it would leave 0..127. */
export function degreeStep(k: number, degrees: number, s: ScaleCtx): number | null {
  let x = k;
  const dir = Math.sign(degrees);
  for (let n = Math.abs(degrees); n > 0;) {
    x += dir;
    if (x < 0 || x > 127) return null;
    if (inScale(x, s)) n--;
  }
  return x;
}

/** Notes of a step: `keys` when it holds 2-5 notes, else the root alone. */
export function notesOf(step: { key?: number; keys?: unknown }, fallback = 60): number[] {
  const keys = step.keys;
  if (Array.isArray(keys) && keys.length > 1 && keys.every(k => Number.isInteger(k))) return [...keys as number[]];
  return [step.key ?? fallback];
}

/** True when `keys` would pass the native G4a decoder. */
export function validKeys(key: number | undefined, keys: unknown): boolean {
  if (!Array.isArray(keys) || keys.length < 1 || keys.length > MAX_KEYS) return false;
  if (!keys.every(k => Number.isInteger(k) && k >= 0 && k <= 127)) return false;
  return new Set(keys).size === keys.length && keys[0] === key;
}

/**
 * Store notes on a step (mutates). Keeps `root` when it is one of the notes,
 * otherwise uses the lowest. Writes `keys` (root first, rest ascending) only for
 * 2-5 notes; a single note deletes `keys` so it saves exactly as before G4.
 */
// Caps at MAX_KEYS (lowest five kept); the editor toasts before a sixth note is offered.
export function setNotes(step: { key?: number; keys?: unknown }, notes: readonly number[], root?: number): void {
  const uniq = [...new Set(notes.filter(k => Number.isInteger(k) && k >= 0 && k <= 127))].sort(asc).slice(0, MAX_KEYS);
  if (!uniq.length) return;
  const r = root !== undefined && uniq.includes(root) ? root : uniq[0]!;
  step.key = r;
  if (uniq.length > 1) step.keys = [r, ...uniq.filter(k => k !== r)];
  else delete step.keys;
}

/** Chord symbol for the keys spelled on `root` (`Am`, `Cmaj7`, `Cm7/E♭`), or null if unknown. */
export function chordName(keys: readonly number[], root?: number): string | null {
  if (keys.length < 2) return null;
  const pcs = [...new Set(keys.map(pc))];
  const bass = pc(Math.min(...keys));
  const first = root === undefined ? bass : pc(root);
  for (const r of new Set([first, bass, ...pcs])) {
    const set = pcs.map(x => pc(x - r)).sort(asc).join();
    for (const [, sym, iv] of QUALITIES) {
      if ([...new Set(iv.map(pc))].sort(asc).join() === set) {
        return PC_NAMES[r] + sym + (r !== bass ? `/${PC_NAMES[bass]}` : '');
      }
    }
  }
  return null;
}

/** Step label: a note name for one key, the chord name, or root + count (`A4+2`). `short` drops the slash bass (pads). */
export function stepLabel(step: { key?: number; keys?: unknown }, short = false): string {
  const ks = notesOf(step);
  const root = step.key ?? ks[0]!;
  if (ks.length < 2) return noteName(root);
  const name = chordName(ks, root);
  if (!name) return `${noteName(root)}+${ks.length - 1}`;
  return short ? name.split('/')[0]! : name;
}

/** Scale chord on `root`: stacked thirds within the scale (n = 3 triad, 4 seventh). */
export function scaleStack(root: number, n: number, s: ScaleCtx): number[] {
  const out = [root];
  let k: number | null = root;
  for (let j = 1; j < n && k !== null; j++) {
    k = degreeStep(k, 2, s);
    if (k !== null) out.push(k);
  }
  return out;
}

/** Keys for a picker entry on `root`, clipped to 0..127 and 5 notes. */
export function qualityKeys(root: number, q: Quality): number[] {
  const iv = QUALITIES.find(x => x[0] === q)![2];
  return iv.map(x => root + x).filter(k => k <= 127).slice(0, MAX_KEYS);
}

/** True when the chord is the scale stack on its root (so it moves diatonically). */
export function isScaleChord(keys: readonly number[], root: number, s: ScaleCtx): boolean {
  if (keys.length < 2 || s.intervals.length >= 12) return false;
  const want = scaleStack(root, keys.length, s).sort(asc).join();
  return [...keys].sort(asc).join() === want;
}

/** Inversion: d > 0 moves the lowest note up an octave, d < 0 the highest down. The root follows its pitch class. */
export function invert(keys: readonly number[], root: number, d: number): { keys: number[]; root: number } | null {
  if (keys.length < 2) return null;
  const ks = [...keys].sort(asc);
  if (d > 0) {
    const lo = ks.shift()!;
    if (lo + 12 > 127 || ks.includes(lo + 12)) return null;
    ks.push(lo + 12);
    return { keys: ks, root: root === lo ? lo + 12 : root };
  }
  const hi = ks.pop()!;
  if (hi - 12 < 0 || ks.includes(hi - 12)) return null;
  ks.unshift(hi - 12);
  return { keys: ks, root: root === hi ? hi - 12 : root };
}

/**
 * Move a step's notes by `n` scale steps (or octaves). A scale chord moves
 * diatonically (Am → B° → C in A minor); anything else moves in parallel by the
 * root's scale step. Returns null when a note would leave 0..127.
 */
export function moveNotes(keys: readonly number[], root: number, n: number, s: ScaleCtx, octave = false):
  { keys: number[]; root: number } | null {
  let next: (number | null)[];
  let r: number | null;
  if (octave) {
    next = keys.map(k => k + 12 * n);
    r = root + 12 * n;
  } else if (isScaleChord(keys, root, s)) {
    next = keys.map(k => degreeStep(k, n, s));
    r = degreeStep(root, n, s);
  } else {
    r = degreeStep(snapKey(root, s), n, s);
    const d = r === null ? 0 : r - root;
    next = keys.map(k => k + d);
  }
  if (r === null || r < 0 || r > 127 || next.some(k => k === null || k < 0 || k > 127)) return null;
  return { keys: next as number[], root: r };
}
