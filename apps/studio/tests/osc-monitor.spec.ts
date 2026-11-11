// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';

const patch = () => ({
  format: 'wirescape.native.patch' as const, version: 2 as const,
  modules: [
    { id: 1, module: 'osc.in' },
    { id: 2, module: 'monitor' },
    { id: 3, module: 'essentials.output' },
  ],
  connections: [{ kind: 'event', from: { module: 1, port: 'messages' },
    to: { module: 2, port: 'events' } }],
  ui: { positions: { 1: [10, 40], 2: [480, 40], 3: [850, 40] },
    view: { x: 0, y: 0, zoom: 1 } },
});

const log = (page: Page) => page.getByTestId('monitor-lines-2');
const push = async (page: Page, entries: unknown[]) => {
  await page.evaluate(entries => window.__wirescapeTest.engine.setMonitorEntries(2, entries), entries);
  await settle(page);
};

test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await page.evaluate(() => { window.__wirescapeTest.engine.debug.oscTraffic = false; });
  await settle(page);
});

test('direct-only empty state, bounded oldest-first log and 64-byte address', async ({ page }, testInfo) => {
  await expect(log(page)).toContainText('direct OSC/MIDI only');
  const items = Array.from({ length: 40 }, (_, i) => ({
    t: (i + 1) / 10, kind: 'osc', address: `/knob/${i}`, args: [i / 40],
  }));
  await push(page, items);
  const lines = log(page).locator('.monitor-line');
  await expect(lines).toHaveCount(32);
  await expect(lines.first()).toContainText('/knob/8');
  await expect(lines.last()).toContainText('/knob/39');
  const patchAfter = await state(page);
  expect(patchAfter.modules.find((m: any) => m.id === 2)?.config).toBeUndefined();
  await page.getByTestId('monitor-log-2').screenshot({ path: testInfo.outputPath('monitor-log.png') });
  await testInfo.attach('Monitor log', { path: testInfo.outputPath('monitor-log.png'), contentType: 'image/png' });
});

test('Pause freezes only the display; Resume shows the newest snapshot', async ({ page }) => {
  await push(page, [{ t: 0.1, kind: 'osc', address: '/first', args: [0.2] }]);
  await expect(log(page)).toContainText('/first');
  await page.getByTestId('monitor-pause-2').click();
  await expect(page.getByTestId('monitor-pause-2')).toHaveAttribute('aria-pressed', 'true');
  const tick = await page.evaluate(() => window.__wirescapeTest.ticker.last?.tick ?? 0);
  await push(page, [{ t: 0.2, kind: 'midi', note: 60, args: [9, 127] }]);
  await page.waitForFunction(t => (window.__wirescapeTest.ticker.last?.tick ?? 0) > t, tick);
  await expect(log(page)).not.toContainText('note 60');
  await page.getByTestId('monitor-pause-2').click();
  await expect(log(page)).toContainText('MIDI · note 60');
});

test('Clear is local to Studio and only later ingress appears again', async ({ page }) => {
  await push(page, [
    { t: 0.1, kind: 'osc', address: '/old', args: [1] },
    { t: 0.2, kind: 'osc', address: '/latest', args: [2] },
  ]);
  await expect(log(page)).toContainText('/latest');
  await page.getByTestId('monitor-clear-2').click();
  await expect(log(page)).toContainText('direct OSC/MIDI only');
  expect(await page.evaluate(() => window.__wirescapeTest.engine['monitorEntries'][2].length)).toBe(2);
  await push(page, [
    { t: 0.1, kind: 'osc', address: '/old', args: [1] },
    { t: 0.2, kind: 'osc', address: '/latest', args: [2] },
    { t: 0.3, kind: 'osc', address: '/new', args: [3] },
  ]);
  await expect(log(page)).toContainText('/new');
  await expect(log(page)).not.toContainText('/old');
});

test('missing engine capability displays not available; new graph clears old log', async ({ page }) => {
  await push(page, [{ t: 0.1, kind: 'osc', address: '/first', args: [1] }]);
  await expect(log(page)).toContainText('/first');
  await page.evaluate(() => { window.__wirescapeTest.engine.monitorAvailable = false; });
  await expect(log(page)).toContainText('not available');
  await page.evaluate(() => { window.__wirescapeTest.engine.monitorAvailable = true; });
  await expect(log(page)).toContainText('/first');
  await page.evaluate(p => window.__wirescapeTest.load(p), patch());
  await settle(page);
  await expect(log(page)).toContainText('direct OSC/MIDI only');
});

test('FakeEngine emits sample OSC observations only for direct OSC-to-Monitor wiring', async ({ page }) => {
  await page.evaluate(() => { window.__wirescapeTest.engine.debug.oscTraffic = true; });
  await expect(log(page)).toContainText('/pico1/knob/1', { timeout: 5000 });
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'osc.in' }, { id: 2, module: 'monitor' }, { id: 3, module: 'essentials.output' }],
    connections: [], ui: { positions: {}, view: { x: 0, y: 0, zoom: 1 } },
  }));
  await settle(page);
  await expect(log(page)).toContainText('direct OSC/MIDI only');
});
