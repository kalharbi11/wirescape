// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

const notes = (page: Page) => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes ?? []);
async function disable(page: Page): Promise<void> {
  await page.getByTestId('open-settings').click();
  await page.getByTestId('computer-keyboard-plays').click();
  await page.getByTestId('settings-modal-close').click();
}

test('computer shortcuts start enabled and old preferences inherit that default', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('wirescape.prefs.v1', JSON.stringify({ motion: false })));
  await boot(page);
  await page.keyboard.down('a');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.keyboard.up('a');
  await expect.poll(() => notes(page)).toEqual([]);
  await page.getByTestId('open-settings').click();
  await expect(page.getByTestId('computer-keyboard-plays')).toHaveAttribute('aria-pressed', 'true');
});

test('disable releases held computer notes, persists and leaves explicit screen keys usable', async ({ page }) => {
  await boot(page);
  await page.keyboard.down('a');
  await expect.poll(() => notes(page)).toEqual([48]);
  await disable(page);
  await expect.poll(() => notes(page)).toEqual([]);
  await page.keyboard.up('a');
  await page.reload();
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
  await page.keyboard.type('ax');
  await expect.poll(() => notes(page)).toEqual([]);
  await expect(page.getByTestId('octave')).toHaveText('OCT 4');
  await page.getByTestId('toggle-keyboard').click();
  await page.getByTestId('piano-key-0').hover();
  await page.mouse.down();
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.mouse.up();
  await expect.poll(() => notes(page)).toEqual([]);
  await page.getByTestId('open-settings').click();
  await expect(page.getByTestId('computer-keyboard-plays')).toHaveAttribute('aria-pressed', 'false');
  await page.getByTestId('computer-keyboard-plays').click();
  await page.getByTestId('settings-modal-close').click();
  await page.keyboard.down('a');
  await expect.poll(() => notes(page)).toEqual([48]);
  await page.getByTestId('panic').click();
  await expect.poll(() => notes(page)).toEqual([]);
  await page.keyboard.up('a');
});

test('disabled shortcuts emit no real note commands and do not toggle MIDI inputs', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await disable(page);
  await page.keyboard.press('a');
  expect(await page.evaluate(() => (window as any).__mockBridge.sent.filter(
    (m: any) => ['noteOn', 'noteOff', 'setMidiEnabled'].includes(m.type)))).toEqual([]);
  await page.getByTestId('toggle-keyboard').click();
  await page.getByTestId('piano-key-0').hover();
  await page.mouse.down();
  await page.mouse.up();
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.sent.filter(
    (m: any) => m.type === 'noteOn' || m.type === 'noteOff').map((m: any) => [m.type, m.note])))
    .toEqual([['noteOn', 48], ['noteOff', 48]]);
});
