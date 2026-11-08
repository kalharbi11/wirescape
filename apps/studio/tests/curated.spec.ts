// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { STARTERS, DEFAULT_STARTER } from '../src/app/starters';
import { CURATED_PATCHES } from '../src/app/curated';
import catalogue from '../public/wirescape_modules.json' with { type: 'json' };
import { boot, settle } from './helpers';

const curated = STARTERS.filter(s => CURATED_PATCHES.some(p => p.id === s.id));
const descriptors = new Map(catalogue.modules.map(m => [m.id, m]));

test('curated collection is eight independent, valid v2 patches with no replaced generic starters', () => {
  expect(curated).toHaveLength(8);
  expect(new Set(curated.map(s => s.id)).size).toBe(8);
  expect(DEFAULT_STARTER.id).toBe('keyboard-voice'); // web smoke-test baseline
  expect(STARTERS.filter(s => s.id.startsWith('qt-'))).toHaveLength(6); // session fixtures
  for (const removed of ['drone', 'clocked-random', 'sequence', 'macro', 'chaos']) {
    expect(STARTERS.some(s => s.id === removed)).toBe(false);
  }
  for (const starter of curated) {
    const patch = starter.patch();
    expect(patch.format).toBe('wirescape.native.patch');
    expect(patch.version).toBe(2);
    expect(patch.settings?.sample_rate).toBe(48000);
    expect(patch.outputs).toHaveLength(2);
    expect(patch.modules.filter(m => m.module === 'essentials.output')).toHaveLength(1);
    const modules = new Map(patch.modules.map(m => [m.id, m]));
    expect(modules.size).toBe(patch.modules.length);
    for (const module of patch.modules) {
      const descriptor = descriptors.get(module.module);
      expect(descriptor, module.module).toBeDefined();
      expect(module.version).toBe(descriptor!.version);
      for (const [id, value] of Object.entries(module.parameters ?? {})) {
        const param = descriptor!.parameters.find(p => p.id === id);
        expect(param, module.module + ':' + id).toBeDefined();
        expect(value).toBeGreaterThanOrEqual(param!.minimum);
        expect(value).toBeLessThanOrEqual(param!.maximum);
      }
      expect(patch.ui?.positions?.[String(module.id)]).toHaveLength(2);
    }
    for (const connection of patch.connections) {
      const from = descriptors.get(modules.get(connection.from.module)!.module)!;
      const to = descriptors.get(modules.get(connection.to.module)!.module)!;
      expect(from.ports.some(p => p.id === connection.from.port && p.direction === 'output'
        && p.kind === 'signal'), starter.id + ' source').toBe(true);
      if (connection.kind === 'signal') {
        expect(to.ports.some(p => p.id === connection.to.port && p.direction === 'input'
          && p.kind === 'signal'), starter.id + ' destination').toBe(true);
      } else {
        expect(connection.kind).toBe('modulation');
        expect(to.parameters.some(p => p.id === connection.to.port && p.modulatable),
          starter.id + ' modulation target').toBe(true);
        expect(Math.abs(connection.amount ?? 2)).toBeLessThanOrEqual(1);
      }
    }
    const output = patch.modules.find(m => m.module === 'essentials.output')!;
    expect(patch.outputs).toEqual([0, 1].map(bus_channel => ({ module: output.id, port: 'audio', bus_channel })));
    patch.modules[0]!.parameters!.mutation = 999;
    expect(starter.patch().modules[0]!.parameters).not.toHaveProperty('mutation');
  }
});

test('autoplay patches have running clocks or continuously sounding sources', () => {
  const autoplay = ['hollow-chapel', 'breathing-walls', 'dust-reactor', 'witching-hour', 'neon-elegy'];
  for (const id of autoplay) {
    const patch = curated.find(s => s.id === id)!.patch();
    const sequences = patch.modules.filter(m => m.module === 'example.step_sequencer');
    for (const seq of sequences) {
      expect(seq.parameters?.run).toBe(1);
      expect(seq.parameters?.length).toBe(8);
      expect(new Set(Array.from({ length: 8 }, (_, i) => seq.parameters?.['pitch' + (i + 1)])).size)
        .toBeGreaterThan(3);
      expect(Array.from({ length: 8 }, (_, i) => seq.parameters?.['gate' + (i + 1)]))
        .toContain(0);
      const clock = patch.modules.find(m => m.module === 'essentials.clock')!;
      expect(clock.parameters?.run).toBe(1);
      expect(patch.connections.some(c => c.kind === 'signal' && c.from.module === clock.id
        && c.to.module === seq.id && c.to.port === 'clock')).toBe(true);
    }
    expect(patch.modules.some(m => m.module === 'host.keyboard')).toBe(false);
  }
});

for (const starter of curated) {
  test(starter.id + ' loads into the Studio and preserves its patch graph', async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await boot(page);
    const patch = starter.patch();
    await page.evaluate(({ patch, name }) => window.__wirescapeTest.load(patch, name),
      { patch, name: starter.name });
    await settle(page);
    const saved = await page.evaluate(() => window.__wirescapeTest.state());
    for (const field of ['modules', 'connections', 'outputs', 'settings', 'name']) {
      expect(saved[field], starter.id + ' ' + field).toEqual(patch[field]);
    }
    await expect(page.locator('[data-testid^="module-header-"]')).toHaveCount(patch.modules.length);
    expect(errors).toEqual([]);
  });
}
