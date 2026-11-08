// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

async function loadSequence(page: Page) {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'example.step_sequencer' }], connections: [],
    ui: { positions: { 1: [80, 60] }, view: { x: 0, y: 0, zoom: 1 } },
  }));
  await settle(page);
}

test('a new pointer gesture preserves the pending keyboard edit as its own undo step', async ({ page }) => {
  await loadSequence(page);
  // Dispatch in one task so this specifically exercises edits inside the
  // 350 ms key debounce, independent of test-machine/browser speed.
  await page.evaluate(() => {
    const pitch = document.querySelector<HTMLElement>('[data-testid="number-1-pitch1"]')!;
    pitch.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
    const gate = document.querySelector<HTMLButtonElement>('[data-testid="step-gate-1-1"]')!;
    gate.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, pointerId: 1 }));
    gate.dispatchEvent(new PointerEvent('pointerup', { bubbles: true, pointerId: 1 }));
    gate.click();
  });
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(2);
  await page.evaluate(() => window.__wirescapeTest.undo());
  const parameters = (await state(page)).modules[0].parameters;
  expect(parameters.pitch1).toBeCloseTo(0.04);
  expect(parameters.gate1 ?? 1).toBe(1);
  await page.evaluate(() => window.__wirescapeTest.undo());
  expect((await state(page)).modules[0].parameters?.pitch1 ?? 0).toBe(0);
});

test('rapid separate gate clicks create separate undo steps and commit at gesture end', async ({ page }) => {
  await loadSequence(page);
  await page.evaluate(() => {
    for (const step of [1, 2]) {
      const gate = document.querySelector<HTMLButtonElement>(`[data-testid="step-gate-1-${step}"]`)!;
      gate.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, pointerId: 1 }));
      gate.dispatchEvent(new PointerEvent('pointerup', { bubbles: true, pointerId: 1 }));
      gate.click();
    }
  });
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(2);
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect(page.getByTestId('step-gate-1-1')).toHaveText('Tie');
  await expect(page.getByTestId('step-gate-1-2')).toHaveText('Note');
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect(page.getByTestId('step-gate-1-1')).toHaveText('Note');
});
