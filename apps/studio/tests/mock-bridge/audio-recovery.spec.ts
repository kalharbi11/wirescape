// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import { boot, state } from '../helpers';
import { mockBridge } from './bridge';

test('T386 recovery: real protocol status sequence preserves patch and restores meters', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');

  const patch = {
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output', config: { opaque: 'keep' },
      extraModuleData: { future: 17 } }],
    connections: [], customUserData: { keep: ['unknown', 1] },
  };
  await page.evaluate(p => window.__wirescapeTest.load(p), patch);
  await expect.poll(async () => (await page.evaluate(() =>
    (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  const saved = await state(page);
  expect(saved.customUserData).toEqual(patch.customUserData);
  expect(saved.modules[0].extraModuleData).toEqual(patch.modules[0].extraModuleData);
  const before = await page.evaluate(() => ({
    session: (window as any).__mockBridge.getSession(),
    generation: (window as any).__mockBridge.generation(),
    loads: (window as any).__mockBridge.sent.filter((m: any) => m.type === 'loadGraph').length,
  }));

  const status = (state: string, cpuPercent: number) => page.evaluate(
    value => (window as any).__mockBridge.status(value),
    { state, cpuPercent, dropouts: 1 });
  await status('running', 5);
  await expect(page.getByTestId('status-audio')).toHaveText('RUNNING');
  await status('device-lost', 0);
  await expect(page.getByTestId('status-audio')).toHaveText('DEVICE LOST');
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU not available');
  await status('reconnecting', 0);
  await expect(page.getByTestId('status-audio')).toHaveText('RECONNECTING');
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU not available');
  await page.evaluate(() => (window as any).__mockBridge.devices());
  await status('running', 6);
  await expect(page.getByTestId('status-audio')).toHaveText('RUNNING');
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU 6%');

  await page.evaluate(() => (window as any).__mockBridge.telemetry({ meters: { 7: [0.25, 0.4] } }));
  await expect.poll(async () => page.evaluate(() =>
    window.__wirescapeTest.ticker.last?.meters[7])).toEqual([0.25, 0.4]);
  expect(await state(page)).toEqual(saved);
  const after = await page.evaluate(() => ({
    session: (window as any).__mockBridge.getSession(),
    generation: (window as any).__mockBridge.generation(),
    loads: (window as any).__mockBridge.sent.filter((m: any) => m.type === 'loadGraph').length,
    restarts: (window as any).__mockBridge.sent.filter((m: any) => m.type === 'app.restartEngine').length,
  }));
  expect(after).toMatchObject({ ...before, restarts: 0 });
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});
