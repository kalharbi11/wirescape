// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

async function loadQuad(page: Page): Promise<void> {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'essentials.quad_vca',
      parameters: { level1: 0.65, level2: 0.5, level3: 0.4, level4: 0.3, response: 0.48 } }],
    connections: [],
    ui: { positions: { 1: [70, 70] }, view: { x: 0, y: 0, zoom: 0.75 } },
  }, 'Quad VCA'));
  await settle(page);
}

const params = async (page: Page) => (await state(page)).modules.find((m: any) => m.id === 1).parameters;

test('Quad VCA card has four level/mode columns, shared response and all thirteen jacks', async ({ page }) => {
  await loadQuad(page);
  const card = page.getByTestId('module-1');
  await expect(card).toContainText('Quad VCA');
  for (let n = 1; n <= 4; n++) {
    await expect(page.getByTestId(`knob-1-level${n}`)).toBeVisible();
    await expect(page.getByTestId(`segmented-1-mode${n}`)).toBeVisible();
    await expect(page.getByTestId(`port-1-input-in${n}`)).toBeVisible();
    await expect(page.getByTestId(`port-1-input-cv${n}`)).toBeVisible();
    await expect(page.getByTestId(`port-1-output-out${n}`)).toBeVisible();
  }
  await expect(page.getByTestId('port-1-output-mix')).toBeVisible();
  await expect(page.getByTestId('knob-1-response')).toBeVisible();
  const rows = card.locator('.panel-controls > .panel-row');
  await expect(rows).toHaveCount(3);
  await expect(rows.nth(0).locator('.knob-unit')).toHaveCount(4);
  await expect(rows.nth(1).locator('.segmented-row')).toHaveCount(4);
  await expect(rows.nth(2).locator('.knob-unit')).toHaveCount(1);
});

test('VCA/LPG segmented switch commits through FakeEngine and survives save/reload', async ({ page }) => {
  await loadQuad(page);
  await expect(page.getByTestId('seg-1-mode2-0')).toHaveAttribute('aria-checked', 'true');
  await page.getByTestId('seg-1-mode2-1').click();
  await expect(page.getByTestId('seg-1-mode2-1')).toHaveAttribute('aria-checked', 'true');
  await expect.poll(async () => (await params(page)).mode2).toBe(1);
  await page.getByTestId('seg-1-mode4-1').click();
  await expect.poll(async () => (await params(page)).mode4).toBe(1);
  const saved = await state(page);
  await page.evaluate(patch => window.__wirescapeTest.load(patch), saved);
  await settle(page);
  expect(await params(page)).toMatchObject({ level1: 0.65, level2: 0.5, response: 0.48, mode2: 1, mode4: 1 });
  await expect(page.getByTestId('seg-1-mode2-1')).toHaveAttribute('aria-checked', 'true');
  await expect(page.getByTestId('seg-1-mode4-1')).toHaveAttribute('aria-checked', 'true');
  await expect(page.getByTestId('port-1-input-cv4')).toBeVisible();
});
