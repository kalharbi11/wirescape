// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleDescriptor } from '../modules/types';

export interface ModulePreset { name: string; params: Readonly<Record<string, number>> }
/** Data keyed by module type. Preset names remain UI-only, never saved to patches. */
export const MODULE_PRESETS: Readonly<Record<string, readonly ModulePreset[]>> = {
  'essentials.chaos_lite': [
    // Four starting points from native/modules2/chaos_lite/README.md.
    { name: 'Warm fifth', params: { shape_a: 2, shape_b: 2, detune: 7, body_mix: 0.15, roughness: 0 } },
    { name: 'Bent wire', params: { route: 2, depth: 0.8, body_mix: 0.65, feedback: 0.7, drift: 0.25 } },
    { name: 'Broken bell', params: { route: 1, depth: 0.9, shape_a: 2, octave_b: 1, body_tune: 1, fall: 0.1 } },
    { name: 'Slow weather', params: { lfo_target: 1, lfo_rate: 0.1, lfo_depth: 0.5, drift: 0.8, jitter: 0.2 } },
    // Four original sound-design variations, bounded below the feedback/output ceilings.
    { name: 'Basalt drone', params: {
      shape_a: 0.1, shape_b: 1.15, detune: -11, octave_b: -2, level_a: 0.65,
      level_b: 0.4, body_mix: 0.75, feedback: 0.65, damping: 0.75,
      roughness: 0.12, rough_rate: 18, drift: 0.18, jitter: 0.1, cutoff: 920, level: 0.5,
    } },
    { name: 'Glass pluck', params: {
      shape_a: 2, shape_b: 2, octave_b: 1, route: 2, depth: 0.22,
      body_mix: 0.45, feedback: 0.52, damping: 0.65, fall: 0.68,
      lfo_depth: 0, roughness: 0.01, cutoff: 12500, level: 0.55,
    } },
    { name: 'Iron rain', params: {
      shape_a: 0.35, shape_b: 0.9, route: 1, depth: 0.95, octave_b: 2,
      detune: 21, roughness: 0.66, rough_rate: 57, body_mix: 0.27,
      feedback: 0.33, drift: 0.12, jitter: 0.26, cutoff: 6400, level: 0.4,
    } },
    { name: 'Tide choir', params: {
      shape_a: 2, shape_b: 2, detune: -4, octave_b: 0, pan_a: -0.4, pan_b: 0.4,
      body_mix: 0.43, feedback: 0.37, damping: 0.68, lfo_rate: 0.14,
      lfo_target: 2, lfo_depth: 0.4, lfo_self: 0.1, cutoff: 4200, level: 0.52,
    } },
  ],
};

/** Expands a preset to ALL known descriptor params; unknown patch keys are untouched. */
export function resolvePreset(d: ModuleDescriptor, p: ModulePreset): Record<string, number> {
  const ids = new Set(d.parameters.map(x => x.id));
  for (const id of Object.keys(p.params)) if (!ids.has(id))
    throw new Error('Unknown preset parameter: ' + d.id + ':' + id);
  const values: Record<string, number> = {};
  for (const x of d.parameters) {
    const v = p.params[x.id] ?? x.default;
    if (!Number.isFinite(v) || v < x.minimum || v > x.maximum ||
        (x.step > 0 && Math.abs(v / x.step - Math.round(v / x.step)) > 1e-8))
      throw new Error('Invalid preset value: ' + d.id + ':' + x.id);
    values[x.id] = v;
  }
  return values;
}

/** A label is inferred from current saved parameter values, never serialized. */
export function matchingPreset(d: ModuleDescriptor, params: Readonly<Record<string, number>>): string | null {
  for (const p of MODULE_PRESETS[d.id] ?? []) {
    const resolved = resolvePreset(d, p);
    if (d.parameters.every(x => (params[x.id] ?? x.default) === resolved[x.id])) return p.name;
  }
  return null;
}
