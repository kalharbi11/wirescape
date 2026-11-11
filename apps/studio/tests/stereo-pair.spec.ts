// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state, patch } from './helpers';

/** #178: one gesture connects a stereo pair as two ordinary cables, all or nothing. */

const pairPatch = {
  format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 1, module: 'essentials.chaos_lite' }, { id: 2, module: 'fx.delay' }],
  connections: [],
  ui: { positions: { '1': [80, 120], '2': [1100, 120] }, labels: {}, view: { x: 0, y: 0, zoom: 0.7 } },
};

async function loadPair(page: Page): Promise<void> {
  await boot(page);
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Stereo'), pairPatch);
  await settle(page);
}

const signalCables = async (page: Page) =>
  (await state(page)).connections
    .filter((c: any) => c.kind === 'signal')
    .map((c: any) => `${c.from.module}:${c.from.port}>${c.to.module}:${c.to.port}`)
    .sort();

const occupied = [
  { kind: 'signal', from: { module: 1, port: 'right' }, to: { module: 2, port: 'left' } },
  { kind: 'signal', from: { module: 1, port: 'left' }, to: { module: 2, port: 'right' } },
];
const cableNames = (cs: any[]) => cs
  .filter(c => c.kind === 'signal')
  .map(c => `${c.from.module}:${c.from.port}>${c.to.module}:${c.to.port}`)
  .sort();
const engineCables = (page: Page) => page.evaluate(() =>
  (window.__wirescapeTest.engine as any).conns
    .filter((c: any) => c.kind === 'signal')
    .map((c: any) => `${c.from.module}:${c.from.port}>${c.to.module}:${c.to.port}`).sort());
async function loadOccupied(page: Page): Promise<void> {
  await loadPair(page);
  await page.evaluate(connections => {
    const p = window.__wirescapeTest.state();
    window.__wirescapeTest.load({ ...p, connections }, 'Occupied stereo');
  }, occupied);
  await settle(page);
}

test.describe('stereo pair patching', () => {
  test('descriptor exposes the stereo pair on the ports', async ({ page }) => {
    await loadPair(page);
    await expect(page.getByTestId('port-1-output-left')).toHaveAttribute('data-stereo', 'stereo');
    await expect(page.getByTestId('port-2-input-right')).toHaveAttribute('data-stereo', 'stereo');
  });

  test('CONNECT BOTH makes two ordinary cables that survive save/reload', async ({ page }) => {
    await loadPair(page);
    await patch(page, { module: 1, port: 'left' }, { module: 2, port: 'left' });
    await page.getByTestId('stereo-yes').click();
    await settle(page);
    const want = ['1:left>2:left', '1:right>2:right'];
    expect(await signalCables(page)).toEqual(want);

    await page.evaluate(() => window.__wirescapeTest.load(window.__wirescapeTest.state()));
    await settle(page);
    expect(await signalCables(page)).toEqual(want);
  });

  test('JUST THIS ONE keeps the single-cable behaviour', async ({ page }) => {
    await loadPair(page);
    await patch(page, { module: 1, port: 'right' }, { module: 2, port: 'right' });
    await page.getByTestId('stereo-no').click();
    await settle(page);
    expect(await signalCables(page)).toEqual(['1:right>2:right']);
  });

  test('replacing both occupied inputs is one Undo/Redo gesture', async ({ page }) => {
    await loadOccupied(page);
    const before = cableNames(occupied);
    expect(await signalCables(page)).toEqual(before);
    expect(await engineCables(page)).toEqual(before);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);

    await patch(page, { module: 1, port: 'left' }, { module: 2, port: 'left' });
    await page.getByTestId('stereo-yes').click();
    await settle(page);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    const paired = ['1:left>2:left', '1:right>2:right'];
    expect(await signalCables(page)).toEqual(paired);
    expect(await engineCables(page)).toEqual(paired);
    expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth + 1);

    await page.evaluate(() => window.__wirescapeTest.undo());
    await settle(page);
    expect(await signalCables(page)).toEqual(before);
    expect(await engineCables(page)).toEqual(before);
    await page.evaluate(() => window.__wirescapeTest.redo());
    await settle(page);
    expect(await signalCables(page)).toEqual(paired);
    expect(await engineCables(page)).toEqual(paired);
  });

  test('partner refusal restores both occupied inputs in document and engine, with no history edit', async ({ page }) => {
    await loadOccupied(page);
    const before = await state(page);
    const engineBefore = await engineCables(page);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
    await page.evaluate(() => {
      const eng = window.__wirescapeTest.engine;
      const connect = eng.connect.bind(eng);
      eng.connect = (c: any) => {
        if (c.from.port === 'right' && c.to.port === 'right') throw new Error('partner refused for test');
        return connect(c);
      };
    });
    await patch(page, { module: 1, port: 'left' }, { module: 2, port: 'left' });
    await page.getByTestId('stereo-yes').click();
    await settle(page);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    expect(await state(page)).toEqual(before);
    expect(await engineCables(page)).toEqual(engineBefore);
    expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth);
    await expect(page.getByTestId('toast')).toContainText('partner refused for test');
  });

  test('existing stereo partner refuses CONNECT BOTH without adding a half-pair', async ({ page }) => {
    await loadPair(page);
    await page.evaluate(() => {
      const p = window.__wirescapeTest.state();
      window.__wirescapeTest.load({
        ...p, connections: [
          { kind: 'signal', from: { module: 1, port: 'right' }, to: { module: 2, port: 'right' } },
        ],
      }, 'Partly wired stereo');
    });
    await settle(page);
    const before = await state(page);
    const running = await engineCables(page);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
    await patch(page, { module: 1, port: 'left' }, { module: 2, port: 'left' });
    await page.getByTestId('stereo-yes').click();
    await settle(page);
    await page.evaluate(() => window.__wirescapeTest.flushHistory());
    expect(await state(page)).toEqual(before);
    expect(await engineCables(page)).toEqual(running);
    expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth);
  });

  test('a refused partner cable leaves no half pair', async ({ page }) => {
    await loadPair(page);
    await page.evaluate(() => {
      const eng = window.__wirescapeTest.engine;
      const real = eng.connect.bind(eng);
      eng.connect = (c: any) => {
        if (c.to.port === 'right') throw new Error('refused for test');
        return real(c);
      };
    });
    await patch(page, { module: 1, port: 'left' }, { module: 2, port: 'left' });
    await page.getByTestId('stereo-yes').click();
    await settle(page);
    expect(await signalCables(page)).toEqual([]);
    const engineConns = await page.evaluate(() => (window.__wirescapeTest.engine as any).conns.length);
    expect(engineConns).toBe(0);
  });
});
