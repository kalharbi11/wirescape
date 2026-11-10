// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import catalogue from '../public/wirescape_modules.json';
import type { ModuleDescriptor } from '../src/modules/types';
import { MODULE_PRESETS, matchingPreset, resolvePreset } from '../src/app/modulePresets';

const descriptor = catalogue.modules.find(m => m.id === 'essentials.chaos_lite') as ModuleDescriptor | undefined;
if (!descriptor) throw new Error('CHAOS descriptor is missing from generated catalogue');
const presets = MODULE_PRESETS['essentials.chaos_lite']!;
if (!presets) throw new Error('CHAOS presets are missing');

test.describe('module preset data validation (#539)', () => {
  test('unknown parameter names are refused, not silently saved', () => {
    expect(() => resolvePreset(descriptor, { name: 'Future', params: { future_knob: 0.5 } }))
      .toThrow('Unknown preset parameter: essentials.chaos_lite:future_knob');
  });

  test('out-of-range and off-step values are refused', () => {
    expect(() => resolvePreset(descriptor, { name: 'Too loud', params: { feedback: 1.5 } }))
      .toThrow('Invalid preset value: essentials.chaos_lite:feedback');
    expect(() => resolvePreset(descriptor, { name: 'Half route', params: { route: 1.5 } }))
      .toThrow('Invalid preset value: essentials.chaos_lite:route');
    expect(() => resolvePreset(descriptor, { name: 'Not finite', params: { level: Number.NaN } }))
      .toThrow('Invalid preset value: essentials.chaos_lite:level');
  });

  test('unspecified parameters receive descriptor defaults exactly', () => {
    const values = resolvePreset(descriptor, { name: 'Partial', params: { route: 2 } });
    expect(Object.keys(values)).toEqual(descriptor.parameters.map(p => p.id));
    for (const param of descriptor.parameters) {
      expect(values[param.id], param.id).toBe(param.id === 'route' ? 2 : param.default);
    }
  });

  test('changing any single parameter of a named preset produces Custom', () => {
    for (const preset of presets) {
      const values = resolvePreset(descriptor, preset);
      expect(matchingPreset(descriptor, values), preset.name).toBe(preset.name);
      for (const param of descriptor.parameters) {
        if (param.minimum === param.maximum) continue;
        const changed = param.minimum === values[param.id] ? param.maximum : param.minimum;
        expect(matchingPreset(descriptor, { ...values, [param.id]: changed }),
          preset.name + '/' + param.id).toBeNull();
      }
    }
  });
});
