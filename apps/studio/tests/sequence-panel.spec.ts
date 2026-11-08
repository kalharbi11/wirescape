// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, settle, state } from './helpers';

async function sequence(page: import('@playwright/test').Page, length = 8) {
  await boot(page);
  await page.evaluate(length => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'example.step_sequencer', parameters: { length } }],
    connections: [], ui: { positions: { 1: [80, 60] }, view: { x: 0, y: 0, zoom: 1 } },
  }), length);
  await settle(page);
}

test('sequence pitch and velocity support typed values and keyboard edits, with undo and round-trip', async ({ page }) => {
  await sequence(page);
  for (const [id, value] of [['pitch1', '0.5'], ['vel1', '65%']]) {
    await page.getByTestId(`readout-1-${id}`).click();
    await page.getByTestId(`readout-input-1-${id}`).fill(value!);
    await page.getByTestId(`readout-input-1-${id}`).press('Enter');
  }
  const velocity = page.getByTestId('number-1-vel1');
  await velocity.focus();
  await velocity.press('ArrowUp');
  await settle(page);
  const parameters = (await state(page)).modules[0].parameters;
  expect(parameters.pitch1).toBeCloseTo(0.5);
  expect(parameters.vel1).toBeCloseTo(0.66);
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  const beforeGate = await state(page);
  await page.getByTestId('step-gate-1-2').click();
  await expect(page.getByTestId('step-1-2')).toHaveClass(/tied/);
  await expect(page.getByTestId('step-gate-1-2')).toHaveText('Tie');
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect(page.getByTestId('step-gate-1-2')).toHaveText('Note');
  expect((await state(page)).modules).toEqual(beforeGate.modules);
  const saved = await state(page);
  await page.evaluate(patch => window.__wirescapeTest.load(patch), saved);
  await expect(page.getByTestId('number-1-vel1')).toHaveAttribute('aria-valuenow', '0.66');
});

test('shortened sequence maps engine phase to its actual length and stop clears the playhead', async ({ page }) => {
  await sequence(page, 4);
  await expect(page.getByTestId('step-1-5')).toHaveClass(/outside-length/);
  await expect(page.getByTestId('step-1-4')).toHaveClass(/playing/, { timeout: 5000 });
  expect(await page.locator('.outside-length.playing').count()).toBe(0);
  await page.getByTestId('toggle-1-run').click();
  await expect(page.locator('.step-cell.playing')).toHaveCount(0);
  // Gate state is an editable neutral state, independent of transport activity.
  await page.getByTestId('step-gate-1-3').click();
  await expect(page.getByTestId('step-gate-1-3')).toHaveText('Tie');
  await page.getByTestId('step-gate-1-3').click();
  await expect(page.getByTestId('step-gate-1-3')).toHaveText('Rest');
});
