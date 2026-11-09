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

// Exercise the shipped 50-parameter catalogue: no test-only descriptor overrides.
test('16-step editor uses real catalogue; high steps, length, undo/redo and patch reload work', async ({ page }) => {
  await sequence(page, 16);
  await expect(page.getByTestId('step-1-9')).toBeVisible();
  await expect(page.getByTestId('step-1-16')).toBeVisible();
  const grid = page.getByTestId('steps-1').locator('.steps-grid');
  await expect(grid).toHaveAttribute('aria-label', '16-step pattern');
  expect(await grid.evaluate(e => getComputedStyle(e).gridTemplateColumns.split(' ').length)).toBe(8);

  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Rest');
  // Each change must be a distinct reversible edit. The first eight IDs
  // remain stable and edits in the upper bank use the same history path.
  await page.getByTestId('readout-1-pitch16').click();
  await page.getByTestId('readout-input-1-pitch16').fill('0.5');
  await page.getByTestId('readout-input-1-pitch16').press('Enter');
  await page.getByTestId('readout-1-vel16').click();
  await page.getByTestId('readout-input-1-vel16').fill('65%');
  await page.getByTestId('readout-input-1-vel16').press('Enter');
  await page.getByTestId('step-gate-1-16').click();
  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Note');
  await page.getByTestId('step-gate-1-16').click();
  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Tie');
  await page.getByTestId('readout-1-length').click();
  await page.getByTestId('readout-input-1-length').fill('12');
  await page.getByTestId('readout-input-1-length').press('Enter');
  await page.evaluate(() => window.__wirescapeTest.flushHistory());

  const after = await state(page);
  const edited = after.modules[0].parameters;
  expect(edited.pitch16).toBeCloseTo(0.5);
  expect(edited.vel16).toBeCloseTo(0.65);
  expect(edited.gate16).toBe(2);
  expect(edited.length).toBe(12);
  await expect(page.getByTestId('step-1-16')).toHaveClass(/outside-length/);
  await expect(page.getByTestId('step-1-12')).not.toHaveClass(/outside-length/);

  await page.evaluate(() => window.__wirescapeTest.undo()); // length 12 -> 16
  expect((await state(page)).modules[0].parameters.length).toBe(16);
  await expect(page.getByTestId('step-1-16')).not.toHaveClass(/outside-length/);
  await page.evaluate(() => window.__wirescapeTest.undo()); // gate tie -> note
  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Note');
  await page.evaluate(() => window.__wirescapeTest.undo()); // gate note -> rest
  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Rest');
  await page.evaluate(() => window.__wirescapeTest.undo()); // velocity -> default
  expect((await state(page)).modules[0].parameters.vel16 ?? 0.8).toBeCloseTo(0.8);
  await page.evaluate(() => window.__wirescapeTest.undo()); // pitch -> default
  expect((await state(page)).modules[0].parameters.pitch16 ?? 0).toBeCloseTo(0);

  for (let i = 0; i < 5; ++i) await page.evaluate(() => window.__wirescapeTest.redo());
  const redone = (await state(page)).modules[0].parameters;
  expect(redone.pitch16).toBeCloseTo(0.5);
  expect(redone.vel16).toBeCloseTo(0.65);
  expect(redone.gate16).toBe(2);
  expect(redone.length).toBe(12);

  // Patch round-trip must retain edited high-step values even when inactive.
  await page.evaluate(patch => window.__wirescapeTest.load(patch), after);
  const loaded = (await state(page)).modules[0].parameters;
  expect(loaded.pitch16).toBeCloseTo(0.5);
  expect(loaded.vel16).toBeCloseTo(0.65);
  await expect(page.getByTestId('step-gate-1-16')).toHaveText('Tie');
  await expect(page.getByTestId('step-1-16')).toHaveClass(/outside-length/);
});

// FakeEngine is the UI fixture; DSP/render correctness belongs to the native/WASM gates.
test('touch editor keeps two rows, distinct controls and keyboard focus', async ({ page }, info) => {
  const errors: string[] = [];
  page.on('pageerror', error => errors.push(error.message));
  await sequence(page, 16);
  await page.evaluate(() => {
    window.__wirescapeTest.setPref({ hit: 'touch' });
    window.__wirescapeTest.zoomTo(1);
  });
  await settle(page);
  const first = await page.getByTestId('step-1-1').boundingBox();
  const eighth = await page.getByTestId('step-1-8').boundingBox();
  const ninth = await page.getByTestId('step-1-9').boundingBox();
  const last = await page.getByTestId('step-1-16').boundingBox();
  expect(first && eighth && ninth && last).toBeTruthy();
  expect(first!.y).toBeCloseTo(eighth!.y);
  expect(ninth!.y).toBeCloseTo(last!.y);
  expect(ninth!.y).toBeGreaterThanOrEqual(first!.y + first!.height);
  const gate = page.getByTestId('step-gate-1-16');
  expect((await gate.boundingBox())!.height).toBeGreaterThanOrEqual(44);
  await gate.focus();
  await expect(gate).toBeFocused();
  await gate.press('Space');
  await expect(gate).toHaveText('Note');
  expect(errors).toEqual([]);
  await page.screenshot({ path: info.outputPath('seq16-touch.png') });
});
