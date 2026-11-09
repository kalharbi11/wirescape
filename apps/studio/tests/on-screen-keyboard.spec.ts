// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

const notes = (page: Page) => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes ?? []);
async function press(page: Page, semi = 0): Promise<void> {
  await page.getByTestId(`piano-key-${semi}`).hover();
  await page.mouse.down();
}
async function show(page: Page): Promise<void> {
  await page.getByTestId('toggle-keyboard').click();
  await expect(page.getByTestId('on-screen-keyboard')).toBeVisible();
}

test('keyboard starts hidden; a mouse note releases even outside the key', async ({ page }) => {
  await boot(page);
  await expect(page.getByTestId('on-screen-keyboard')).toBeHidden();
  await expect(page.getByTestId('toggle-keyboard')).toHaveAttribute('aria-expanded', 'false');
  await show(page);
  await expect(page.getByTestId('toggle-keyboard')).toHaveAttribute('aria-expanded', 'true');
  await press(page);
  await expect.poll(() => notes(page)).toEqual([48]);
  await expect(page.getByTestId('piano-key-0')).toHaveAttribute('aria-pressed', 'true');
  await page.mouse.move(5, 5);
  await page.mouse.up();
  await expect.poll(() => notes(page)).toEqual([]);
});

test('computer and screen inputs share a pitch until both release it', async ({ page }) => {
  await boot(page);
  await show(page);
  await page.keyboard.down('a');
  await press(page);
  await page.keyboard.up('a');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.mouse.up();
  await expect.poll(() => notes(page)).toEqual([]);
  await page.keyboard.down('a');
  await press(page);
  await page.mouse.up();
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.keyboard.up('a');
  await expect.poll(() => notes(page)).toEqual([]);
});

test('octave controls agree with Z/X and held notes release at their original pitch', async ({ page }) => {
  await boot(page);
  await show(page);
  await press(page);
  await page.keyboard.press('x');
  await expect(page.getByTestId('piano-key-0')).toHaveAttribute('aria-label', 'Play C5');
  await expect(page.getByTestId('octave')).toHaveText('OCT 5');
  await page.mouse.up();
  await expect.poll(() => notes(page)).toEqual([]);
  await page.getByTestId('keyboard-octave-down').click();
  await expect(page.getByTestId('octave')).toHaveText('OCT 4');
  for (let i = 0; i < 4; ++i) await page.getByTestId('keyboard-octave-down').click();
  await expect(page.getByTestId('keyboard-octave-down')).toBeDisabled();
  for (let i = 0; i < 8; ++i) await page.getByTestId('keyboard-octave-up').click();
  await expect(page.getByTestId('keyboard-octave-up')).toBeDisabled();
});

test('hiding releases only screen input and focused-key note sources overlap safely', async ({ page }) => {
  await boot(page);
  await show(page);
  await page.keyboard.down('a');
  await press(page);
  await page.getByTestId('toggle-keyboard').dispatchEvent('click');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.mouse.up();
  await page.keyboard.up('a');
  await expect.poll(() => notes(page)).toEqual([]);
  await show(page);
  await page.getByTestId('piano-key-0').focus();
  await page.keyboard.down('Space');
  await page.keyboard.down('Enter');
  await page.keyboard.up('Space');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.keyboard.up('Enter');
  await expect.poll(() => notes(page)).toEqual([]);
});

test('hiding, pointer cancellation, focus loss and Panic release screen notes', async ({ page }) => {
  await boot(page);
  await show(page);
  for (const action of ['hide', 'cancel', 'blur', 'panic']) {
    await press(page);
    await expect.poll(() => notes(page)).toEqual([48]);
    if (action === 'hide') await page.getByTestId('toggle-keyboard').dispatchEvent('click');
    if (action === 'cancel') await page.getByTestId('piano-key-0').evaluate(key => {
      key.dispatchEvent(new PointerEvent('pointercancel', { pointerId: 1 }));
    });
    if (action === 'blur') await page.evaluate(() => window.dispatchEvent(new Event('blur')));
    if (action === 'panic') await page.getByTestId('panic').dispatchEvent('click');
    await expect.poll(() => notes(page)).toEqual([]);
    await page.mouse.up();
    if (action === 'hide') await show(page);
  }
});

test('focused keys support Enter/Space, release on focus change and respect typing/modals', async ({ page }) => {
  await boot(page);
  await show(page);
  const key = page.getByTestId('piano-key-0');
  await key.focus();
  await page.keyboard.down('Space');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.getByTestId('keyboard-octave-up').focus();
  await expect.poll(() => notes(page)).toEqual([]);
  await page.keyboard.up('Space');
  await key.focus();
  await page.keyboard.down('Enter');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.keyboard.up('Enter');
  await expect.poll(() => notes(page)).toEqual([]);
  await page.locator('.library-search').focus();
  await page.keyboard.type('axz');
  await expect.poll(() => notes(page)).toEqual([]);
  await expect(page.getByTestId('octave')).toHaveText('OCT 4');
  await page.getByTestId('open-settings').click();
  await page.keyboard.press('a');
  await expect.poll(() => notes(page)).toEqual([]);
});

test('screen notes reach RealEngine through existing noteOn/noteOff messages', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await show(page);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await press(page, 7);
  await page.mouse.up();
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.sent
    .filter((m: any) => m.type === 'noteOn' || m.type === 'noteOff')
    .map((m: any) => [m.type, m.note, m.velocity ?? null])))
    .toEqual([['noteOn', 55, 0.8], ['noteOff', 55, null]]);
});
