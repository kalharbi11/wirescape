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
