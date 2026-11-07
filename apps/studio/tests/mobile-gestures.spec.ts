// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle } from './helpers';

// Emulate an iPhone-sized touch viewport. Chromium covers pointer capture and
// responsive layout; physical iOS Safari remains a separate acceptance check.
test.use({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true, deviceScaleFactor: 2 });

interface Finger { x: number; y: number }

async function pinch(page: Page, from: [Finger, Finger], to: [Finger, Finger]): Promise<void> {
  const cdp = await page.context().newCDPSession(page);
  const points = (a: Finger, b: Finger) => [
    { ...a, id: 1 }, { ...b, id: 2 },
  ];
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ ...from[0], id: 1 }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: points(...from) });
  for (let i = 1; i <= 8; i++) {
    const t = i / 8;
    const at = from.map((p, j) => ({
      x: p.x + (to[j]!.x - p.x) * t,
      y: p.y + (to[j]!.y - p.y) * t,
    })) as [Finger, Finger];
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: points(...at) });
  }
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [{ ...to[1], id: 2 }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  await cdp.detach();
  await settle(page);
}

const view = (page: Page) => page.evaluate(() => window.__wirescapeTest.state().ui.view as
  { x: number; y: number; zoom: number });

async function visibleChrome(page: Page): Promise<void> {
  const metrics = await page.evaluate(() => {
    const top = document.querySelector('.topbar')!.getBoundingClientRect();
    const foot = document.querySelector('.statusbar')!.getBoundingClientRect();
    const canvas = document.querySelector('.viewport')!.getBoundingClientRect();
    return { h: window.innerHeight, top: { top: top.top, bottom: top.bottom },
      foot: { top: foot.top, bottom: foot.bottom },
      canvas: { top: canvas.top, bottom: canvas.bottom } };
  });
  expect(metrics.top.top).toBeGreaterThanOrEqual(-1);
  expect(metrics.top.bottom).toBeLessThan(metrics.canvas.top + 1);
  expect(metrics.foot.top).toBeGreaterThanOrEqual(metrics.canvas.bottom - 1);
  expect(metrics.foot.bottom).toBeLessThanOrEqual(metrics.h + 1);
}

test('empty-canvas pinch zooms both directions without hiding mobile chrome', async ({ page }) => {
  await boot(page);
  await visibleChrome(page);
  await pinch(page, [{ x: 135, y: 300 }, { x: 245, y: 300 }],
    [{ x: 80, y: 300 }, { x: 300, y: 300 }]);
  const enlarged = await view(page);
  expect(enlarged.zoom).toBeGreaterThan(1.5);
  await visibleChrome(page);
  await pinch(page, [{ x: 80, y: 300 }, { x: 300, y: 300 }],
    [{ x: 160, y: 310 }, { x: 220, y: 310 }]);
  expect((await view(page)).zoom).toBeLessThan(enlarged.zoom * 0.6);
  await visibleChrome(page);

  const toolbar = page.locator('.topbar-right');
  expect(await toolbar.evaluate(el => el.scrollWidth)).toBeGreaterThanOrEqual(
    await toolbar.evaluate(el => el.clientWidth));
  await toolbar.evaluate(el => { el.scrollLeft = el.scrollWidth; });
  await expect(page.getByTestId('save-patch')).toBeInViewport();
  await expect(page.getByTestId('zoom-out')).toBeAttached();
});

test('pinch beginning on a module header and knob zooms without editing either', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'essentials.vco' }], connections: [],
    ui: { positions: { 1: [25, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
  }, 'Mobile touch'));
  await page.evaluate(() => window.__wirescapeTest.zoomTo(0.5));
  await settle(page);
  await page.evaluate(() => window.__wirescapeTest.canvas.selectModule(1, false));
  const head = await page.getByTestId('module-header-1').boundingBox();
  const knob = await page.getByTestId('module-1').locator('.dial-wrap').first().boundingBox();
  expect(head).not.toBeNull();
  expect(knob).not.toBeNull();
  const a = { x: head!.x + Math.min(38, head!.width / 4), y: head!.y + head!.height / 2 };
  const b = { x: knob!.x + knob!.width / 2, y: knob!.y + knob!.height / 2 };
  const before = await page.evaluate(() => window.__wirescapeTest.state());
  const centre = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
  await pinch(page, [a, b], [
    { x: centre.x + (a.x - centre.x) * 1.6, y: centre.y + (a.y - centre.y) * 1.6 },
    { x: centre.x + (b.x - centre.x) * 1.6, y: centre.y + (b.y - centre.y) * 1.6 },
  ]);
  const after = await page.evaluate(() => window.__wirescapeTest.state());
  expect(after.ui.view.zoom).toBeGreaterThan(before.ui.view.zoom);
  expect(after.ui.positions).toEqual(before.ui.positions);
  expect(after.modules[0].parameters).toEqual(before.modules[0].parameters);
  await expect(page.getByTestId('module-1')).toHaveClass(/selected/);
  await visibleChrome(page);
});

test('phone UI keeps patch context, touch defaults and an easily dismissed library', async ({ page }, testInfo) => {
  await boot(page);
  await expect(page.getByTestId('patch-name')).toBeVisible();
  await expect(page.getByTestId('patch-name')).toHaveText('Test');
  await expect(page.locator('html')).toHaveAttribute('data-hit', 'touch');

  const menu = page.getByTestId('menu-toggle');
  await expect(menu).toHaveAttribute('aria-expanded', 'false');
  await menu.click();
  await expect(menu).toHaveAttribute('aria-expanded', 'true');
  await expect(page.getByTestId('library-backdrop')).toBeVisible();
  await expect(page.getByTestId('library')).toBeInViewport();
  await testInfo.attach('mobile-library-open.png', {
    body: await page.screenshot({ animations: 'disabled' }), contentType: 'image/png',
  });

  // Tapping outside the 240px drawer must return to patching, not leave an
  // invisible overlay that intercepts gestures or keyboard navigation.
  await page.mouse.click(375, 240);
  await expect(menu).toHaveAttribute('aria-expanded', 'false');
  await expect(page.getByTestId('library-backdrop')).toBeHidden();
  // Escape (hardware keyboard on a tablet/phone) must close the scrim too.
  await menu.click();
  await expect(page.getByTestId('library-backdrop')).toBeVisible();
  await page.keyboard.press('Escape');
  await expect(menu).toHaveAttribute('aria-expanded', 'false');
  await expect(page.getByTestId('library-backdrop')).toBeHidden();
  await menu.click();
  await page.getByTestId('library-item-essentials.vco').click();
  await expect(menu).toHaveAttribute('aria-expanded', 'false');
  await expect(page.locator('.module')).toHaveCount(1);
  await testInfo.attach('mobile-canvas.png', {
    body: await page.screenshot({ animations: 'disabled' }), contentType: 'image/png',
  });
});
