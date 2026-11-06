// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { STARTERS, DEFAULT_STARTER } from '../src/app/starters';
import catalogue from '../public/wirescape_modules.json' with { type: 'json' };
import { boot, settle } from './helpers';

const qt = STARTERS.filter(s => s.id.startsWith('qt-'));

test('six Qt starters retain native data and use valid catalogue endpoints', () => {
  expect(qt).toHaveLength(6);
  expect(DEFAULT_STARTER.id).toBe('keyboard-voice');
  for (const starter of qt) {
    const patch = starter.patch();
    expect(patch['SPDX-License-Identifier']).toBe('GPL-3.0-only');
    expect(patch.settings).toHaveProperty('seed', 42);
    expect(patch.outputs).toHaveLength(2);
    const output = patch.modules.find(m => m.module === 'essentials.output')!;
    expect(patch.outputs).toEqual([0, 1].map(bus_channel => ({ module: output.id, port: 'audio', bus_channel })));
    const types = new Map(patch.modules.map(m => [m.id, catalogue.modules.find(d => d.id === m.module)!]));
    for (const module of patch.modules) {
      const descriptor = types.get(module.id);
      expect(descriptor, module.module).toBeDefined();
      for (const [id, value] of Object.entries(module.parameters ?? {})) {
        const param = descriptor!.parameters.find(p => p.id === id);
        expect(param, `${module.module}:${id}`).toBeDefined();
        expect(value).toBeGreaterThanOrEqual(param!.minimum);
        expect(value).toBeLessThanOrEqual(param!.maximum);
      }
      expect(patch.ui?.positions?.[String(module.id)]).toHaveLength(2);
    }
    for (const connection of patch.connections) {
      const from = types.get(connection.from.module)!.ports.find(p => p.direction === 'output' && p.id === connection.from.port);
      const to = types.get(connection.to.module)!.ports.find(p => p.direction === 'input' && p.id === connection.to.port);
      expect(from, `${starter.id}: output ${connection.from.port}`).toBeDefined();
      expect(to, `${starter.id}: input ${connection.to.port}`).toBeDefined();
      expect(from!.kind).toBe(connection.kind);
      expect(to!.kind).toBe(connection.kind);
    }
    // Each invocation returns an independent document, including native extras.
    patch.modules[0]!.parameters!.testMutation = 1;
    expect(starter.patch().modules[0]!.parameters).not.toHaveProperty('testMutation');
  }
});

for (const starter of qt) {
  test(`${starter.id} loads and saves native fields without loss`, async ({ page }) => {
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await boot(page);
    const source = starter.patch();
    await page.evaluate(({ patch, name }) => window.__wirescapeTest.load(patch, name), { patch: source, name: starter.name });
    await settle(page);
    const saved = await page.evaluate(() => window.__wirescapeTest.state());
    // Camera may be fitted by the app; native graph, routing and metadata stay exact.
    for (const field of ['modules', 'connections', 'outputs', 'settings', 'name', 'SPDX-License-Identifier']) {
      expect(saved[field]).toEqual(source[field]);
    }
    await expect(page.locator('[data-testid^="module-header-"]')).toHaveCount(source.modules.length);
    const intersections = await page.locator('.module').evaluateAll(modules => {
      const bounds = modules.map(module => ({
        id: (module as HTMLElement).dataset.id,
        box: module.getBoundingClientRect(),
      }));
      return bounds.flatMap((a, i) => bounds.slice(i + 1)
        .filter(b => a.box.left < b.box.right && a.box.right > b.box.left
          && a.box.top < b.box.bottom && a.box.bottom > b.box.top)
        .map(b => `${a.id} overlaps ${b.id}`));
    });
    expect(intersections, `${starter.id} card spacing`).toEqual([]);
    expect(errors).toEqual([]);
  });
}

test('all bundled starters place cards without overlaps in mouse and touch profiles', async ({ page }) => {
  await boot(page);
  const failures: string[] = [];
  for (const hit of ['mouse', 'touch']) {
    await page.evaluate(hit => window.__wirescapeTest.setPref({ hit }), hit);
    for (const starter of STARTERS) {
      await page.evaluate(({ patch, name }) => {
        window.__wirescapeTest.load(patch, name);
        window.__wirescapeTest.fit();
      }, { patch: starter.patch(), name: starter.name });
      await settle(page);
      const overlaps = await page.locator('.module').evaluateAll(cards => {
        const bad = [];
        for (let i = 0; i < cards.length; i++) for (let j = i + 1; j < cards.length; j++) {
          const a = cards[i]!.getBoundingClientRect(), b = cards[j]!.getBoundingClientRect();
          if (Math.min(a.right, b.right) - Math.max(a.left, b.left) > 2 &&
              Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 2) {
            bad.push(`${cards[i]!.getAttribute('data-testid')} overlaps ${cards[j]!.getAttribute('data-testid')}`);
          }
        }
        return bad;
      });
      failures.push(...overlaps.map(s => `${starter.id} / ${hit}: ${s}`));
    }
  }
  expect(failures).toEqual([]);
});
