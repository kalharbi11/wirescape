// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { state } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

const snapshot = { name: 'Recovered song', patch: {
  format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 9, module: 'essentials.vco', parameters: { frequency: 123 }, future: { retained: true } }],
  connections: [], futureTop: { retained: [1, 2] },
} };
async function seed(page: Page, raw: string | null, crashed = true) {
  await page.addInitScript(({ raw, crashed }) => {
    if (raw !== null) localStorage.setItem('wirescape.autosave.v2', raw);
    if (crashed) localStorage.setItem('wirescape.session.active.v1', '1');
  }, { raw, crashed });
  await page.goto('/');
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
}

test('unclean app startup protects the snapshot and offers lossless restore without saving a file', async ({ page }) => {
  await mockBridge(page);
  const raw = JSON.stringify(snapshot);
  await seed(page, raw);
  await expect(page.getByTestId('recovery-modal')).toBeVisible();
  await page.evaluate(() => window.__wirescapeTest.autosaveNow());
  expect(await page.evaluate(() => localStorage.getItem('wirescape.autosave.v2'))).toBe(raw);
  await page.getByTestId('recovery-restore').click();
  const restored = await state(page);
  expect(restored.futureTop).toEqual(snapshot.patch.futureTop);
  expect(restored.modules[0].future).toEqual({ retained: true });
  expect(await page.evaluate(() => window.__wirescapeTest.patchName())).toBe(snapshot.name);
  expect(await page.evaluate(() => (window as any).__mockBridge.getSaved())).toBe('');
});

for (const dismiss of ['recovery-decline', 'recovery-modal-close']) {
  test(`declining app recovery through ${dismiss} keeps the current patch`, async ({ page }) => {
    await seed(page, JSON.stringify(snapshot));
    const before = await state(page);
    await page.getByTestId(dismiss).click();
    await expect(page.getByTestId('recovery-modal')).toBeHidden();
    expect((await state(page)).modules).toEqual(before.modules);
  });
}

for (const raw of [null, '{broken', JSON.stringify({ patch: { modules: [], connections: [] } })]) {
  test(`missing or corrupt snapshot does not block startup: ${raw}`, async ({ page }) => {
    await seed(page, raw);
    await expect(page.getByTestId('recovery-modal')).toBeHidden();
    expect((await state(page)).modules.length).toBeGreaterThan(0);
  });
}

test('clean reload keeps automatic restore and shows no crash offer', async ({ page }) => {
  await seed(page, JSON.stringify(snapshot), false);
  await expect(page.getByTestId('recovery-modal')).toBeHidden();
  expect((await state(page)).futureTop).toEqual(snapshot.patch.futureTop);
  // Orderly pagehide clears the marker; the next startup must not offer crash recovery.
  await page.evaluate(() => window.dispatchEvent(new PageTransitionEvent('pagehide')));
  expect(await page.evaluate(() => localStorage.getItem('wirescape.session.active.v1'))).toBeNull();
  await page.reload();
  await page.waitForFunction(() => document.body.dataset.ready === 'true');
  await expect(page.getByTestId('recovery-modal')).toBeHidden();
  expect((await state(page)).futureTop).toEqual(snapshot.patch.futureTop);
});

for (const restore of [true, false]) {
  test(`engine crash offer ${restore ? 'restores autosave' : 'keeps current document'} without requesting restart`, async ({ page }) => {
    await mockBridge(page);
    await seed(page, null, false);
    await page.evaluate(saved => {
      localStorage.setItem('wirescape.autosave.v2', JSON.stringify(saved));
      (window as any).__mockBridge.exit();
    }, snapshot);
    await expect(page.getByTestId('recovery-modal')).toBeVisible();
    const before = await state(page);
    await page.getByTestId(restore ? 'recovery-restore' : 'recovery-decline').click();
    if (restore) expect((await state(page)).futureTop).toEqual(snapshot.patch.futureTop);
    else expect((await state(page)).modules).toEqual(before.modules);
    expect(await page.evaluate(() => (window as any).__mockBridge.sent.filter((m: any) => m.type === 'restart'))).toEqual([]);
    expect(await page.evaluate(() => (window as any).__mockBridge.getSaved())).toBe('');
  });
}
