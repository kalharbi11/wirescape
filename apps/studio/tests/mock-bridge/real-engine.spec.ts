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
  await expect(page.getByTestId('status-engine')).toHaveText('engine: restarting');
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  expect(await page.evaluate(() => (window as any).__mockBridge.getSession())).not.toBe(previous);
  await page.evaluate(() => (window as any).__mockBridge.releaseHeld());
  expect((await state(page)).modules.length).toBe(1);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});


test('unexpected engine exit waits for main to restart and never requests app.restartEngine', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.output'));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  const previous = await page.evaluate(() => (window as any).__mockBridge.getSession());
  await page.evaluate(() => (window as any).__mockBridge.exit(false));
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  expect(await page.evaluate(() => (window as any).__mockBridge.getSession())).not.toBe(previous);
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  const sent = await page.evaluate(() => (window as any).__mockBridge.sent as any[]);
  expect(sent.some(m => m.type === 'app.restartEngine')).toBe(false);
});


test('MIDI In dropdown uses enumerated endpoints without normalizing original config', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output' },
      { id: 10, module: 'midi.in', parameters: {}, config: { opaque: 'kept' } }],
    connections: [],
  }));
  const choice = page.getByTestId('midi-endpoint-10');
  await expect(choice.locator('option:checked')).toHaveText('All enabled devices');
  await expect(choice.locator('option[value="M8 MIDI 1|M8:M8 MIDI 1"]')).toHaveCount(1);
  let patch = await state(page);
  expect(patch.modules.find((m: any) => m.id === 10).config).toEqual({ opaque: 'kept' });
  await choice.selectOption('M8 MIDI 1|M8:M8 MIDI 1');
  await expect.poll(async () => (await state(page)).modules.find((m: any) => m.id === 10).config)
    .toEqual({ endpoint: 'M8 MIDI 1', system_name: 'M8:M8 MIDI 1', opaque: 'kept' });
  patch = await state(page);
  expect(patch.modules.find((m: any) => m.id === 10).config.opaque).toBe('kept');
});

test('scripted delayed acknowledgement never means early success', async ({ page }) => {
  await page.evaluate(() => (window as any).__mockBridge.setDelay(150));
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.output'));
  expect((await state(page)).modules.length).toBe(1); // optimistic document
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules.length).toBe(1);
  expect((await state(page)).modules.length).toBe(1);
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


test('callback load and real dropouts preserve unavailable states', async ({ page }) => {
  const status = (audio: Record<string, unknown>) => page.evaluate(value =>
    (window as any).__mockBridge.status(value), audio);
  await status({ state: 'starting', cpuPercent: 0, dropouts: 0 });
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU not available');
  await status({ state: 'running', cpuPercent: 23.5, dropouts: 2 });
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU 24%');
  await expect(page.getByTestId('status-dropouts')).toHaveText('2 dropouts');
  await status({ state: 'running', cpuPercent: 120, dropouts: 3 });
  await expect(page.getByTestId('status-cpu')).toHaveClass(/warn/);
  await status({ state: 'device-lost', cpuPercent: 120, dropouts: 3 });
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU not available');
  await expect(page.getByTestId('status-cpu')).not.toHaveClass(/warn/);
  await expect(page.getByTestId('status-dropouts')).toHaveText('3 dropouts');
  await status({ state: 'running', cpuPercent: null, dropouts: 3 });
  await expect(page.getByTestId('status-cpu')).toHaveText('CPU not available');
});

test('a patch switch the engine refuses puts the still-playing patch back on screen', async ({ page }) => {
  const patch = (name: string, module: string) => ({ format: 'wirescape.native.patch', version: 2, name,
    modules: [{ id: 1, module }, { id: 2, module: 'essentials.output' }],
    connections: [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }],
    outputs: [{ module: 2, port: 'audio', bus_channel: 0 }] });
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Playing'), patch('Playing', 'essentials.vco'));
  await expect.poll(async () =>
    (await page.evaluate(() => (window as any).__mockBridge.getGraph())).modules[0]?.module).toBe('essentials.vco');
  // The sidecar refuses the D5 swap (e.g. audio gap budget) and keeps the previous graph.
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'unavailable'));
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Refused'), patch('Refused', 'essentials.noise'));
  await expect(page.getByTestId('patch-name')).toHaveText('Playing');
  await expect.poll(async () => (await state(page)).modules[0]?.module).toBe('essentials.vco');
  await expect(page.getByText(/Refused could not start: Scripted unavailable\. Still playing Playing\./)).toBeVisible();
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
});

for (const code of ['unavailable', 'busy']) {
  test(`a ${code} patch refusal preserves Undo, Redo and unsaved edits`, async ({ page }) => {
    const patch = (module: string) => ({ format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 1, module }, { id: 2, module: 'essentials.output' }],
      connections: [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }],
      outputs: [{ module: 2, port: 'audio', bus_channel: 0 }] });
    await page.evaluate(p => window.__wirescapeTest.load(p, 'Playing'), patch('essentials.vco'));
    await page.evaluate(() => {
      const api = window.__wirescapeTest;
      api.commitParam(1, 'frequency', 220, true); api.flushHistory();
      api.commitParam(1, 'frequency', 880, true); api.flushHistory();
      api.undo();
    });
    const before = await page.evaluate(() => window.__wirescapeTest.historyDepth());
    expect(before.past).toBeGreaterThan(0);
    expect(before.future).toBe(1);
    await page.evaluate(value => (window as any).__mockBridge.refuseNext('loadGraph', value), code);
    await page.evaluate(p => window.__wirescapeTest.load(p, 'Refused'), patch('essentials.noise'));
    await expect(page.getByTestId('patch-name')).toHaveText('Playing');
    expect(await page.evaluate(() => window.__wirescapeTest.doc.dirty)).toBe(true);
    expect(await page.evaluate(() => window.__wirescapeTest.historyDepth())).toEqual(before);
    expect(await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'))).toBe(220);
    await page.evaluate(() => window.__wirescapeTest.redo());
    expect(await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'))).toBe(880);
    await page.evaluate(() => window.__wirescapeTest.undo());
    expect(await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'))).toBe(220);
  });
}

test('a refused patch load commits a pending gesture to the restored history', async ({ page }) => {
  const patch = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'essentials.vco' }, { id: 2, module: 'essentials.output' }],
    connections: [{ kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'audio' } }],
    outputs: [{ module: 2, port: 'audio', bus_channel: 0 }] };
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Playing'), patch);
  await page.evaluate(async p => {
    window.__wirescapeTest.commitParam(1, 'frequency', 880, true);
    (window as any).__mockBridge.refuseNext('loadGraph', 'unavailable');
    await window.__wirescapeTest.load(p, 'Refused');
  }, { ...patch, modules: [{ id: 1, module: 'essentials.noise' }, patch.modules[1]!] });
  expect(await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'))).toBe(880);
  expect((await page.evaluate(() => window.__wirescapeTest.historyDepth())).past).toBe(1);
  await page.evaluate(() => window.__wirescapeTest.undo());
  expect(await page.evaluate(() => window.__wirescapeTest.doc.param(1, 'frequency'))).not.toBe(880);
});


test('Groove Apply requires current measured STOP, not request ACK or stale generation', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.addModule('sequencing.groove'));
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(1);
  const allowed = () => page.evaluate(() => window.__wirescapeTest.engine.canApplyGroove(1));
  expect(await allowed()).toBe(false);
  await page.evaluate(() => {
    const e = window.__wirescapeTest.engine;
    (window as any).__mockBridge.telemetry({ playhead: { 1: [3, 3] } });
    e.setParam(1, 'stop', 1);
  });
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules[0].parameters.stop)).toBe(1);
  expect(await allowed()).toBe(false);
  await page.evaluate(() => (window as any).__mockBridge.telemetry({ playhead: { 1: [-1, -1] } }, '0'));
  expect(await allowed()).toBe(false);
  await page.evaluate(() => (window as any).__mockBridge.telemetry({ playhead: { 1: [-1, 31] } }));
  expect(await allowed()).toBe(false); // second track still owes release
  await page.evaluate(() => (window as any).__mockBridge.telemetry({ playhead: { 1: [-1, -1] } }));
  expect(await allowed()).toBe(true);
  await page.evaluate(() => (window as any).__mockBridge.telemetry({ playhead: {} }));
  expect(await allowed()).toBe(false); // complete snapshots replace old STOP
  await page.evaluate(() => {
    (window as any).__mockBridge.telemetry({ playhead: { 1: [-1, -1] } });
    window.__wirescapeTest.engine.addModule(2, 'essentials.output');
  });
  expect(await allowed()).toBe(false); // mutating dispatch invalidates evidence immediately
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(2);
  expect(await allowed()).toBe(false);
  await page.evaluate(() => (window as any).__mockBridge.status({ state: 'no-device' }));
  expect(await allowed()).toBe(true);
  await page.evaluate(() => (window as any).__mockBridge.status({ state: 'running' }));
  expect(await allowed()).toBe(false);
});
