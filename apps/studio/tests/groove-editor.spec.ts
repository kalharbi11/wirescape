// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

/** Groove G2a (#498), FakeEngine: Pads gestures, inspector, step counts, Apply, history, save/reload. */
const cfg = async (page: Page) => (await state(page)).modules.find((m: any) => m.module === 'sequencing.groove')?.config;
const pad = (page: Page, i: number) => page.getByTestId(`groove-pad-1-${i}`);

async function addGroove(page: Page): Promise<void> {
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await showGroove(page);
}
/** Put the Groove card at 1:1 near the top-left of the canvas. */
async function showGroove(page: Page): Promise<void> {
  await settle(page);
  await page.evaluate(() => {
    const v = window.__wirescapeTest.canvas.viewport;
    v.setView(v.view.x, v.view.y, 1);
    const c = document.querySelector('[data-testid="groove-1"]')!.getBoundingClientRect();
    const r = v.rect();
    v.setView(v.view.x - (c.left - r.left) + 40, v.view.y - (c.top - r.top) + 60, 1);
  });
  await settle(page);
}
async function center(page: Page, i: number) {
  await settle(page);
  const b = (await pad(page, i).boundingBox())!;
  return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
}
/** Select the pad if needed, then click it again to add/remove its trig. */
async function toggle(page: Page, i: number) {
  if (!/\bsel\b/.test(await pad(page, i).getAttribute('class') ?? '')) await pad(page, i).click();
  await expect(pad(page, i)).toHaveClass(/\bsel\b/);
  await pad(page, i).click();
}
async function flush(page: Page) { await page.evaluate(() => window.__wirescapeTest.flushHistory()); }

test.beforeEach(async ({ page }) => { await boot(page); await addGroove(page); });

test('first click selects, second click toggles a note; v2 is written on first edit', async ({ page }) => {
  expect(await cfg(page)).toBeUndefined();
  await pad(page, 3).click();
  await expect(pad(page, 3)).toHaveClass(/sel/);
  expect(await cfg(page)).toBeUndefined();
  await pad(page, 3).click();
  await expect(pad(page, 3)).toHaveAttribute('data-kind', 'note');
  const c = await cfg(page);
  expect(c.version).toBe(2);
  expect(c.tracks[0].length).toBe(16);
  expect(c.tracks[0].steps[2]).toMatchObject({ kind: 'note', key: 60, velocity: 0.8 });
  await pad(page, 3).click();
  await expect(pad(page, 3)).toHaveAttribute('data-kind', 'rest');
  expect((await cfg(page)).tracks[0].steps[2]).toEqual({ kind: 'rest' });
});

test('drag pitch by semitones, Shift by octaves, wheel, and drag-right tie', async ({ page }) => {
  await toggle(page, 2);
  let p = await center(page, 2);
  await page.mouse.move(p.x, p.y); await page.mouse.down();
  await page.mouse.move(p.x, p.y - 28, { steps: 6 }); await page.mouse.up();
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(62);
  await page.keyboard.down('Shift');
  await page.mouse.move(p.x, p.y); await page.mouse.down();
  await page.mouse.move(p.x, p.y + 14, { steps: 4 }); await page.mouse.up();
  await page.keyboard.up('Shift');
  expect((await cfg(page)).tracks[0].steps[1].key).toBe(50);
  p = await center(page, 2);
  await page.mouse.move(p.x, p.y);
  await page.mouse.wheel(0, -60);
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].key).toBe(51);
  await page.keyboard.down('Shift'); await page.mouse.wheel(0, 60); await page.keyboard.up('Shift');
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].key).toBe(39);
  const q = await center(page, 5);
  await page.mouse.move(p.x, p.y); await page.mouse.down();
  await page.mouse.move(q.x, p.y, { steps: 10 }); await page.mouse.up();
  const steps = (await cfg(page)).tracks[0].steps;
  expect(steps.slice(1, 5).map((s: any) => s.kind)).toEqual(['note', 'tie', 'tie', 'tie']);
  await expect(pad(page, 4)).toHaveClass(/tie/);
  await expect(page.getByTestId('groove-note-1')).toHaveValue('D#2');
});

test('inspector: velocity, gate length and condition picker', async ({ page }) => {
  await toggle(page, 5);
  await page.getByTestId('groove-vel-1').fill('55');
  await page.getByTestId('groove-vel-1').press('Enter');
  await page.getByTestId('groove-gate-0.5-1').click();
  await page.getByTestId('groove-cond-1').click();
  await expect(page.getByTestId('groove-cond-pop-1')).toBeVisible();
  await page.getByTestId('groove-cond-pop-1').getByRole('button', { name: '1:4', exact: true }).click();
  await expect(page.getByTestId('groove-cond-pop-1')).toHaveCount(0);
  expect((await cfg(page)).tracks[0].steps[4]).toEqual(
    { kind: 'note', key: 60, velocity: 0.55, gate: 0.5, cond: { type: 'ab', a: 1, b: 4 } });
  await expect(pad(page, 5)).toHaveClass(/cond/);
  await expect(pad(page, 5)).toContainText('1:4');
  await page.getByTestId('groove-cond-1').click();
  await page.getByTestId('groove-cond-pop-1').getByRole('button', { name: '50%', exact: true }).click();
  expect((await cfg(page)).tracks[0].steps[4].cond).toEqual({ type: 'chance', p: 50 });
  await page.getByTestId('groove-cond-1').click();
  await page.getByTestId('groove-cond-pop-1').getByRole('button', { name: 'Not first' }).click();
  expect((await cfg(page)).tracks[0].steps[4].cond).toEqual({ type: 'not_first' });
  await page.getByTestId('groove-cond-1').click();
  await page.getByTestId('groove-cond-pop-1').getByRole('button', { name: 'Always' }).click();
  expect((await cfg(page)).tracks[0].steps[4].cond).toBeUndefined();
  await page.getByTestId('groove-kind-tie-1').click();
  expect((await cfg(page)).tracks[0].steps[4]).toEqual({ kind: 'tie', gate: 0.5 });
  await expect(page.getByTestId('groove-cond-1')).toBeDisabled();
});

test('every step count lays out bars of 16 and trims the saved steps', async ({ page }) => {
  for (const n of [8, 16, 32, 64]) {
    await page.getByTestId(`groove-len-${n}-1`).click();
    await expect(page.locator('[data-testid="groove-1"] .g-pad')).toHaveCount(n);
    await expect(page.locator('[data-testid="groove-1"] .g-bar')).toHaveCount(Math.ceil(n / 16));
    expect((await cfg(page)).tracks[0].length).toBe(n);
  }
  await toggle(page, 64);
  await page.getByTestId('groove-len-16-1').click();
  const c = await cfg(page);
  expect(c.tracks[0].steps.length).toBeLessThanOrEqual(16);
  await page.getByTestId('groove-track-1-2').click();
  await page.getByTestId('groove-len-32-1').click();
  expect((await cfg(page)).tracks.map((t: any) => t.length)).toEqual([16, 32]);
});

test('draft, Apply, Revert; undo/redo and save/reload equality', async ({ page }) => {
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  await toggle(page, 2);
  await expect(page.getByTestId('groove-draft-1')).toBeVisible();
  await expect(pad(page, 2)).toHaveClass(/chg/);
  await page.getByTestId('groove-revert-1').click();
  await expect(pad(page, 2)).toHaveAttribute('data-kind', 'rest');
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  await toggle(page, 2);
  await expect(page.getByTestId('groove-apply-1')).toBeEnabled();
  await page.getByTestId('groove-apply-1').click();
  await expect(page.getByTestId('groove-draft-1')).toBeHidden();
  await expect(pad(page, 2)).not.toHaveClass(/chg/);
  await flush(page);
  await page.getByTestId('groove-gate-0.75-1').click();
  await flush(page);
  expect((await cfg(page)).tracks[0].steps[1].gate).toBe(0.75);
  await page.getByTestId('groove-undo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].gate).toBeUndefined();
  await page.getByTestId('groove-redo-1').click();
  await expect.poll(async () => (await cfg(page)).tracks[0].steps[1].gate).toBe(0.75);
  // Unknown keys survive editing and save/reload.
  await page.evaluate(() => {
    const t = window.__wirescapeTest;
    const m = t.doc.modules.get(1);
    m.config.future = { keep: 1 };
    m.config.tracks[0].steps[1].shade = 'blue';
  });
  await page.getByTestId('groove-vel-1').fill('40');
  await page.getByTestId('groove-vel-1').press('Enter');
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  expect(JSON.parse(saved).modules[0].config).toMatchObject({ future: { keep: 1 } });
  await page.evaluate(s => window.__wirescapeTest.load(JSON.parse(s), 'Reloaded'), saved);
  await settle(page);
  expect(JSON.stringify(await state(page))).toBe(saved);
  await expect(page.getByTestId('groove-pad-1-2')).toHaveAttribute('data-kind', 'note');
});

test('v1 patterns load as 32 steps and are written as v2 on the first edit', async ({ page }) => {
  const v1 = { version: 1, bpm: 100, tracks: [{ steps: [{ kind: 'note', key: 64, velocity: 1 }, { kind: 'tie' }] }] };
  await page.evaluate(c => window.__wirescapeTest.load({ format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'sequencing.groove', version: 2, config: c }], connections: [] }, 'V1'), v1);
  await showGroove(page);
  await expect(page.locator('[data-testid="groove-1"] .g-pad')).toHaveCount(32);
  expect(await cfg(page)).toEqual(v1);
  await toggle(page, 4);
  const c = await cfg(page);
  expect(c.version).toBe(2);
  expect(c.tracks[0].length).toBe(32);
  expect(c.tracks[0].steps.slice(0, 2)).toEqual(v1.tracks[0].steps);
  expect(c.tracks).toHaveLength(2);
});
