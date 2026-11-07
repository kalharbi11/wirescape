// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, state } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

test('browser lists nine read-only bundles and loads through existing graph path', async ({ page }) => {
  await boot(page);
  await page.getByTestId('open-patch-browser').click();
  await expect(page.getByTestId('patch-browser').locator('[data-testid^="bundled-patch-"]')).toHaveCount(9);
  await page.getByTestId('bundled-patch-0').click();
  await expect(page.getByTestId('patch-browser')).toBeHidden();
  expect((await state(page)).modules.length).toBeGreaterThan(0);
});

test('user selection round-trips unknown fields; bundled Save creates a copy', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await page.evaluate(() => {
    const b = window.wirescape!;
    const calls: any[] = [];
    (window as any).__libraryCalls = calls;
    b.openPatch = async (request: any) => {
      calls.push(request);
      if (request.library === 'list') return { name: 'User patches', text: JSON.stringify(['Song.json']) };
      if (request.library === 'reset') return null;
      return { name: 'Song.json', text: JSON.stringify({ format: 'wirescape.native.patch', version: 2,
        modules: [{ id: 1, module: 'essentials.vco', future: { retained: true } }], connections: [], futureTop: 42 }) };
    };
    b.savePatch = async (_text, options) => { calls.push(options); return { name: 'Copy.json' }; };
  });
  await page.getByTestId('open-patch-browser').click();
  await page.getByTestId('user-patch-0').click();
  expect((await state(page)).futureTop).toBe(42);
  expect((await state(page)).modules[0].future).toEqual({ retained: true });
  await page.getByTestId('open-patch-browser').click();
  await page.getByTestId('bundled-patch-0').click();
  await page.getByTestId('save-patch').click();
  expect(await page.evaluate(() => (window as any).__libraryCalls)).toContainEqual({ library: 'reset' });
  expect(await page.evaluate(() => (window as any).__libraryCalls)).toContainEqual({ saveAs: true });
});

test('patch list errors remain readable and do not replace the current document', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  const before = await state(page);
  await page.evaluate(() => { window.wirescape!.openPatch = async () => { throw new Error('Patch folder unavailable'); }; });
  await page.getByTestId('open-patch-browser').click();
  await expect(page.getByTestId('patch-browser')).toContainText('Patch folder unavailable');
  expect((await state(page)).modules).toEqual(before.modules);
});
