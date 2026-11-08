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

test('Quantizer Custom notes retain saved values through scale changes and reload', async ({ page }) => {
  await boot(page);
  const notes = Object.fromEntries(Array.from({ length: 12 }, (_, i) => [`n${i}`, i % 2]));
  await page.evaluate(parameters => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'essentials.quantizer', parameters: { scale: 1, ...parameters } }],
    connections: [],
    ui: { positions: { 1: [100, 100] }, view: { x: 0, y: 0, zoom: 0.7 } },
  }), notes);
  const scale = page.getByTestId('select-1-scale');
  const expectNotes = async (visible: boolean) => {
    for (let i = 0; i < 12; ++i) {
      const note = knob(page, 1, `n${i}`);
      if (visible) await expect(note).toBeVisible();
      else await expect(note).toBeHidden();
    }
  };
  const expectSavedNotes = async () => {
    const parameters = (await state(page)).modules[0].parameters;
    for (const [id, value] of Object.entries(notes)) expect(parameters[id]).toBe(value);
  };
  await expectNotes(false); // Major
  await scale.selectOption('13'); // Custom
  await expectNotes(true);
  await expectSavedNotes();
  await scale.selectOption('1'); // Major
  await expectNotes(false);
  await expectSavedNotes();
  const saved = await state(page);
  await page.evaluate(patch => window.__wirescapeTest.load(patch), saved);
  await expectNotes(false);
  await scale.selectOption('13');
  await expectNotes(true);
  await expectSavedNotes();
});
