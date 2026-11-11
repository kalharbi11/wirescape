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
  // D major: C4 is outside the scale, next permitted note is C#4 (61).
  await pane(page).focus();
  await page.keyboard.press('ArrowUp');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(61);
  await page.getByTestId('groove-scale-1').selectOption('Chromatic');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(61);
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
  // The Roll's lollipop lanes edit the same authored step values.
  const setRollValue = async (field: string, fraction: number) => {
    const cell = page.getByTestId('groove-roll-' + field + '-1-2');
    await expect(cell).toBeVisible(); // the lane re-renders after each edit
    const b = (await cell.boundingBox())!;
    await page.mouse.click(b.x + b.width / 2, b.y + b.height * (1 - fraction));
  };
  await setRollValue('velocity', 0.75);
  await setRollValue('gate', 0.5);
  await setRollValue('mod1', 0.8);
  const afterValues = (await cfg(page)).tracks[0].steps[1];
  expect(afterValues.velocity).toBeCloseTo(0.75, 2);
  expect(afterValues.gate).toBe(0.5);
  expect(afterValues.mod1).toBeCloseTo(0.8, 2);
  await page.getByTestId('groove-mod2-1').fill('65');
  await page.getByTestId('groove-mod2-1').press('Enter');
  expect((await cfg(page)).tracks[0].steps[1].mod2).toBeCloseTo(0.65, 2);
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

test('Roll note buttons and value sliders are keyboard-accessible', async ({ page }) => {
  await page.getByTestId('groove-view-roll-1').click();
  const note = page.getByTestId('groove-roll-1-2-60');
  await expect(note).toHaveAttribute('role', 'button');
  await note.focus();
  await page.keyboard.press('Enter');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].kind).toBe('note');
  await page.getByTestId('groove-roll-1-2-60').focus();
  await page.keyboard.press('ArrowRight');
  await expect(page.getByTestId('groove-roll-1-3-60')).toBeFocused();
  await page.keyboard.press('Space');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[2].kind).toBe('note');
  const slider = page.getByTestId('groove-roll-mod1-1-3');
  await expect(slider).toHaveAttribute('role', 'slider');
  await slider.focus();
  await page.keyboard.press('End');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[2].mod1).toBe(1);
  await expect(page.getByTestId('groove-roll-mod1-1-3')).toBeFocused();
  await page.keyboard.press('Home');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[2].mod1).toBe(0);
  await page.getByTestId('groove-roll-1-3-60').focus();
  await page.keyboard.press('Backspace');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[2].kind).toBe('rest');
  await expect(page.getByTestId('groove-roll-1-3-60')).toBeFocused();
});

test('Graphite is default; phone keeps the horizontal Groove card and patchable outputs', async ({ page }) => {
  expect(await pane(page).getAttribute('data-look')).toBe('graphite');
  const before = JSON.stringify(await state(page));
  await page.getByTestId('groove-look-1').selectOption('oxblood');
  expect(await pane(page).getAttribute('data-look')).toBe('oxblood');
  await page.getByTestId('groove-look-1').selectOption('led');
  expect(await pane(page).getAttribute('data-look')).toBe('led');
  expect(JSON.stringify(await state(page))).toBe(before);
  await page.setViewportSize({ width: 390, height: 844 });
  await settle(page);
  // Owner ruling: phones pan/zoom the original Windows-sized sequencer.
  const dimensions = await page.evaluate(() => {
    const card = document.querySelector<HTMLElement>('[data-testid="module-1"]')!;
    const canvas = window.__wirescapeTest.canvas;
    const actual = canvas.cardFor(1);
    return { width: card.offsetWidth, stored: actual?.width, right: actual?.anchor('output', 't1_pitch')?.x };
  });
  expect(dimensions).toEqual({ width: 1250, stored: 1250, right: 1250 });
  for (const id of ['t1_pitch', 't1_gate', 't1_notes']) {
    await expect(page.getByTestId(`port-1-output-${id}`)).toHaveCSS('display', 'flex');
  }
  const groups = await pane(page).locator('.g-pgrp').evaluateAll(els => els.slice(0, 4).map(el => {
    const r = el.getBoundingClientRect();
    return { x: r.x, y: r.y };
  }));
  expect(groups).toHaveLength(4);
  for (const group of groups.slice(1)) expect(Math.abs(group.y - groups[0]!.y)).toBeLessThan(1);
  await page.evaluate(() => window.__wirescapeTest.fit());
  await settle(page);
  await page.getByTestId('groove-view-roll-1').click();
  const roll = page.getByTestId('groove-roll-1');
  await expect(roll).toBeVisible();
  const sizes = await roll.evaluate(el => ({ client: el.clientWidth, scroll: el.scrollWidth }));
  expect(sizes.scroll).toBeGreaterThan(sizes.client);
});

test('scale helper is saved in ui.groove, survives reload and undo, and stays out of the engine config', async ({ page }) => {
  await page.evaluate(() => {
    const s = window.__wirescapeTest.state();
    s.ui = { ...(s.ui ?? {}), groove: { 1: { root: 0, scale: 'Chromatic', future: 1 }, 9: { root: 4, scale: 'x' } }, other: true };
    window.__wirescapeTest.load(s, 'Extras');
  });
  await showGroove(page);
  const before = await cfg(page);
  await page.getByTestId('groove-scale-root-1').selectOption('9');
  await flush(page);
  await page.getByTestId('groove-scale-1').selectOption('Minor');
  await flush(page);
  const s = await state(page);
  expect(s.ui.groove).toEqual({ 1: { root: 9, scale: 'Minor', future: 1 }, 9: { root: 4, scale: 'x' } });
  expect(s.ui.other).toBe(true);
  expect(await cfg(page)).toEqual(before); // engine-facing config is unchanged
  expect(Object.keys(await cfg(page) ?? {}).sort()).not.toContain('scale');
  // Each change is one undo step; redo brings both back.
  await page.getByTestId('groove-undo-1').click();
  await expect(page.getByTestId('groove-scale-1')).toHaveValue('Chromatic');
  await expect(page.getByTestId('groove-scale-root-1')).toHaveValue('9');
  await page.getByTestId('groove-undo-1').click();
  await expect(page.getByTestId('groove-scale-root-1')).toHaveValue('0');
  expect((await state(page)).ui.groove['1']).toEqual({ root: 0, scale: 'Chromatic', future: 1 });
  await page.getByTestId('groove-redo-1').click();
  await page.getByTestId('groove-redo-1').click();
  await expect(page.getByTestId('groove-scale-1')).toHaveValue('Minor');
  // Save and reload: the selectors come back from the patch.
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(p => window.__wirescapeTest.load(JSON.parse(p), 'Reloaded'), saved);
  await showGroove(page);
  await expect(page.getByTestId('groove-scale-root-1')).toHaveValue('9');
  await expect(page.getByTestId('groove-scale-1')).toHaveValue('Minor');
  expect((await state(page)).ui.groove).toEqual(s.ui.groove);
});
