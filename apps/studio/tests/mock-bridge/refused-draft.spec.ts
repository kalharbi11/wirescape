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

test('Apply owns its exact candidate across global shortcuts, menus and an earlier pending read', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), refused);
  await page.getByTestId('open-refused-draft').click();
  const candidate = await state(page);
  await page.evaluate(() => {
    const bridge = (window as any).__mockBridge;
    bridge.setOpen({ name: 'Late C', text: JSON.stringify({
      format: 'wirescape.native.patch', version: 2, modules: [], connections: [], late: true,
    }) });
    bridge.holdOpen(); bridge.menu('app.menuOpenPatch');
  });
  const opens = await page.evaluate(() => (window as any).__mockBridge.openCount());
  await page.evaluate(() => (window as any).__mockBridge.holdNext('loadGraph'));
  await page.getByTestId('apply-refused-draft').click();
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
  await page.keyboard.press('Control+a'); await page.keyboard.press('Delete');
  await page.keyboard.press('Control+z'); await page.keyboard.press('Control+v');
  await page.evaluate(() => {
    const bridge = (window as any).__mockBridge;
    bridge.menu('app.menuOpenPatch'); bridge.menu('app.menuSavePatch'); bridge.releaseOpen();
  });
  expect(await state(page)).toEqual(candidate);
  expect(await page.evaluate(() => (window as any).__mockBridge.openCount())).toBe(opens);
  expect(await page.evaluate(() => (window as any).__mockBridge.getSaves())).toEqual([]);
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  await page.evaluate(() => (window as any).__mockBridge.acceptHeld());
  await expect(page.getByTestId('draft-not-running')).toBeHidden();
  expect(await state(page)).toEqual(candidate);
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph())).toEqual(candidate);
  await expect(page.getByTestId('patch-name')).toHaveText('Refused B');
});

test('a pending playing edit settles on A before opening B with overlapping module IDs', async ({ page }) => {
  const a = { ...playing, owner: 'A', modules: [...playing.modules,
    { id: 2, module: 'essentials.vco', parameters: { frequency: 220 }, unknown: 'keep A' }] };
  const b = { ...refused, modules: [{ ...refused.modules[0], parameters: { frequency: 440 }, unknown: 'keep B' }] };
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Playing A'), a);
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), b);
  await page.evaluate(() => {
    (window as any).__mockBridge.holdNext('setParam');
    window.__wirescapeTest.commitParam(2, 'frequency', 900, true);
  });
  await page.getByTestId('open-refused-draft').click();
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
  await expect(page.getByTestId('patch-name')).toHaveText('Playing A');
  await page.evaluate(() => (window as any).__mockBridge.refuseHeld('refused'));
  await expect(page.getByTestId('patch-name')).toHaveText('Refused B');
  expect((await state(page)).modules[0]).toMatchObject(b.modules[0]);
  expect((await state(page)).future).toEqual(refused.future);
  await page.getByTestId('back-to-playing').click();
  const restored = await state(page);
  expect(restored.modules.find((m: any) => m.id === 2)).toMatchObject(a.modules[1]);
  expect(restored.owner).toBe('A');
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph().modules
    .find((m: any) => m.id === 2).parameters.frequency)).toBe(220);
});

test('an acknowledgement cannot label an externally changed draft as running', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), refused);
  await page.getByTestId('open-refused-draft').click();
  const submitted = await state(page);
  await page.evaluate(() => (window as any).__mockBridge.holdNext('loadGraph'));
  await page.getByTestId('apply-refused-draft').click();
  // Deliberately bypass the UI's guards to exercise the immutable-candidate fallback.
  await page.evaluate(() => window.__wirescapeTest.doc.addModule('essentials.output', {}, 0, 0, 17));
  await page.evaluate(() => (window as any).__mockBridge.acceptHeld());
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect(page.getByTestId('draft-not-running')).toBeVisible();
  expect((await state(page)).modules).toHaveLength(2);
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph())).toEqual(submitted);
  await page.getByTestId('back-to-playing').click();
  expect(await state(page)).toEqual(submitted);
});

test('disconnect releases a draft switch waiting for an outstanding edit', async ({ page }) => {
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Playing A'), refused);
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Refused B'), playing);
  await page.evaluate(() => {
    (window as any).__mockBridge.holdNext('setParam');
    window.__wirescapeTest.commitParam(2, 'frequency', 900, true);
  });
  await page.getByTestId('open-refused-draft').click();
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
  await page.evaluate(() => (window as any).__mockBridge.exit(true));
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect(page.getByTestId('patch-name')).toHaveText('Refused B');
});

test('R2 guards reject future or malformed loads without losing the retained draft', async ({ page }) => {
  const optional = { format: 'wirescape.native.patch', version: 2, future: refused.future };
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'busy'));
  await page.evaluate(async p => window.__wirescapeTest.load(p, 'Optional B'), optional);
  const before = await state(page);
  const loads = await page.evaluate(() => (window as any).__mockBridge.sent.filter((m: any) => m.type === 'loadGraph').length);
  for (const bad of [{ ...refused, version: 99 }, { ...refused, modules: [null] }]) {
    expect(await page.evaluate(async p => {
      try { await window.__wirescapeTest.load(p, 'Invalid C'); return false; } catch { return true; }
    }, bad)).toBe(true);
    expect(await state(page)).toEqual(before);
    await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  }
  expect(await page.evaluate(() => (window as any).__mockBridge.sent.filter((m: any) => m.type === 'loadGraph').length)).toBe(loads);
  await page.getByTestId('open-refused-draft').click();
  await expect(page.getByTestId('patch-name')).toHaveText('Optional B');
  expect((await state(page)).future).toEqual(refused.future);
});

for (const result of ['accepted', 'refused'] as const) {
  test(`native Save cannot cross an ordinary patch load that is ${result}`, async ({ page }) => {
    const before = await state(page);
    await page.evaluate(p => {
      const bridge = (window as any).__mockBridge;
      bridge.holdNext('loadGraph');
      (window as any).__pendingPatchLoad = window.__wirescapeTest.load(p, 'Loading B');
    }, refused);
    await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
    await expect(page.getByTestId('patch-name')).toHaveText('Loading B');
    expect(await page.evaluate(() => (window as any).__mockBridge.getGraph().modules
      .map((m: any) => m.id))).toEqual([1]);
    await page.evaluate(() => (window as any).__mockBridge.menu('app.menuSavePatch'));
    expect(await page.evaluate(() => (window as any).__mockBridge.getSaves())).toEqual([]);
    await page.evaluate(outcome => {
      const bridge = (window as any).__mockBridge;
      if (outcome === 'accepted') bridge.acceptHeld();
      else bridge.refuseHeld('busy');
    }, result);
    await page.evaluate(() => (window as any).__pendingPatchLoad);
    await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
    expect(await page.evaluate(() => (window as any).__mockBridge.getSaves())).toEqual([]);
    if (result === 'refused') expect(await state(page)).toEqual(before);
    else expect((await state(page) as any).future).toEqual(refused.future);
    const selected = await state(page);
    await page.evaluate(() => (window as any).__mockBridge.menu('app.menuSavePatch'));
    await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getSaves().length)).toBe(1);
    expect(await page.evaluate(() => JSON.parse((window as any).__mockBridge.getSaves()[0].text)))
      .toEqual(selected);
  });
}
