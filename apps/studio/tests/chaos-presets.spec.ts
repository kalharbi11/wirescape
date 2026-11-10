// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const menu = (page: Page) => page.getByTestId('module-presets-1');
const values = async (page: Page) => (await state(page)).modules.find((m: any) => m.id === 1)?.parameters ?? {};
const flush = (page: Page) => page.evaluate(() => window.__wirescapeTest.flushHistory());

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.chaos_lite'));
  await settle(page);
});

test('eight CHAOS presets are a UI-only header control, not module metadata', async ({ page }) => {
  await expect(menu(page)).toHaveCount(1);
  await expect(menu(page).locator('option')).toHaveText([
    'Custom', 'Warm fifth', 'Bent wire', 'Broken bell', 'Slow weather',
    'Basalt drone', 'Glass pluck', 'Iron rain', 'Tide choir',
  ]);
  const saved = await state(page);
  expect(saved.modules[0].module).toBe('essentials.chaos_lite');
  expect(JSON.stringify(saved)).not.toContain('modulePresets');
  expect(JSON.stringify(saved)).not.toContain('Warm fifth');
});

test('preset resets every omitted parameter, saves/reloads and is exactly one Undo', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.commitParam(1, 'cutoff', 1337));
  await flush(page);
  const previous = await values(page);
  const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
  await menu(page).selectOption('Bent wire');
  await flush(page);
  const next = await values(page);
  expect(next.route).toBe(2);
  expect(next.depth).toBe(0.8);
  expect(next.feedback).toBe(0.7);
  expect(next.cutoff).toBe(9000); // descriptor default, not the prior manual setting
  expect(Object.keys(next)).toHaveLength(26);
  expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth + 1);
  await page.getByTestId('undo').click();
  await expect.poll(async () => await values(page)).toEqual(previous);
  await page.getByTestId('redo').click();
  await expect.poll(async () => (await values(page)).cutoff).toBe(9000);
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(p => window.__wirescapeTest.load(JSON.parse(p), 'Reloaded'), saved);
  await settle(page);
  expect(JSON.stringify(await state(page))).toBe(saved);
  await expect(menu(page)).toHaveValue('Bent wire');
});

test('manual parameter changes show Custom without writing preset names', async ({ page }) => {
  await menu(page).selectOption('Warm fifth');
  await flush(page);
  await expect(menu(page)).toHaveValue('Warm fifth');
  await page.evaluate(() => window.__wirescapeTest.commitParam(1, 'body_mix', 0.42));
  await expect(menu(page)).toHaveValue('');
  const saved = JSON.stringify(await state(page));
  expect(saved).not.toContain('Warm fifth');
  expect(saved).not.toContain('Custom');
  expect((await values(page)).body_mix).toBe(0.42);
});

test('all presets use finite descriptor-bounded values, preserving unknown module data and cables', async ({ page }) => {
  const base = await state(page);
  base.modules[0].futureField = { unsupported: true };
  base.modules[0].parameters = { futuristic: 0.314 };
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Future patch'), base);
  await settle(page);
  const names = ['Warm fifth', 'Bent wire', 'Broken bell', 'Slow weather',
    'Basalt drone', 'Glass pluck', 'Iron rain', 'Tide choir'];
  const descriptor = await page.evaluate(() => window.__wirescapeTest.registry.get('essentials.chaos_lite'));
  for (const name of names) {
    await menu(page).selectOption(name);
    const p = await values(page);
    for (const param of descriptor.parameters) {
      expect(Number.isFinite(p[param.id]), name + ':' + param.id).toBe(true);
      expect(p[param.id], name + ':' + param.id).toBeGreaterThanOrEqual(param.minimum);
      expect(p[param.id], name + ':' + param.id).toBeLessThanOrEqual(param.maximum);
    }
    expect(p.futuristic).toBe(0.314);
    const m = (await state(page)).modules[0];
    expect(m.futureField).toEqual({ unsupported: true });
    expect((await state(page)).connections).toEqual(base.connections);
  }
});
