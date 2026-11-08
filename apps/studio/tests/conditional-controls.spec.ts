// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, knob, state } from './helpers';

// Use an existing module so this contract does not depend on Spectral landing.
test('mode visibility retains parameter values and modulation cables', async ({ page }) => {
  await page.route('**/wirescape_modules.json', async route => {
    const response = await route.fetch();
    const data = await response.json();
    const lfo = data.modules.find((m: any) => m.id === 'essentials.lfo');
    lfo.parameters.find((p: any) => p.id === 'depth').visibleWhen = { param: 'sync', values: [1] };
    await route.fulfill({ response, json: data });
  });
  await page.route('**/panels/essentials.lfo.json', route => route.fulfill({ json: {
    sections: [{ label: 'GATED DEPTH', params: ['depth'] }, { params: ['sync'] }],
  } }));
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'essentials.lfo', parameters: { sync: 0, depth: 0.37 } },
      { id: 2, module: 'essentials.lfo' }],
    connections: [{ from: { module: 2, port: 'control' }, to: { module: 1, port: 'depth' },
      kind: 'modulation', amount: 0.25 }],
    ui: { positions: { 1: [100, 100], 2: [1000, 100] }, view: { x: 0, y: 0, zoom: 0.7 } },
  }));
  const depth = knob(page, 1, 'depth');
  const label = page.getByTestId('module-1').getByText('GATED DEPTH', { exact: true });
  await expect(depth).toBeHidden();
  await expect(label).toBeHidden();
  const saved = await state(page);
  const height = await page.getByTestId('module-1').evaluate(e => e.getBoundingClientRect().height);
  await page.getByTestId('toggle-1-sync').click();
  await expect(depth).toBeVisible();
  await expect(label).toBeVisible();
  expect(await page.getByTestId('module-1').evaluate(e => e.getBoundingClientRect().height)).toBeGreaterThan(height);
  await expect(depth).toHaveAttribute('data-mod-count', '1');
  await page.getByTestId('toggle-1-sync').click();
  await expect(depth).toBeHidden();
  const after = await state(page);
  expect(after.connections).toEqual(saved.connections);
  expect(after.modules[0].parameters.depth).toBe(0.37);
  await page.evaluate(p => window.__wirescapeTest.load(p), after);
  await expect(depth).toBeHidden();
  await page.getByTestId('toggle-1-sync').click();
  await expect(depth).toBeVisible();
  expect((await state(page)).connections).toEqual(saved.connections);
});
