// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const cfg = async (p: Page) => (await state(p)).modules.find((m: any) => m.module === 'sequencing.groove')?.config;
const pane = (p: Page) => p.getByTestId('groove-1');
const flush = (p: Page) => p.evaluate(() => window.__wirescapeTest.flushHistory());
const keyCell = (p: Page, i: number, key: number) => p.getByTestId('groove-roll-1-' + i + '-' + key);

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await settle(page);
  await page.evaluate(() => {
    const v = window.__wirescapeTest.canvas.viewport;
    v.setView(v.view.x, v.view.y, 1);
    const c = document.querySelector('[data-testid="groove-1"]')!.getBoundingClientRect();
    const r = v.rect();
    v.setView(v.view.x - (c.left - r.left) + 35, v.view.y - (c.top - r.top) + 45, 1);
  });
  await settle(page);
});

test('Roll adds up to 5 voices, names the top note, moves one, and refuses a sixth', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-pad-1-2').click();
  await page.getByTestId('groove-notes-chord-1').click();
  await page.getByTestId('groove-view-roll-1').click();
  await expect(keyCell(page, 2, 67).locator('.g-rch')).toHaveText('C');
  await keyCell(page, 2, 62).click();
  await keyCell(page, 2, 63).click();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 62, 63, 64, 67]);
  await keyCell(page, 2, 61).click();
  await expect(pane(page).locator('.g-toast')).toHaveText('5 notes per step');
  expect((await cfg(page)).tracks[0].steps[1].keys).toHaveLength(5);
  await keyCell(page, 2, 62).click({ button: 'right' });
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 63, 64, 67]);
  await keyCell(page, 2, 63).focus();
  await page.keyboard.press('Backspace');
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67]);
  await flush(page);
  const from = (await keyCell(page, 2, 64).boundingBox())!;
  await page.mouse.move(from.x + 12, from.y + 13);
  await page.mouse.down();
  await page.mouse.move(from.x + 12, from.y - 14, { steps: 8 });
  await page.mouse.up();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 65, 67]);
  await flush(page);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67]);
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 65, 67]);
  await keyCell(page, 2, 65).dblclick();
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 67]);
});

test('keyboard held notes write one chord on final release, then advance; 1 key retains no keys', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await pane(page).focus();
  await page.keyboard.down('a');
  await page.keyboard.down('d');
  await page.keyboard.down('g');
  await page.keyboard.up('a');
  await page.keyboard.up('d');
  expect((await cfg(page))?.tracks[0]?.steps[1]?.kind ?? 'rest').toBe('rest'); // nothing written yet
  await page.keyboard.up('g');
  expect((await cfg(page)).tracks[0].steps[1].keys).toEqual([60, 64, 67]);
  await expect(page.getByTestId('groove-pad-1-3')).toHaveClass(/sel/);
  await flush(page);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page))?.tracks[0]?.steps[1]?.kind ?? 'rest').toBe('rest');
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1]?.keys).toEqual([60, 64, 67]);
  await pane(page).focus();
  for (const k of ['a', 's', 'd', 'f', 'g']) await page.keyboard.down(k);
  for (const k of ['a', 's', 'd', 'f', 'g']) await page.keyboard.up(k);
  expect((await cfg(page)).tracks[0].steps[2].keys).toEqual([60, 62, 64, 65, 67]);
  await page.keyboard.press('a');
  expect((await cfg(page)).tracks[0].steps[3]).toMatchObject({ kind: 'note', key: 60 });
  expect((await cfg(page)).tracks[0].steps[3].keys).toBeUndefined();
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(s => window.__wirescapeTest.load(JSON.parse(s), 'Reload'), saved);
  await settle(page);
  const [triad, five, single] = (await cfg(page)).tracks[0].steps.slice(1, 4);
  expect(single).toMatchObject({ kind: 'note', key: 60 });
  expect(single.keys).toBeUndefined();
  expect(triad.keys).toEqual([60, 64, 67]);
  expect(five.keys).toEqual([60, 62, 64, 65, 67]);
});
test('six held keys still wait until every keyup, but save no more than five voices', async ({ page }) => {
  await page.getByTestId('groove-pad-1-5').click();
  await pane(page).focus();
  for (const key of ['a', 'w', 's', 'e', 'd', 'f']) await page.keyboard.down(key);
  await expect(pane(page).locator('.g-toast')).toHaveText('5 notes per step');
  for (const key of ['a', 'w', 's', 'e', 'd']) await page.keyboard.up(key);
  expect((await cfg(page))?.tracks[0]?.steps[4]?.kind ?? 'rest').toBe('rest'); // nothing written yet
  await page.keyboard.up('f');
  expect((await cfg(page)).tracks[0].steps[4].keys).toEqual([60, 61, 62, 63, 64]);
  await expect(page.getByTestId('groove-pad-1-6')).toHaveClass(/sel/);
});

test('a window blur mid-chord drops the held keys; the next key enters a single note in one undo step', async ({ page }) => {
  await page.getByTestId('groove-pad-1-2').click();
  await pane(page).focus();
  await page.keyboard.down('a');
  await page.keyboard.down('d');
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.keyboard.up('a');
  await page.keyboard.up('d');
  expect((await cfg(page))?.tracks[0]?.steps[1]?.kind ?? 'rest').toBe('rest'); // nothing written
  await page.keyboard.down('g');
  await page.keyboard.up('g');
  const step = (await cfg(page)).tracks[0].steps[1];
  expect(step).toMatchObject({ kind: 'note', key: 67 });
  expect(step.keys).toBeUndefined();
  await flush(page);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page))?.tracks[0]?.steps[1]?.kind ?? 'rest').toBe('rest');
});
