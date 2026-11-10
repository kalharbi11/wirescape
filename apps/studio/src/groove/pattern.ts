// SPDX-License-Identifier: GPL-3.0-only
/**
 * Groove pattern model (config v2, docs/tasks/GROOVE_PLAYABLE_V1.md "Keel").
 * Pure data: no DOM, no engine. The editable copy keeps unknown keys; the
 * engine copy (`engineConfig`) carries only the fields the native parser knows.
 */
export type Kind = 'rest' | 'note' | 'tie';
export type Cond =
  | { type: 'chance'; p: number }
  | { type: 'ab'; a: number; b: number }
  | { type: 'first' }
  | { type: 'not_first' };
export interface Step {
  kind: Kind; key?: number; velocity?: number; gate?: number; cond?: Cond;
  mod1?: number; mod2?: number; [extra: string]: unknown;
}
export interface Track { length?: number; steps: Step[]; armed?: boolean; mute?: boolean; [extra: string]: unknown }
export interface GrooveConfig { version: number; bpm: number; tracks: Track[]; [extra: string]: unknown }

export const GROOVE = 'sequencing.groove';
export const LENGTHS = [8, 16, 32, 64] as const;
export const DIVS = [1, 2, 4, 8] as const;
export const SPEED_LABEL: Record<number, string> = { 1: '1×', 2: '½', 4: '¼', 8: '⅛' };
export const GATES = [0.25, 0.5, 0.75, 1] as const;
export const GATE_LABEL: Record<number, string> = { 0.25: '¼', 0.5: '½', 0.75: '¾', 1: '1' };
/** Picker order: Always, chance, A:B ("loops"), first / not first. */
export const CONDS: (Cond | null)[] = [null,
  ...[10, 25, 50, 75, 90].map(p => ({ type: 'chance', p }) as Cond),
  ...[[1, 2], [2, 2], [1, 4], [2, 4], [3, 4], [4, 4], [1, 8]].map(([a, b]) => ({ type: 'ab', a, b }) as Cond),
  { type: 'first' }, { type: 'not_first' }];
/** Default "pen" pitch per track: the last pitch used there. C4 lead, C3 bass. */
export const PEN = [60, 48];

const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const clone = <T>(v: T): T => JSON.parse(JSON.stringify(v)) as T;
const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v);
const clamp = (v: number, a: number, b: number) => Math.max(a, Math.min(b, v));

export const noteName = (k: number): string => `${NAMES[((k % 12) + 12) % 12]}${Math.floor(k / 12) - 1}`;
export function parseNote(text: string): number | null {
  const s = text.trim();
  if (/^\d+$/.test(s)) return clamp(Number(s), 0, 127);
  const m = /^([A-Ga-g])([#♯b]?)(-?\d)$/.exec(s);
  if (!m) return null;
  const base = NAMES.indexOf(m[1]!.toUpperCase());
  return clamp((Number(m[3]) + 1) * 12 + base + (m[2] === 'b' ? -1 : m[2] ? 1 : 0), 0, 127);
}
export function condLabel(c: Cond | null | undefined, long = false): string {
  if (!c) return long ? 'Always' : '';
  switch (c.type) {
    case 'chance': return `${c.p}%`;
    case 'ab': return `${c.a}:${c.b}`;
    case 'first': return long ? 'First' : '1st';
    case 'not_first': return long ? 'Not first' : '!1st';
  }
}
export const sameCond = (a: Cond | null | undefined, b: Cond | null | undefined): boolean =>
  JSON.stringify(a ?? null) === JSON.stringify(b ?? null);

const emptyTrack = (length = 16): Track => ({ length, steps: [{ kind: 'rest' }] });
export const defaultConfig = (): GrooveConfig => ({ version: 2, bpm: 120, tracks: [emptyTrack(), emptyTrack()] });

/** The editable copy, or null for a version this editor does not understand (shown read-only). */
export function readConfig(raw: unknown): GrooveConfig | null {
  if (raw === undefined || (isObj(raw) && !Object.keys(raw).length)) return defaultConfig();
  if (!wellFormed(raw)) return null;
  const cfg = clone(raw) as GrooveConfig;
  if (typeof cfg.bpm !== 'number') cfg.bpm = 120;
  return cfg;
}
/**
 * The structure the native adapter accepts: version 1 or 2 and 1-2 track objects,
 * each with a `steps` array. Anything else is shown read-only and never repaired.
 */
function wellFormed(raw: unknown): raw is GrooveConfig {
  if (!isObj(raw) || (raw.version !== 1 && raw.version !== 2) || !Array.isArray(raw.tracks)) return false;
  const tracks = raw.tracks as unknown[];
  return tracks.length >= 1 && tracks.length <= 2 && tracks.every(t => isObj(t) && Array.isArray(t.steps));
}

export function trackLength(cfg: GrooveConfig, t: number): number {
  if (cfg.version === 1) return 32;
  // A missing second track takes the first's length; never recurse (tracks may be empty).
  const track = cfg.tracks[t] ?? cfg.tracks[0];
  const length = track?.length as number;
  return (LENGTHS as readonly number[]).includes(length) ? length : 16;
}
/** UI time is measured in sixteenths; telemetry remains the track's own step index. */
export function trackDiv(cfg: GrooveConfig, t: number): number {
  const div = cfg.tracks[t]?.div;
  return (DIVS as readonly number[]).includes(div as number) ? div as number : 1;
}
export const trackCycle = (cfg: GrooveConfig, t: number): number => trackLength(cfg, t) * trackDiv(cfg, t);
export function masterCycle(cfg: GrooveConfig): number {
  const armed = cfg.tracks.map((track, t) => track.armed === false ? 0 : trackCycle(cfg, t)).filter(Boolean);
  return armed.length ? Math.max(...armed) : 16;
}
export const stepAt = (cfg: GrooveConfig, t: number, i: number): Step =>
  cfg.tracks[t]?.steps[i] ?? { kind: 'rest' };
/** Index of the note a step sounds from (itself or the note its ties lead back to), or -1. */
export function sourceOf(cfg: GrooveConfig, t: number, i: number): number {
  for (let j = i; j >= 0; j--) {
    const k = stepAt(cfg, t, j).kind;
    if (k === 'note') return j;
    if (k === 'rest') return -1;
  }
  return -1;
}

/** First edit writes v2: a v1 track keeps its 32-step meaning as an explicit length. */
export function upgrade(cfg: GrooveConfig): void {
  if (cfg.version === 1) {
    cfg.version = 2;
    for (const t of cfg.tracks) t.length = 32;
  }
  while (cfg.tracks.length < 2) cfg.tracks.push(emptyTrack(trackLength(cfg, 0)));
}
/** Writable step, padding the track with rests up to `i`. */
export function stepFor(cfg: GrooveConfig, t: number, i: number): Step {
  upgrade(cfg);
  const steps = cfg.tracks[t]!.steps;
  while (steps.length <= i) steps.push({ kind: 'rest' });
  return steps[i]!;
}
/** Change a step's kind, dropping note-only keys the native parser refuses on rests and ties. */
export function setKind(step: Step, kind: Kind, key?: number): void {
  step.kind = kind;
  if (kind === 'note') {
    if (typeof step.key !== 'number') step.key = key ?? 60;
    if (typeof step.velocity !== 'number') step.velocity = 0.8;
  } else { delete step.key; delete step.velocity; delete step.cond; }
}
export function setLength(cfg: GrooveConfig, t: number, length: number): void {
  upgrade(cfg);
  const track = cfg.tracks[t]!;
  track.length = length;
  if (track.steps.length > length) track.steps.length = length;
  if (!track.steps.length) track.steps.push({ kind: 'rest' });
}

const num = (v: unknown, lo: number, hi: number): v is number =>
  typeof v === 'number' && Number.isFinite(v) && v >= lo && v <= hi;
function engineCond(c: unknown): Cond | undefined {
  if (!isObj(c)) return undefined;
  if (c.type === 'chance' && Number.isInteger(c.p) && num(c.p, 1, 99)) return { type: 'chance', p: c.p as number };
  if (c.type === 'ab' && Number.isInteger(c.a) && Number.isInteger(c.b) && num(c.b, 2, 8) && num(c.a, 1, c.b as number))
    return { type: 'ab', a: c.a as number, b: c.b as number };
  if (c.type === 'first' || c.type === 'not_first') return { type: c.type };
  return undefined;
}

/**
 * The execution copy: only known v1/v2 fields, each step within its track length.
 * Absent, empty or unknown-version configs pass through untouched (the engine decides).
 */
export function engineConfig(raw: unknown): unknown {
  // Malformed configs go to the engine unchanged so it refuses them truthfully.
  if (!isObj(raw) || !Object.keys(raw).length || !wellFormed(raw))
    return raw === undefined ? undefined : clone(raw);
  const v2 = raw.version === 2;
  const tracks = (raw.tracks as unknown[]).slice(0, 2).map(t => {
    const track = isObj(t) ? t : {};
    const length = v2 && (LENGTHS as readonly number[]).includes(track.length as number) ? track.length as number : v2 ? 16 : 32;
    const steps = (Array.isArray(track.steps) ? track.steps : []).slice(0, length).map(s => {
      const step = isObj(s) ? s : {};
      const kind: Kind = step.kind === 'note' || step.kind === 'tie' ? step.kind : 'rest';
      const out: Step = { kind };
      if (kind === 'note') {
        out.key = Number.isInteger(step.key) && num(step.key, 0, 127) ? step.key as number : 60;
        out.velocity = num(step.velocity, 0, 1) ? step.velocity as number : 0.8;
      }
      for (const m of ['mod1', 'mod2'] as const) if (num(step[m], 0, 1)) out[m] = step[m] as number;
      if (v2 && (GATES as readonly number[]).includes(step.gate as number) && step.gate !== 1) out.gate = step.gate as number;
      const cond = v2 && kind === 'note' ? engineCond(step.cond) : undefined;
      if (cond) out.cond = cond;
      return out;
    });
    const out: Track = { ...(v2 ? { length } : {}), steps: steps.length ? steps : [{ kind: 'rest' }] };
    // Preserve the exact authored div (including invalid values) for the native refusal path.
    if ('div' in track) out.div = track.div;
    if (typeof track.armed === 'boolean') out.armed = track.armed;
    if (typeof track.mute === 'boolean') out.mute = track.mute;
    return out;
  });
  return { version: raw.version, bpm: num(raw.bpm, 20, 300) ? raw.bpm : 120,
    tracks: tracks.length ? tracks : [emptyTrack(v2 ? 16 : 32)] };
}
