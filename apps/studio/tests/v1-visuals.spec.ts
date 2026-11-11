// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, settle, state, addModuleAt, patch, port } from './helpers';

test.describe('v1 owner-reported Studio visuals (#599)', () => {
  test('Random Source displays actual step and smooth history, never a sine phase', async ({ page }, info) => {
    await boot(page);
    const id = await addModuleAt(page, 'example.random_source', 150, 180);
    const history = page.getByTestId(`random-history-${id}`);
    await expect(history).toBeVisible();
    await expect.poll(async () => Number(await history.getAttribute('data-samples'))).toBeGreaterThan(12);
    await expect.poll(async () => Number(await history.getAttribute('data-step-changes'))).toBeGreaterThan(1);
    await expect(history).not.toContainText('phase not available');
    await expect(history).toContainText('SMOOTH');
    await info.attach('random-history.png', { body: await page.screenshot(), contentType: 'image/png' });
  });

  test('Alt-drag unplugs the newest cable at its source, input drag unplugs the other end, Undo restores', async ({ page }, info) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const source = await addModuleAt(page, 'essentials.vco', 70, 180);
    const dest = await addModuleAt(page, 'essentials.vcf', 850, 180);
    const from = { module: source, port: 'audio' };
    const to = { module: dest, port: 'audio' };
    await patch(page, from, to);
    const viewport = await page.getByTestId('viewport').boundingBox();
    const empty = { x: viewport!.x + viewport!.width * 0.8, y: viewport!.y + viewport!.height * 0.8 };

    const sourcePort = await port(page, source, 'output', 'audio').boundingBox();
    await page.keyboard.down('Alt');
    await page.mouse.move(sourcePort!.x + sourcePort!.width / 2, sourcePort!.y + sourcePort!.height / 2);
    await page.mouse.down();
    await page.mouse.move(empty.x, empty.y, { steps: 8 });
    await page.mouse.up();
    await page.keyboard.up('Alt');
    await settle(page);
    expect((await state(page)).connections).toHaveLength(0);
    await expect(page.getByTestId('quick-add')).toBeHidden();
    await page.keyboard.press('Control+z');
    await settle(page);
    expect((await state(page)).connections).toHaveLength(1);

    const destPort = await port(page, dest, 'input', 'audio').boundingBox();
    await page.mouse.move(destPort!.x + destPort!.width / 2, destPort!.y + destPort!.height / 2);
    await page.mouse.down();
    await page.mouse.move(empty.x, empty.y, { steps: 8 });
    await page.mouse.up();
    await settle(page);
    expect((await state(page)).connections).toHaveLength(0);
    await page.keyboard.press('Control+z');
    await settle(page);
    expect((await state(page)).connections).toHaveLength(1);

    // The existing selected-cable Delete path also has a context-menu route.
    const mid = await page.evaluate(() => {
      const p = document.querySelector<SVGPathElement>('.cable-hit')!;
      const q = p.getPointAtLength(p.getTotalLength() / 2).matrixTransform(p.getScreenCTM()!);
      return { x: q.x, y: q.y };
    });
    await page.mouse.click(mid.x, mid.y, { button: 'right' });
    await page.getByTestId('context-remove').click();
    await settle(page);
    expect((await state(page)).connections).toHaveLength(0);
    await page.keyboard.press('Control+z');
    await settle(page);
    expect((await state(page)).connections).toHaveLength(1);
    await info.attach('cable-unplug.png', { body: await page.screenshot(), contentType: 'image/png' });
  });

  test('signal cable activity highlights measured outputs without requiring motion', async ({ page }, info) => {
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const source = await addModuleAt(page, 'essentials.noise', 70, 180);
    const output = await addModuleAt(page, 'essentials.output', 850, 180);
    await patch(page, { module: source, port: 'audio' }, { module: output, port: 'audio' });
    const cable = page.locator('.cable-group .cable');
    await expect(cable).toHaveClass(/active/);
    await expect.poll(() => cable.evaluate(e => getComputedStyle(e).transitionDuration)).toBe('0s');
    await info.attach('cable-activity.png', { body: await page.screenshot(), contentType: 'image/png' });
  });

  test('Output card meter tracks FakeEngine peak telemetry', async ({ page }, info) => {
    await boot(page);
    await page.evaluate(() => window.__wirescapeTest.zoomTo(0.6));
    const source = await addModuleAt(page, 'essentials.noise', 70, 180);
    const output = await addModuleAt(page, 'essentials.output', 850, 180);
    await patch(page, { module: source, port: 'audio' }, { module: output, port: 'audio' });
    const meter = page.getByTestId(`meter-${output}`);
    await expect(meter).toBeVisible();
    await expect.poll(() => meter.locator('.meter-bar').first().evaluate(e => {
      const m = /scaleX\(([^)]+)\)/.exec((e as HTMLElement).style.transform);
      return m ? Number(m[1]) : 0;
    })).toBeGreaterThan(0.05);
    await info.attach('output-meter.png', { body: await page.screenshot(), contentType: 'image/png' });
  });
});
