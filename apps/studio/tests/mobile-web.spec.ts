// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, settle, state } from './helpers';

test.use({ viewport: { width: 390, height: 667 }, hasTouch: true, isMobile: true });

test('iPhone-sized chrome stays on screen and every toolbar action remains reachable', async ({ page }) => {
  await boot(page);
  const layout = await page.evaluate(() => {
    const top = document.querySelector('.topbar')!.getBoundingClientRect();
    const foot = document.querySelector('.statusbar')!.getBoundingClientRect();
    const viewport = document.querySelector('.viewport')!.getBoundingClientRect();
    const actions = document.querySelector('.topbar-right') as HTMLElement;
    return { top: { top: top.top, bottom: top.bottom }, foot: { top: foot.top, bottom: foot.bottom },
      canvasHeight: viewport.height, scrollWidth: document.documentElement.scrollWidth,
      width: innerWidth, height: innerHeight, actionsCanScroll: actions.scrollWidth > actions.clientWidth };
  });
  expect(layout.top.top).toBeGreaterThanOrEqual(-1);
  expect(layout.top.bottom).toBeLessThan(layout.height);
  expect(layout.foot.bottom).toBeLessThanOrEqual(layout.height + 1);
  expect(layout.foot.top).toBeGreaterThan(layout.top.bottom);
  expect(layout.canvasHeight).toBeGreaterThan(100);
  expect(layout.scrollWidth).toBeLessThanOrEqual(layout.width + 1);
  expect(layout.actionsCanScroll).toBe(true);
  await page.getByTestId('save-patch').scrollIntoViewIfNeeded();
  await expect(page.getByTestId('save-patch')).toBeInViewport();
});

test('two touch points spanning a module and background zoom without dragging the module', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await page.evaluate(() => window.__wirescapeTest.fit());
  await settle(page);
  const id = (await state(page)).modules[0].id as number;
  const before = await state(page);
  const header = await page.getByTestId(`module-header-${id}`).boundingBox();
  const canvas = await page.getByTestId('viewport').boundingBox();
  expect(header).not.toBeNull();
  expect(canvas).not.toBeNull();

  const p1 = { x: Math.round(header!.x + Math.min(50, header!.width / 2)),
    y: Math.round(header!.y + header!.height / 2), id: 1 };
  const p2 = { x: Math.round(canvas!.x + canvas!.width - 22),
    y: Math.round(canvas!.y + canvas!.height / 2), id: 2 };
  const cdp = await page.context().newCDPSession(page);
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [p1] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [p1, p2] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove',
    touchPoints: [p1, { ...p2, x: p2.x - 90 }] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [p1] });
  await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
  await settle(page);

  const after = await state(page);
  expect(after.ui.view.zoom).toBeLessThan(before.ui.view.zoom * 0.95);
  expect(after.ui.positions[String(id)]).toEqual(before.ui.positions[String(id)]);
  expect(await page.getByTestId('viewport').getAttribute('class')).not.toContain('panning');
  await cdp.detach();
});
