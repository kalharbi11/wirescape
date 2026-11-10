// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import { boot, settle, state } from './helpers';

test('CHAOS performance strip edits exact values with one Undo and stays in sync', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.chaos_lite'));
  await settle(page);
  await page.evaluate(() => window.__wirescapeTest.flushHistory());

  const strip = page.getByTestId('chaos-performance-strip-1');
  await expect(strip).toBeVisible();
  await expect(strip.locator('.chaos-performance-title')).toHaveText([
    'Tone', 'Motion', 'Edge', 'Body', 'Brightness',
  ]);
  const ids = [
    'shape_a', 'shape_b', 'detune', 'drift', 'lfo_depth', 'lfo_rate',
    'lfo_shape', 'roughness', 'depth', 'body_mix', 'feedback', 'cutoff',
  ];
  for (const id of ids) await expect(strip.getByTestId(`performance-knob-1-${id}`)).toHaveCount(1);
  await expect(page.locator('.panel-controls .section-label')).toHaveText([
    '01 / OSCILLATORS', '02 / BALANCE', '03 / BODY',
    '04 / CHARACTER', '05 / INTERNAL LFO + OUT', '06 / ROUTING',
  ]);

  const before = await state(page);
  const past = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
  // The compact knob's readout uses the same typed-value and commitParam path as the full knob.
  await strip.getByTestId('performance-readout-1-cutoff').click();
  const input = strip.getByTestId('performance-readout-input-1-cutoff');
  await input.fill('1337');
  await input.press('Enter');
  await page.evaluate(() => window.__wirescapeTest.flushHistory());

  await expect(strip.getByTestId('performance-readout-1-cutoff')).toHaveText(/^1337(?: Hz)?$/);
  await expect(strip.getByTestId('performance-knob-1-cutoff').locator('.knob'))
    .toHaveAttribute('aria-valuenow', '1337');
  await expect(page.getByTestId('knob-1-cutoff').locator('.knob'))
    .toHaveAttribute('aria-valuenow', '1337');
  expect((await state(page)).modules[0].parameters.cutoff).toBe(1337);
  expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(past + 1);

  await page.getByTestId('undo').click();
  await expect.poll(async () => state(page)).toEqual(before);
  await expect(strip.getByTestId('performance-knob-1-cutoff').locator('.knob'))
    .not.toHaveAttribute('aria-valuenow', '1337');
  await page.getByTestId('redo').click();
  await expect.poll(async () => (await state(page)).modules[0].parameters.cutoff).toBe(1337);

  // Editing the full-panel knob refreshes its compact twin, without a macro fan-out.
  await page.getByTestId('readout-1-drift').click();
  const fullInput = page.getByTestId('readout-input-1-drift');
  await fullInput.fill('0.42');
  await fullInput.press('Enter');
  await expect(strip.getByTestId('performance-readout-1-drift')).toHaveText('0.42');
  expect((await state(page)).modules[0].parameters.cutoff).toBe(1337);
});
