// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const patch = (config?: unknown) => ({
  format: 'wirescape.native.patch' as const, version: 2 as const,
  ownerExtension: { survive: 1 },
  modules: [{ id: 1, module: 'osc.in', ownerNote: 'keep me', ...(config === undefined ? {} : { config }) }],
  connections: [],
  ui: { positions: { '1': [48, 30] as [number, number] }, view: { x: 0, y: 0, zoom: 1 } },
});

async function open(page: Page): Promise<void> {
  await page.getByTestId('osc-edit-1').click();
  await expect(page.getByTestId('osc-editor')).toBeVisible();
}

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
});

test('OSC In card shows actual route state, received counter and activity', async ({ page }, testInfo) => {
  await page.evaluate(() => { window.__wirescapeTest.engine.debug.oscTraffic = false; });
  await expect(page.getByTestId('osc-status-1')).toHaveText('Listening on 127.0.0.1:9000');
  await expect(page.getByTestId('osc-received-1')).toHaveText('Received: 0');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscReceived(1, 8));
  await expect(page.getByTestId('osc-received-1')).toHaveText('Received: 8');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscRouteState(1, 'port-busy'));
  await expect(page.getByTestId('osc-status-1')).toContainText('Port busy');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscRouteState(1, 'lan-not-confirmed'));
  await expect(page.getByTestId('osc-status-1')).toContainText('LAN not confirmed');
  await page.getByTestId('module-1').screenshot({ path: testInfo.outputPath('osc-in-card.png') });
  await testInfo.attach('OSC In card', { path: testInfo.outputPath('osc-in-card.png'), contentType: 'image/png' });
});

test('editor round-trips eight rows with stable module ID and patch extras', async ({ page }, testInfo) => {
  await open(page);
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/pico1/knob/8');
  await page.getByTestId('osc-editor').locator('.modal').screenshot({
    path: testInfo.outputPath('osc-in-editor.png'),
  });
  await testInfo.attach('OSC In editor', { path: testInfo.outputPath('osc-in-editor.png'), contentType: 'image/png' });
  await page.getByTestId('osc-row1-pattern').fill('/show/voice/*');
  await page.getByTestId('osc-row1-kind').selectOption('gate');
  await page.getByTestId('osc-row1-argument').fill('1');
  await page.getByTestId('osc-row1-input_max').fill('2');
  await page.getByTestId('osc-editor-port').fill('9001');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.modules[0]).toMatchObject({ id: 1, ownerNote: 'keep me' });
  expect(saved.ownerExtension).toEqual({ survive: 1 });
  expect(saved.modules[0].config).toMatchObject({
    port: 9001, lan: false, rows: [{ pattern: '/show/voice/*', kind: 'gate', argument: 1, input_max: 2 }],
  });
  expect(saved.modules[0].config.rows).toHaveLength(8);
  await open(page);
  await expect(page.getByTestId('osc-row1-pattern')).toHaveValue('/show/voice/*');
  await expect(page.getByTestId('osc-editor-port')).toHaveValue('9001');
});

test('invalid pattern, range and port are refused without modifying authored config', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-row1-pattern').fill('bad/without/slash');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('address must start');
  expect((await state(page)).modules[0].config).toBeUndefined();
  await page.getByTestId('osc-row1-pattern').fill('/valid');
  await page.getByTestId('osc-row1-input_min').fill('5');
  await page.getByTestId('osc-row1-input_max').fill('4');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('input max');
  await page.getByTestId('osc-row1-input_max').fill('7');
  await page.getByTestId('osc-editor-port').fill('99');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('UDP port');
  expect((await state(page)).modules[0].config).toBeUndefined();
});

test('presets fill all rows, while config unknowns only drop on editor save', async ({ page }) => {
  const original = patch({ port: 9011, futureOption: 123, rows: [{ pattern: '/old', futureField: true }] });
  await page.evaluate(p => window.__wirescapeTest.load(p), original);
  await settle(page);
  expect((await state(page)).modules[0].config.futureOption).toBe(123);
  await open(page);
  await expect(page.getByTestId('osc-unknown-warning')).toContainText('engine will refuse: futureOption, rows[0].futureField');
  expect((await state(page)).modules[0].config.futureOption).toBe(123);
  await page.getByTestId('osc-preset-touchosc').click();
  await expect(page.getByTestId('osc-row1-pattern')).toHaveValue('/1/fader1');
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/1/fader8');
  await page.getByTestId('osc-preset-reset').click();
  await expect(page.getByTestId('osc-row8-pattern')).toHaveValue('/pico1/knob/8');
  await page.getByTestId('osc-preset-touchosc').click();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.modules[0].config.futureOption).toBeUndefined();
  expect(saved.modules[0].config.rows[0].futureField).toBeUndefined();
  expect(saved.modules[0].config.rows[7].pattern).toBe('/1/fader8');
  expect(saved.modules[0].ownerNote).toBe('keep me');
  expect(saved.ownerExtension).toEqual({ survive: 1 });
});

test('LAN checkbox requests a route but cannot authorize a LAN socket', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-editor-lan').check();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-status-1')).toContainText('LAN not confirmed');
  expect((await state(page)).modules[0].config.lan).toBe(true);
});
