// SPDX-License-Identifier: GPL-3.0-only
import type {
  ModuleCatalogFile, ModuleDescriptor, ModulePort, ModuleParam,
  PortDirection, PortHint,
} from './types';
import { portRef } from './types';

export class ModuleRegistry {
  private readonly byId = new Map<string, ModuleDescriptor>();

  constructor(public readonly all: ModuleDescriptor[]) {
    for (const m of all) this.byId.set(m.id, m);
  }

  static async load(url = 'wirescape_modules.json'): Promise<ModuleRegistry> {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`Cannot load module catalogue: ${res.status}`);
    const data = (await res.json()) as ModuleCatalogFile;
    if (!Array.isArray(data.modules)) throw new Error('Malformed module catalogue');
    return new ModuleRegistry(data.modules);
  }

  get(type: string): ModuleDescriptor | undefined { return this.byId.get(type); }
  has(type: string): boolean { return this.byId.has(type); }

  /** Categories in first-seen order, each with its modules. */
  categories(): { name: string; modules: ModuleDescriptor[] }[] {
    const out: { name: string; modules: ModuleDescriptor[] }[] = [];
    const index = new Map<string, number>();
    for (const m of this.all) {
      let i = index.get(m.category);
      if (i === undefined) { i = out.length; index.set(m.category, i); out.push({ name: m.category, modules: [] }); }
      out[i]!.modules.push(m);
    }
    return out;
  }

  search(query: string): ModuleDescriptor[] {
    const q = query.trim().toLowerCase();
    if (!q) return this.all;
    return this.all.filter(m =>
      m.name.toLowerCase().includes(q) ||
      m.id.toLowerCase().includes(q) ||
      m.category.toLowerCase().includes(q) ||
      m.ports.some(p => p.name.toLowerCase().includes(q)) ||
      m.parameters.some(p => p.name.toLowerCase().includes(q)));
  }
}

export const inputs = (d: ModuleDescriptor): ModulePort[] => d.ports.filter(p => p.direction === 'input');
export const outputs = (d: ModuleDescriptor): ModulePort[] => d.ports.filter(p => p.direction === 'output');

/** Ports are only unique as (direction, id) — see types.ts. */
export function findPort(d: ModuleDescriptor, dir: PortDirection, id: string): ModulePort | undefined {
  return d.ports.find(p => p.direction === dir && p.id === id);
}
export function findParam(d: ModuleDescriptor, id: string): ModuleParam | undefined {
  return d.parameters.find(p => p.id === id);
}
export const refOf = (p: ModulePort) => portRef(p.direction, p.id);

export function hintColor(hint: string): string {
  const known: PortHint[] = ['audio', 'pitch', 'cv_uni', 'cv_bi', 'gate', 'trigger', 'ev_notes', 'ev_midi'];
  return known.includes(hint as PortHint) ? `var(--hint-${hint})` : 'var(--hint-audio)';
}

/* ---------------------------------------------------------------------- */
/* Parameter maths: taper, stepping, formatting                            */
/* ---------------------------------------------------------------------- */

/** Knob travel 0..1 -> value, following the parameter's taper. */
export function normToValue(p: ModuleParam, n: number): number {
  const t = clamp01(n);
  let v: number;
  if (p.taper === 'log' && p.minimum > 0 && p.maximum > 0) {
    v = p.minimum * Math.pow(p.maximum / p.minimum, t);
  } else {
    v = p.minimum + (p.maximum - p.minimum) * t;
  }
  return quantize(p, v);
}

/** Value -> knob travel 0..1. */
export function valueToNorm(p: ModuleParam, v: number): number {
  const x = Math.min(p.maximum, Math.max(p.minimum, v));
  if (p.taper === 'log' && p.minimum > 0 && p.maximum > 0) {
    return clamp01(Math.log(x / p.minimum) / Math.log(p.maximum / p.minimum));
  }
  const span = p.maximum - p.minimum;
  return span === 0 ? 0 : clamp01((x - p.minimum) / span);
}

export function quantize(p: ModuleParam, v: number): number {
  let x = Math.min(p.maximum, Math.max(p.minimum, v));
  if (p.step && p.step > 0) x = Math.round((x - p.minimum) / p.step) * p.step + p.minimum;
  return Math.min(p.maximum, Math.max(p.minimum, x));
}

const clamp01 = (n: number) => (n < 0 ? 0 : n > 1 ? 1 : n);

/** True when a parameter carries labelled values at all. */
export const hasChoices = (p: ModuleParam) => Array.isArray(p.choices) && p.choices.length > 0;
/** True when a parameter should be shown as segmented buttons. */
export const isChoice = (p: ModuleParam) => Array.isArray(p.choices) && p.choices.length > 1;
/** DESIGN §4.2: 2–4 named modes are segmented; more use a dropdown (the detent is not built: no catalogue parameter has 5–8 choices). */
export const SEGMENT_LIMIT = 4;
/** True when a parameter is a plain on/off pair — shown as a toggle. */
export function isToggle(p: ModuleParam): boolean {
  if (!isChoice(p) || p.choices!.length !== 2) return false;
  const labels = p.choices!.map(c => c.label.toLowerCase());
  return ['off', 'on'].every((l, i) => labels[i] === l)
    || (labels.includes('stop') && labels.includes('run'));
}

/**
 * Value formatting by unit. Keeps bounds tidy — "20 Hz – 20 kHz", never
 * "20.000000 Hz". Returns the number and its suffix separately so the
 * readout can style them.
 */
export function formatValue(p: ModuleParam, v: number): { text: string; unit: string } {
  if (hasChoices(p)) {
    const c = p.choices!.find(c => Math.abs(c.value - v) < 1e-6);
    if (c) return { text: c.label, unit: '' };
  }
  // DESIGN §3: round to what matters; bipolar values carry a sign.
  const bip = p.minimum < 0 && p.maximum > 0;
  switch (p.unit) {
    case 'Hz': {
      const a = Math.abs(v);
      if (a >= 1000) return { text: sign(v / 1000, a >= 10000 ? 0 : 1, bip), unit: 'kHz' };
      return { text: sign(v, a < 1 ? 2 : a < 100 ? 1 : 0, bip), unit: 'Hz' };
    }
    case 'ms': case 's': {
      const ms = p.unit === 's' ? v * 1000 : v;
      const a = Math.abs(ms);
      if (a >= 1000) return { text: sign(ms / 1000, 1, bip), unit: 's' };
      return { text: sign(ms, a < 10 ? 1 : 0, bip), unit: 'ms' };
    }
    case 's/unit': return { text: trim(v, v < 10 ? 2 : 1), unit: 's' };
    case 'BPM': return { text: trim(v, 1), unit: 'bpm' };
    case 'dB': return v <= -59.5 ? { text: '−∞', unit: 'dB' } : { text: sign(v, 1, true), unit: 'dB' };
    case 'cent': return { text: sign(v, 0, true), unit: 'ct' };
    case 'st': return { text: sign(v, p.step >= 1 ? 0 : 1, true), unit: 'st' };
    case 'oct': return { text: sign(v, v % 1 === 0 ? 0 : 2, bip), unit: 'oct' };
    case 'oct/s': return { text: trim(v, 2), unit: 'oct/s' };
    case 'beats': return { text: trim(v, v < 1 ? 3 : 2), unit: 'beats' };
    case '': {
      // Unitless 0..1 parameters read best as a percentage.
      if (p.minimum >= -1 && p.maximum <= 2 && !p.step) return { text: sign(v * 100, 0, bip), unit: '%' };
      return { text: sign(v, p.step ? 0 : 2, bip), unit: '' };
    }
    default: return { text: sign(v, 2, bip), unit: p.unit };
  }
}

/** The value as shown, e.g. `1.2 kHz`, `40%` (also the starting text when typing a value). */
export function displayValue(p: ModuleParam, v: number): string {
  const f = formatValue(p, v);
  return f.unit === '%' ? `${f.text}%` : f.unit ? `${f.text} ${f.unit}` : f.text;
}

const NOTE_SEMIS: Record<string, number> = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 };

/**
 * Parse a typed value (OPEN-8), FabFilter-style. Accepts what the readout
 * shows and the obvious shorthands: `2k`, `1.2 kHz`, `A4` (Hz parameters),
 * `350 ms`, `1.5 s`, `40%`, `−6 dB`, `+3`, or a choice label. A bare number
 * is in the parameter's own unit (`2000` on a cutoff is 2 kHz), except a
 * percentage, which reads as shown (`40` on a 40% knob means 40%).
 * Returns the quantised value, or null when the text makes no sense.
 */
export function parseTyped(p: ModuleParam, text: string, current: number): number | null {
  const t = text.trim().replace(/[−–]/g, '-').replace(/,/g, '.').toLowerCase();
  if (!t) return null;
  if (p.choices?.length) {
    const c = p.choices.find(c => c.label.toLowerCase() === t);
    if (c) return c.value;
  }
  if (p.unit === 'Hz') {
    const n = t.match(/^([a-g])([#♯b♭]?)(-?\d)$/);
    if (n) {
      const semi = NOTE_SEMIS[n[1]!]! + (n[2] === '#' || n[2] === '♯' ? 1 : n[2] === 'b' || n[2] === '♭' ? -1 : 0);
      const midi = (Number(n[3]) + 1) * 12 + semi;
      return quantize(p, 440 * Math.pow(2, (midi - 69) / 12));
    }
  }
  const m = t.match(/^([-+]?\d*\.?\d+(?:e[-+]?\d+)?)\s*(k|khz|hz|ms|s|sec|%|db|st|ct|cent|bpm|x)?$/);
  if (!m) return null;
  let v = Number(m[1]);
  if (!Number.isFinite(v)) return null;
  const suffix = m[2] ?? '';
  const shown = formatValue(p, current).unit;
  // A bare number is in the parameter's own unit (Hz, ms…), except a percentage, which reads as shown.
  const unit = suffix;
  if (unit === 'k' || unit === 'khz') v *= 1000;
  if (p.unit === 'ms' && (unit === 's' || unit === 'sec')) v *= 1000;
  if (p.unit === 's' && unit === 'ms') v /= 1000;
  if (shown === '%' && (unit === '%' || !suffix)) v /= 100;
  return quantize(p, v);
}

/** Signed with a true minus sign; `always` adds '+' to positive values. */
function sign(n: number, decimals: number, always: boolean): string {
  const t = trim(Math.abs(n), decimals);
  if (t === '0' || t === '—') return t;
  return (n < 0 ? '−' : always ? '+' : '') + t;
}

/** A value as one string: `900 Hz`, `40%` (DESIGN §3). */
export function formatText(p: ModuleParam, v: number): string {
  const f = formatValue(p, v);
  if (!f.unit) return f.text;
  return f.unit === '%' ? `${f.text}%` : `${f.text} ${f.unit}`;
}

/** Short "min – max" summary for tooltips. */
export function formatRange(p: ModuleParam): string {
  const a = formatValue(p, p.minimum), b = formatValue(p, p.maximum);
  const au = a.unit && a.unit !== b.unit ? ` ${a.unit}` : '';
  return `${a.text}${au} – ${b.text}${b.unit ? ` ${b.unit}` : ''}`;
}

function trim(n: number, decimals: number): string {
  if (!Number.isFinite(n)) return '—';
  const s = n.toFixed(decimals);
  return s.includes('.') ? s.replace(/\.?0+$/, '') : s;
}
