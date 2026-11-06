import { test, expect } from '@playwright/test';
import { boot, state } from './helpers';

test('releasing a held key after an octave change releases its original note', async ({ page }) => {
  await boot(page);
  await page.keyboard.down('a');
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes)).toContain(48);
  await page.keyboard.press('x');
  await page.keyboard.up('a');
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes)).toEqual([]);
});

test('settings traps focus, restores it, and blocks background editing and notes', async ({ page }) => {
  await boot(page, { blank: false });
  const before = await state(page);
  const opener = page.getByTestId('open-settings');
  await opener.click();
  const dialog = page.getByTestId('settings-modal');
  const close = page.getByTestId('settings-modal-close');
  await expect(close).toBeFocused();
  await page.keyboard.press('Shift+Tab');
  await expect(dialog.getByTestId('open-debug')).toBeFocused();
  await page.keyboard.press('Tab');
  await expect(close).toBeFocused();
  await page.keyboard.press('Control+a');
  await page.keyboard.press('Delete');
  await page.keyboard.press('a');
  expect((await state(page)).modules).toEqual(before.modules);
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes)).toEqual([]);
  await page.keyboard.press('Escape');
  await expect(dialog).toBeHidden();
  await expect(opener).toBeFocused();
});
