// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, state } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

test('browser preserves built-in test bundles separately from editable shared patches', async ({ page }) => {
  await boot(page);
  await page.getByTestId('open-patch-browser').click();
  const bundles = page.getByTestId('patch-browser').locator('[data-testid^="bundled-patch-"]');
  // The six retired Qt starters no longer occupy the read-only bundle list.
  // MIDI and the three class demos remain in the browser.
  await expect(bundles).toHaveCount(4);
  await expect(bundles.nth(0)).toContainText('MIDI · Keys, Knobs and Wheels');
  await expect(bundles.nth(1)).toContainText('Class demo · CHAOS');
  await expect(bundles.nth(2)).toContainText('Class demo · Subtractive');
  await expect(bundles.nth(3)).toContainText('Class demo · Macro');
  await page.getByTestId('bundled-patch-0').click();
  await expect(page.getByTestId('patch-browser')).toBeHidden();
  expect((await state(page)).modules.length).toBeGreaterThan(0);
});

test('shared JSON library loads read-only without editing the current patch on a failed fetch', async ({ page }) => {
  const patch = { format: 'wirescape.native.patch', version: 2, name: 'Uploaded study',
    modules: [{ id: 1, module: 'essentials.vco' }, { id: 2, module: 'essentials.output' }],
    connections: [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }],
    outputs: [{ module: 2, port: 'audio', bus_channel: 0 }] };
  await page.route('**/patches/index.json', route => route.fulfill({
    json: { patches: [{ id: 'uploaded-study', name: 'Uploaded study', patch }] },
  }));
  await boot(page);
  await page.getByTestId('starter-select').selectOption('uploaded-study');
  await expect(page.getByTestId('patch-name')).toHaveText('Uploaded study');
  await page.getByTestId('open-patch-browser').click();
  await page.getByTestId('shared-patch-uploaded-study').click();
  expect((await state(page)).modules).toEqual(patch.modules);
  const before = await state(page);
  await page.route('**/patches/index.json', route => route.abort());
  await page.getByTestId('open-patch-browser').click();
  await expect(page.getByTestId('patch-browser')).toContainText('Shared patch library is unavailable.');
  expect((await state(page)).modules).toEqual(before.modules);
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

test('declining the unsaved-edit prompt keeps the current patch and browser open', async ({ page }) => {
  await boot(page);
  await page.getByTestId('library-item-essentials.noise').click();
  const before = await state(page);
  await page.getByTestId('open-patch-browser').click();
  page.once('dialog', dialog => dialog.dismiss());
  await page.getByTestId('bundled-patch-0').click();
  await expect(page.getByTestId('patch-browser')).toBeVisible();
  expect((await state(page)).modules).toEqual(before.modules);
});

test('failed bundle reset defers Save until graph loading finishes and still forces Save As', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await page.evaluate(() => {
    const bridge = window.wirescape!;
    window.__wirescapeTest.engine.loadGraph = () => new Promise<void>(resolve => {
      (window as any).__finishGraphLoad = resolve;
    });
    bridge.openPatch = async request => {
      if (request?.library === 'reset') throw new Error('Reset failed');
      return { name: 'User patches', text: '[]' };
    };
    bridge.savePatch = async (text, options) => {
      (window as any).__saveOptions = options;
      (window as any).__savedPatch = JSON.parse(text);
      return { name: 'Copy.json' };
    };
  });
  await page.getByTestId('open-patch-browser').click();
  await page.getByTestId('bundled-patch-0').click();
  await page.waitForFunction(() => typeof (window as any).__finishGraphLoad === 'function');
  const pending = await state(page);
  // Even a programmatic Save cannot write a document whose load is unsettled.
  await page.evaluate(() => (document.querySelector('[data-testid="save-patch"]') as HTMLButtonElement).click());
  expect(await page.evaluate(() => (window as any).__saveOptions)).toBeUndefined();
  expect(await page.evaluate(() => (window as any).__savedPatch)).toBeUndefined();
  expect(await state(page)).toEqual(pending);
  await page.evaluate(() => (window as any).__finishGraphLoad());
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect(page.getByTestId('patch-browser')).toBeHidden();
  expect(await state(page)).toEqual(pending);
  await page.getByTestId('save-patch').click();
  expect(await page.evaluate(() => (window as any).__saveOptions)).toEqual({ saveAs: true });
  expect(await page.evaluate(() => (window as any).__savedPatch)).toEqual(pending);
});

test('malformed user patch keeps the document and cannot silently redirect Save', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  const before = await state(page);
  await page.evaluate(() => {
    window.wirescape!.openPatch = async request => request?.library === 'list'
      ? { name: 'User patches', text: '["Broken.json"]' }
      : { name: 'Broken.json', text: '{"modules":null}' };
    window.wirescape!.savePatch = async (_text, options) => {
      (window as any).__saveOptions = options;
      return null;
    };
  });
  await page.getByTestId('open-patch-browser').click();
  await page.getByTestId('user-patch-0').click();
  await expect(page.getByTestId('patch-browser')).toContainText('Invalid patch');
  expect((await state(page)).modules).toEqual(before.modules);
  await page.getByTestId('patch-browser').getByRole('button', { name: 'Close' }).click();
  await page.getByTestId('save-patch').click();
  expect(await page.evaluate(() => (window as any).__saveOptions)).toEqual({ saveAs: true });
});
