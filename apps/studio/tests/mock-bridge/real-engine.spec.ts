// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import { boot, state } from '../helpers';
import { mockBridge } from './bridge';

test.beforeEach(async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});

test('T01, T02, T09, T10: hello catalogue, correlated commands and real device lists', async ({ page }) => {
  await expect(page.getByTestId('library-item-essentials.output')).toBeVisible();
  await page.getByTestId('open-settings').click();
  await expect(page.getByTestId('audio-device')).toContainText('USB Stereo');
  await expect(page.getByTestId('midi-device')).toContainText('M8 MIDI 1');
  const wire = await page.evaluate(() => (window as any).__mockBridge.sent as any[]);
  expect(wire[0]).toMatchObject({ v: 0, type: 'loadGraph' });
  expect(wire.every(m => typeof m.id === 'string' && m.id.length <= 64)).toBe(true);
  const audio = wire.find(m => m.type === 'listAudioDevices');
  const midi = wire.find(m => m.type === 'listMidiDevices');
  expect(audio?.id).not.toBe(midi?.id);
  await page.getByTestId('audio-device').selectOption('USB Stereo');
  const select = await page.evaluate(() => [...(window as any).__mockBridge.sent].reverse().find(
    (m: any) => m.type === 'selectAudioDevice'));
  expect(select).toMatchObject({ type: 'selectAudioDevice', deviceId: 'USB Stereo' });
  expect(select.id).not.toBe(select.deviceId);
});

test('T03: asynchronous add-module refusal removes only the refused node', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('addModule', 'unknown-module'));
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await expect.poll(async () => (await state(page)).modules.length).toBe(0);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});

test('T04/T05: asynchronous cable refusal restores the editable graph', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.output'));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(2);
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('connect', 'kind-mismatch'));
  await page.evaluate(() => window.__wirescapeTest.connect({
    kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' },
  }));
  await expect.poll(async () => (await state(page)).connections.length).toBe(0);
});

test('T06: async setParam refusal restores the prior displayed value', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.vco'));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  const before = await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'));
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('setParam', 'refused'));
  await page.evaluate(() => window.__wirescapeTest.commitParam(1, 'frequency', 440, true));
  await expect.poll(async () => (await state(page)).modules[0].parameters.frequency).toBe(before);
});

test('T07: setModAmount uses top-level amount, not nested amount', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output' }, { id: 9, module: 'essentials.vco' }],
    connections: [{ kind: 'modulation', from: { module: 9, port: 'audio' },
      to: { module: 9, port: 'frequency' }, amount: 0.2 }],
  }));
  await page.evaluate(() => window.__wirescapeTest.engine.setModAmount({
    kind: 'modulation', from: { module: 9, port: 'audio' },
    to: { module: 9, port: 'frequency' }, amount: 0.2,
  }, -0.5));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).connections[0].amount)
    .toBe(-0.5);
  const cmd = await page.evaluate(() => [...(window as any).__mockBridge.sent].reverse().find(
    (m: any) => m.type === 'setModAmount'));
  expect(cmd.amount).toBe(-0.5);
  expect(cmd.connection.amount).toBe(0.2);
});

test('T13: observed held notes/meters pass through, unsupported telemetry stays empty', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.telemetry({
    heldNotes: [60], meters: { 7: [0.1, 0.1] },
  }));
  await expect.poll(async () => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes))
    .toEqual([60]);
  const t = await page.evaluate(() => window.__wirescapeTest.ticker.last);
  expect(t?.meters[7]).toEqual([0.1, 0.1]);
  expect(t?.scope).toEqual({});
  expect(t?.effective).toEqual({});
  expect(t?.outputs).toBeUndefined();
});

test('T15: exit → restart → hello restores the editable patch and rejects old-session replies', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.output'));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  await page.evaluate(() => {
    (window as any).__mockBridge.holdNext('setParam');
    window.__wirescapeTest.commitParam(1, 'gain', 0.5, true);
  });
  const previous = await page.evaluate(() => (window as any).__mockBridge.getSession());
  await page.evaluate(() => (window as any).__mockBridge.exit(true));
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  expect(await page.evaluate(() => (window as any).__mockBridge.getSession())).not.toBe(previous);
  await page.evaluate(() => (window as any).__mockBridge.releaseHeld());
  expect((await state(page)).modules.length).toBe(1);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});

test('native open/save keeps the renderer-owned document and exact JSON text', async ({ page }) => {
  const patch = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 13, module: 'essentials.output' }], connections: [],
    customUserData: { preserved: true } };
  await page.evaluate(p => (window as any).__mockBridge.setOpen({
    text: JSON.stringify(p), name: 'Opened.wirescape.json',
  }), patch);
  await page.getByTestId('open-patch').click();
  await expect(page.getByTestId('patch-name')).toHaveText('Opened.wirescape.json');
  await page.getByTestId('save-patch').click();
  await expect(page.getByTestId('patch-name')).toHaveText('Saved.wirescape.json');
  const saved = await page.evaluate(() => (window as any).__mockBridge.getSaved());
  expect(JSON.parse(saved).customUserData).toEqual({ preserved: true });
});
