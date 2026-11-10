// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

/** G2b (#510): all authoring operations use FakeEngine, never device I/O. */
const cfg = async (page: Page) => (await state(page)).modules.find((m: any) => m.module === 'sequencing.groove')?.config;
const pad = (page: Page, i: number) => page.getByTestId('groove-pad-1-' + i);
const pane = (page: Page) => page.getByTestId('groove-1');

async function showGroove(page: Page) {
  await settle(page);
  await page.evaluate(() => {
    const v = window.__wirescapeTest.canvas.viewport;
    v.setView(v.view.x, v.view.y, 1);
    const c = document.querySelector('[data-testid="groove-1"]')!.getBoundingClientRect();
    const r = v.rect();
    v.setView(v.view.x - (c.left - r.left) + 35, v.view.y - (c.top - r.top) + 45, 1);
  });
  await settle(page);
}
async function toggle(page: Page, i: number) {
  if (!/\bsel\b/.test(await pad(page, i).getAttribute('class') ?? '')) await pad(page, i).click();
  await pad(page, i).click();
}
const flush = (page: Page) => page.evaluate(() => window.__wirescapeTest.flushHistory());

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await showGroove(page);
});

test('scale helper is view-only; pitch gestures advance through selected scale', async ({ page }) => {
  await toggle(page, 2);
  const before = JSON.stringify(await cfg(page));
  await page.getByTestId('groove-scale-root-1').selectOption('2');
  await page.getByTestId('groove-scale-1').selectOption('Major');
  expect(JSON.stringify(await cfg(page))).toBe(before);
  // D major: C4 is outside the scale, next permitted note is D4 (62).
  await pane(page).focus();
  await page.keyboard.press('ArrowUp');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(62);
  await page.getByTestId('groove-scale-1').selectOption('Chromatic');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(62);
});

test('copy, paste and clear selected bar are each one history edit', async ({ page }) => {
  await toggle(page, 2);
  await flush(page);
  await page.getByTestId('groove-copy-bar-1').click();
  await page.getByTestId('groove-len-32-1').click();
  await pad(page, 18).click();
  await flush(page);
  await page.getByTestId('groove-paste-bar-1').click();
  await flush(page);
  expect((await cfg(page)).tracks[0].steps[17]).toMatchObject({ kind: 'note', key: 60 });
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[17]?.kind ?? 'rest').toBe('rest');
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[17]?.kind).toBe('note');
  await flush(page);
  await page.getByTestId('groove-clear-bar-1').click();
  await flush(page);
  expect((await cfg(page)).tracks[0].steps[17].kind).toBe('rest');
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[17].kind).toBe('note');
});

test('piano row, track keys, tie, rest, arrows, and Undo stay within focused Groove', async ({ page }) => {
  await pad(page, 2).click();
  await pane(page).focus();
  await page.keyboard.press('a');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(60);
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('-');
  expect((await cfg(page)).tracks[0].steps[2].kind).toBe('tie');
  await flush(page);
  await page.keyboard.press('Backspace');
  await flush(page);
  expect((await cfg(page)).tracks[0].steps[2].kind).toBe('rest');
  await page.keyboard.press('Control+z');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[2].kind).toBe('tie');
  await pane(page).focus();
  await page.keyboard.press('2');
  await page.keyboard.press('x');
  await page.keyboard.press('a');
  expect((await cfg(page)).tracks[1].steps[0].key).toBe(72);
});

test('single-note Roll adds, ties and deletes; save/reload retains authored keys', async ({ page }) => {
  await page.getByTestId('groove-view-roll-1').click();
  await expect(page.getByTestId('groove-roll-1')).toBeVisible();
  const start = page.getByTestId('groove-roll-1-2-60');
  await start.click();
  expect((await cfg(page)).tracks[0].steps[1]).toMatchObject({ kind: 'note', key: 60 });
  const end = page.getByTestId('groove-roll-1-5-60');
  const a = (await start.boundingBox())!, b = (await end.boundingBox())!;
  await page.mouse.move(a.x + 10, a.y + 12);
  await page.mouse.down();
  await page.mouse.move(b.x + 10, b.y + 12, { steps: 8 });
  await page.mouse.up();
  expect((await cfg(page)).tracks[0].steps.slice(1, 5).map((s: any) => s.kind))
    .toEqual(['note', 'tie', 'tie', 'tie']);
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(s => window.__wirescapeTest.load(JSON.parse(s), 'Reload'), saved);
  await settle(page);
  expect(JSON.stringify(await state(page))).toBe(saved);
  await page.getByTestId('groove-view-roll-1').click();
  await page.getByTestId('groove-roll-1-5-60').click({ button: 'right' });
  expect((await cfg(page)).tracks[0].steps[4].kind).toBe('rest');
});

test('Graphite is default, looks do not rewrite config, phone pads wrap into two rows', async ({ page }) => {
  expect(await pane(page).getAttribute('data-look')).toBe('graphite');
  const before = JSON.stringify(await state(page));
  await page.getByTestId('groove-look-1').selectOption('oxblood');
  expect(await pane(page).getAttribute('data-look')).toBe('oxblood');
  await page.getByTestId('groove-look-1').selectOption('led');
  expect(await pane(page).getAttribute('data-look')).toBe('led');
  expect(JSON.stringify(await state(page))).toBe(before);
  await page.setViewportSize({ width: 390, height: 844 });
  await settle(page);
  const groups = await pane(page).locator('.g-pgrp').evaluateAll(els => els.slice(0, 4).map(el => {
    const r = el.getBoundingClientRect();
    return { x: r.x, y: r.y, width: r.width };
  }));
  expect(groups).toHaveLength(4);
  expect(groups[0]!.y).toBe(groups[1]!.y);
  expect(groups[2]!.y).toBeGreaterThan(groups[0]!.y);
  const width = await pane(page).evaluate(el => el.getBoundingClientRect().width);
  expect(width).toBeLessThanOrEqual(366);
  await page.getByTestId('groove-view-roll-1').click();
  const roll = page.getByTestId('groove-roll-1');
  await expect(roll).toBeVisible();
  const sizes = await roll.evaluate(el => ({ client: el.clientWidth, scroll: el.scrollWidth }));
  expect(sizes.scroll).toBeGreaterThan(sizes.client);
});
