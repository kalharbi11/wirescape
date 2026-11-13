// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const defaultRows = () => Array.from({ length: 8 }, (_, i) => ({
  pattern: `/pico1/knob/${i + 1}`, argument: 0, kind: 'cv',
  input_min: 0, input_max: 1, output_min: 0, output_max: 1,
  smoothing_ms: 5, timeout_ms: 1000, stale: 'hold',
}));
const patch = (rows = defaultRows(), connection: 'direct' | 'other' | 'none' = 'direct') => ({
  format: 'wirescape.native.patch' as const, version: 2 as const,
  modules: [
    { id: 1, module: 'osc.in', config: { port: 9000, lan: false, rows } },
    { id: 2, module: 'monitor' },
    { id: 3, module: 'essentials.output' },
    { id: 4, module: 'osc.in' },
  ],
  connections: connection === 'none' ? [] : [
    { kind: 'event' as const, from: { module: connection === 'direct' ? 1 : 4, port: 'messages' },
      to: { module: 2, port: 'events' } },
  ],
  ui: { positions: { 1: [10, 35] as [number, number], 2: [490, 35] as [number, number],
      3: [900, 35] as [number, number], 4: [1000, 300] as [number, number] },
    view: { x: 0, y: 0, zoom: 1 } },
});
const open = async (page: Page) => {
  await page.getByTestId('osc-edit-1').click();
  await expect(page.getByTestId('osc-editor')).toBeVisible();
};
const push = async (page: Page, value: number, address = '/pico1/knob/1') => {
  await page.evaluate(({ value, address }) =>
    window.__wirescapeTest.engine.setMonitorEntries(2, [
      { t: value / 10000 + 1, kind: 'osc', address, args: [value] },
    ]), { value, address });
  await settle(page);
};
test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => { window.__wirescapeTest.engine.debug.oscTraffic = false; });
});

test('inferred output mode round-trips all existing config fields unchanged', async ({ page }) => {
  for (const [min, max, mode] of [[0, 1, 'unipolar'], [-1, 1, 'bipolar'],
    [0.2, 0.8, 'custom']] as const) {
    const rows = defaultRows();
    rows[0].output_min = min; rows[0].output_max = max;
    await page.evaluate(p => window.__wirescapeTest.load(p), patch(rows));
    await settle(page);
    const before = (await state(page)).modules[0].config;
    await open(page);
    await expect(page.getByTestId('osc-row1-output-mode')).toHaveValue(mode);
    await page.getByTestId('osc-editor-save').click();
    await expect(page.getByTestId('osc-editor')).toBeHidden();
    expect((await state(page)).modules[0].config).toEqual(before);
  }
});

test('bipolar writes only the existing output range keys', async ({ page }) => {
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
  const before = (await state(page)).modules[0].config;
  await open(page);
  await page.getByTestId('osc-row1-output-mode').selectOption('bipolar');
  await expect(page.getByTestId('osc-row1-output-mode')).toHaveValue('bipolar');
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  const after = (await state(page)).modules[0].config;
  expect(after).toEqual({ ...before, rows: [
    { ...before.rows[0], output_min: -1, output_max: 1 }, ...before.rows.slice(1),
  ] });
});

test('live Set max / Set min commits two raw bounds only when saved', async ({ page }) => {
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
  await open(page);
  await push(page, 1689);
  await expect(page.getByTestId('osc-row1-raw')).toHaveText('raw 1689');
  await page.getByTestId('osc-row1-set-max').click();
  await expect(page.getByTestId('osc-row1-input_max')).toHaveValue('1689');
  expect((await state(page)).modules[0].config.rows[0].input_max).toBe(1);
  await push(page, 121);
  await page.getByTestId('osc-row1-set-min').click();
  await expect(page.getByTestId('osc-row1-input_min')).toHaveValue('121');
  await expect(page.getByTestId('osc-editor-save')).toBeEnabled();
  await page.getByTestId('osc-editor-save').click();
  await expect(page.getByTestId('osc-editor')).toBeHidden();
  expect((await state(page)).modules[0].config.rows[0]).toMatchObject({ input_min: 121, input_max: 1689 });
});

test('capture refuses missing, other-source and unsupported Monitor data', async ({ page }) => {
  for (const kind of ['none', 'other'] as const) {
    await page.evaluate(p => window.__wirescapeTest.load(p), patch(defaultRows(), kind));
    await settle(page);
    await open(page);
    await expect(page.getByTestId('osc-row1-set-min')).toBeDisabled();
    await expect(page.getByTestId('osc-row1-capture-reason')).toContainText('Patch OSC In messages');
    await page.getByTestId('osc-editor').getByRole('button', { name: 'Cancel' }).click();
  }
  const rows = defaultRows();
  rows[0].argument = 4;
  await page.evaluate(p => window.__wirescapeTest.load(p), patch(rows));
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-row1-set-min')).toBeDisabled();
  await expect(page.getByTestId('osc-row1-capture-reason')).toContainText('arguments 0–3');
  await page.getByTestId('osc-editor').getByRole('button', { name: 'Cancel' }).click();

  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
  await open(page);
  await expect(page.getByTestId('osc-row1-capture-reason')).toContainText('Move the control first');
  await page.evaluate(() => { window.__wirescapeTest.engine.monitorAvailable = false; });
  await expect(page.getByTestId('osc-row1-set-min')).toBeDisabled();
  await expect(page.getByTestId('osc-row1-capture-reason')).toContainText('not available');
});

test('off row cannot capture; invalid capture blocks Save until repaired', async ({ page }) => {
  const rows = defaultRows();
  (rows[0] as typeof rows[number] & { enabled?: boolean }).enabled = false;
  await page.evaluate(p => window.__wirescapeTest.load(p), patch(rows));
  await settle(page);
  await open(page);
  await push(page, 200);
  await expect(page.getByTestId('osc-row1-set-min')).toBeDisabled();
  await expect(page.getByTestId('osc-row1-capture-reason')).toContainText('Enable this row');
  await page.getByTestId('osc-row1-enabled').check();
  await expect(page.getByTestId('osc-row1-set-min')).toBeEnabled();
  await page.getByTestId('osc-row1-set-min').click();
  await expect(page.getByTestId('osc-editor-errors')).toContainText('input max must exceed input min');
  await expect(page.getByTestId('osc-editor-save')).toBeDisabled();
  await page.getByTestId('osc-row1-input_max').fill('201');
  await expect(page.getByTestId('osc-editor-save')).toBeEnabled();
});

test('clamp hint only reflects observed direct ingress outside configured range', async ({ page }) => {
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
  const clamp = page.getByTestId('osc-clamp-1-row1');
  await expect(clamp).toBeHidden();
  await push(page, 1689);
  await expect(clamp).toContainText('raw 1689 > max 1 (clamped)');
  await push(page, 0.5);
  await expect(clamp).toBeHidden();
  await page.evaluate(() => window.__wirescapeTest.engine.setMonitorEntries(2, []));
  await settle(page);
  await expect(clamp).toBeHidden();
});
