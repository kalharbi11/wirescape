// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const patch = (config?: unknown) => ({
  format: 'wirescape.native.patch' as const, version: 2 as const,
  futurePatch: { keep: true },
  modules: [{ id: 1, module: 'osc.out', ownerNote: 'keep',
    ...(config === undefined ? {} : { config }) }],
  connections: [],
  ui: { positions: { '1': [48, 30] as [number, number] },
    view: { x: 0, y: 0, zoom: 1 } },
});
const open = async (page: Page) => {
  await page.getByTestId('osc-out-edit-1').click();
  await expect(page.getByTestId('osc-editor')).toBeVisible();
};

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
});

test('OSC Out fake catalogue, channel-0 warning and sender counters', async ({ page }) => {
  await expect(page.getByTestId('library-item-osc.out')).toBeVisible();
  await expect(page.getByTestId('module-1')).toContainText('Sends to this computer (127.0.0.1) only');
  await expect(page.getByTestId('module-1')).toContainText('FIRST');
  await expect(page.getByTestId('osc-out-status-1')).toContainText('Sending to 127.0.0.1:9001');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscOutStatus(1, 23, 2));
  await expect(page.getByTestId('osc-out-count-1')).toHaveText('Sent: 23 · Dropped: 2');
  await page.evaluate(() => window.__wirescapeTest.engine.setOscOutStatus(1, 23, 3, 'error'));
  await expect(page.getByTestId('osc-out-status-1')).toContainText('OSC Out error');
});

test('edits eight send rows and notes with lossless unknown fields after reload', async ({ page }) => {
  const original = patch({
    version: 1, port: 9011, rate_hz: 25, futureOption: { x: 1 },
    rows: [{ address: '/custom/one', futureRow: ['a'], enabled: false }],
    notes: { address: '/note/custom', futureNote: 42 },
  });
  await page.evaluate(p => window.__wirescapeTest.load(p), original);
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-out-row1-address')).toHaveValue('/custom/one');
  await expect(page.getByTestId('osc-out-row8-address')).toHaveValue('/ws/row/8');
  await page.getByTestId('osc-out-row1-enabled').check();
  await page.getByTestId('osc-out-row1-kind').selectOption('gate');
  await page.getByTestId('osc-out-row1-deadband').fill('0.02');
  await page.getByTestId('osc-out-row8-address').fill('/custom/eight');
  await page.getByTestId('osc-out-port').fill('9200');
  await page.getByTestId('osc-out-rate').fill('200');
  await page.getByTestId('osc-out-notes-enabled').check();
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const saved = await state(page);
  expect(saved.futurePatch).toEqual({ keep: true });
  expect(saved.modules[0].ownerNote).toBe('keep');
  expect(saved.modules[0].config).toMatchObject({
    version: 1, port: 9200, rate_hz: 200, futureOption: { x: 1 },
    notes: { address: '/note/custom', futureNote: 42, enabled: true },
  });
  expect(saved.modules[0].config.rows).toHaveLength(8);
  expect(saved.modules[0].config.rows[0]).toMatchObject({
    address: '/custom/one', kind: 'gate', deadband: 0.02, futureRow: ['a'],
  });
  expect(saved.modules[0].config.rows[0].enabled).toBeUndefined(); // true is omitted
  expect(saved.modules[0].config.rows[7].address).toBe('/custom/eight');
  await page.evaluate(p => window.__wirescapeTest.load(p), saved);
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-out-row1-kind')).toHaveValue('gate');
  await expect(page.getByTestId('osc-out-notes-enabled')).toBeChecked();
});

test('invalid address, port, rate and deadband never touch the authored document', async ({ page }) => {
  await open(page);
  await page.getByTestId('osc-out-row1-address').fill('/invalid/*');
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-out-errors')).toContainText('row1: address');
  expect((await state(page)).modules[0].config).toBeUndefined();
  await page.getByTestId('osc-out-row1-address').fill('/valid');
  await page.getByTestId('osc-out-port').fill('0');
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-out-errors')).toContainText('Port must');
  await page.getByTestId('osc-out-port').fill('9001');
  await page.getByTestId('osc-out-rate').fill('501');
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-out-errors')).toContainText('send rate');
  await page.getByTestId('osc-out-rate').fill('100');
  await page.getByTestId('osc-out-row1-deadband').fill('-0.1');
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-out-errors')).toContainText('deadband');
  expect((await state(page)).modules[0].config).toBeUndefined();
});

test('unsupported version is read-only; all-disabled graph reports disabled', async ({ page }) => {
  const future = patch({ version: 2, host: 'example.org', port: 9001 });
  await page.evaluate(p => window.__wirescapeTest.load(p), future);
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-editor')).toContainText('Unsupported OSC Out version');
  await page.getByTestId('osc-out-save').click();
  await expect(page.getByTestId('osc-out-errors')).toContainText('Unsupported');
  expect((await state(page)).modules[0].config).toEqual(future.modules[0].config);
  await page.getByTestId('osc-editor').getByRole('button', { name: 'Cancel' }).click();

  await page.evaluate(p => window.__wirescapeTest.load(p),
    patch({ rows: Array.from({ length: 8 }, () => ({ enabled: false })) }));
  await settle(page);
  await expect(page.getByTestId('osc-out-status-1')).toHaveText('OSC Out disabled');
});
