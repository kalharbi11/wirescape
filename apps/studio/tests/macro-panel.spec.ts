import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

async function loadMacro(page: Page): Promise<void> {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'ports.macro_oscillator', parameters: { model: 0 } }],
    connections: [], ui: { positions: { 1: [60, 40] }, view: { x: 0, y: 0, zoom: 1 } },
  }, 'Macro'));
  await settle(page);
}

const model = async (page: Page) => (await state(page)).modules[0].parameters.model;

test('macro models select, undo and persist through patch reload', async ({ page }) => {
  await loadMacro(page);
  await expect(page.getByTestId('models-1').getByRole('radio')).toHaveCount(24);
  await page.getByTestId('model-1-23').click();
  expect(await model(page)).toBe(23);
  await expect(page.getByTestId('model-1-23')).toHaveAttribute('aria-checked', 'true');
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect(page.getByTestId('model-1-0')).toHaveAttribute('aria-checked', 'true');
  await page.evaluate(() => window.__wirescapeTest.redo());
  const saved = await state(page);
  await page.evaluate(patch => window.__wirescapeTest.load(patch), saved);
  expect(await model(page)).toBe(23);
  await expect(page.getByTestId('model-1-23')).toHaveAttribute('aria-checked', 'true');
  await expect(page.getByTestId('knob-1-harmonics')).toHaveClass(/tier-L/);
  await expect(page.getByTestId('knob-1-timbre')).toHaveClass(/tier-M/);
  await expect(page.getByTestId('knob-1-fm_amount')).toHaveClass(/tier-S/);
});

test('macro model bank works with keyboard and touch targets fit the card', async ({ page }) => {
  await loadMacro(page);
  await page.getByTestId('model-1-0').focus();
  await page.keyboard.press('ArrowDown');
  expect(await model(page)).toBe(8);
  await expect(page.getByTestId('model-1-8')).toBeFocused();
  await page.keyboard.press('End');
  expect(await model(page)).toBe(23);
  await page.keyboard.press('ArrowRight');
  expect(await model(page)).toBe(23);
  await page.keyboard.press('Home');
  expect(await model(page)).toBe(0);
  await page.evaluate(() => window.__wirescapeTest.setPref({ hit: 'touch' }));
  await settle(page);
  const geometry = await page.getByTestId('models-1').evaluate(root => {
    const card = root.closest('[data-testid="module-1"]')!;
    const r = root.getBoundingClientRect(), b = card.getBoundingClientRect();
    const button = root.querySelector('button')!;
    return { fits: r.right <= b.right, width: button.offsetWidth, height: button.offsetHeight };
  });
  expect(geometry.fits).toBe(true);
  expect(geometry.width).toBeGreaterThanOrEqual(44);
  expect(geometry.height).toBeGreaterThanOrEqual(44);
});
