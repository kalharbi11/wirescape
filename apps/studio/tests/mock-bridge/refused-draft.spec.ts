// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import { boot, state } from '../helpers';
import { mockBridge } from './bridge';

const playing = {
  format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 1, module: 'essentials.output' }], connections: [],
};
const refused = {
  format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 2, module: 'essentials.vco' }], connections: [],
  future: { ownerData: ['keep', { nested: 42 }] },
};

test.beforeEach(async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Playing A'), playing);
  await expect.poll(() => page.evaluate(() =>
    (window as any).__mockBridge.getGraph().modules.map((m: any) => m.id))).toEqual([1]);
});

test('busy refusal preserves A while B can be viewed and edited without touching the live graph', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), refused);
  await expect(page.getByTestId('patch-name')).toHaveText('Playing A');
  await expect(page.getByTestId('open-refused-draft')).toBeVisible();
  await page.getByTestId('open-refused-draft').click();
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  await expect(page.getByTestId('patch-name')).toHaveText('Refused B');
  expect((await state(page) as any).future).toEqual(refused.future);
  const before = await page.evaluate(() => (window as any).__mockBridge.sent.length);
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.output'));
  await expect.poll(() => (state(page) as Promise<any>).then(p => p.modules.length)).toBe(2);
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph().modules.map((m: any) => m.id))).toEqual([1]);
  expect(await page.evaluate(() => (window as any).__mockBridge.sent.length)).toBe(before);
  await page.getByTestId('back-to-playing').click();
  await expect(page.getByTestId('patch-name')).toHaveText('Playing A');
  await page.getByTestId('open-refused-draft').click();
  expect((await state(page) as any).modules.length).toBe(2);
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
});

test('reapplying a refused draft retries safely, then runs B and preserves unknown data', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'unavailable'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), refused);
  await page.getByTestId('open-refused-draft').click();
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.getByTestId('apply-refused-draft').click();
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph().modules.map((m: any) => m.id))).toEqual([1]);
  await page.getByTestId('apply-refused-draft').click();
  await expect.poll(() => page.evaluate(() =>
    (window as any).__mockBridge.getGraph().modules.map((m: any) => m.id))).toEqual([2]);
  await expect(page.getByTestId('draft-not-running')).toBeHidden();
  await expect(page.getByTestId('open-refused-draft')).toBeHidden();
  expect((await state(page) as any).future).toEqual(refused.future);
});
