// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const config = async (page: Page) => (await state(page)).modules.find(
  (m: any) => m.module === 'sequencing.groove')?.config;

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await settle(page);
});

test('Speed omits 1 and writes 2/4/8; Steps changes preserve speed and Undo/Redo', async ({ page }) => {
  const speed = (n: number) => page.getByTestId('groove-div-' + n + '-1');
  expect((await config(page)).tracks[0].div).toBeUndefined();
  for (const div of [2, 4, 8]) {
    await speed(div).click();
    expect((await config(page)).tracks[0].div).toBe(div);
    await expect(page.getByTestId('groove-speed-badge-1-1')).toHaveText(
      ({ 2: '½×', 4: '¼×', 8: '⅛×' } as Record<number, string>)[div]!);
    await expect(page.getByTestId('groove-loop-1')).toContainText(String(16 * div));
  }
  await page.getByTestId('groove-len-32-1').click();
  expect((await config(page)).tracks[0]).toMatchObject({ div: 8, length: 32 });
  await expect(page.getByTestId('groove-loop-1')).toContainText('256 sixteenths');
  await page.evaluate(() => window.__wirescapeTest.flushHistory());
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await config(page)).tracks[0].length).toBe(16);
  expect((await config(page)).tracks[0].div).toBe(8);
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await config(page)).tracks[0].length).toBe(32);
  await speed(1).click();
  expect((await config(page)).tracks[0].div).toBeUndefined();
  await expect(page.getByTestId('groove-speed-badge-1-1')).toBeEmpty();
});

test('master loop is max armed track cycle, not sum; reload preserves div', async ({ page }) => {
  await page.getByTestId('groove-div-4-1').click();
  await expect(page.getByTestId('groove-loop-1')).toContainText('64 sixteenths');
  await page.getByTestId('groove-track-1-2').click();
  await page.getByTestId('groove-div-8-1').click();
  await page.getByTestId('groove-len-64-1').click();
  await expect(page.getByTestId('groove-loop-1')).toContainText('512 sixteenths');
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(s => window.__wirescapeTest.load(JSON.parse(s), 'Reload'), saved);
  await settle(page);
  expect((await config(page)).tracks.map((t: any) => t.div)).toEqual([4, 8]);
  await expect(page.getByTestId('groove-loop-1')).toContainText('512 sixteenths');
});