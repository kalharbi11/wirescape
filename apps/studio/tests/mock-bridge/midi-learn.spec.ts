// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { boot, state } from '../helpers';
import { mockBridge } from './bridge';

test('RealEngine capability-gates MIDI Learn and forwards latest CC status', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await expect.poll(() => page.evaluate(() => typeof window.__wirescapeTest.engine.midiLearn))
    .toBe('function');
  await page.evaluate(() => {
    (window as any).__learnStates = [];
    window.__wirescapeTest.engine.onStatus((s: any) => (window as any).__learnStates.push(s.midi));
    window.__wirescapeTest.engine.midiLearn(true);
  });
  const state = () => page.evaluate(() => {
    const a = (window as any).__learnStates;
    return a[a.length - 1]?.learn;
  });
  await expect.poll(state).toEqual({ armed: true });
  await page.evaluate(() => {
    const fake = (window as any).__mockBridge;
    fake.captureCc('M8 MIDI 1|M8:M8 MIDI 1', 1, 21, 55);
    fake.captureCc('M8 MIDI 1|M8:M8 MIDI 1', 16, 74, 127);
  });
  await expect.poll(state).toEqual({
    armed: true, seq: '2', deviceId: 'M8 MIDI 1|M8:M8 MIDI 1',
    channel: 16, controller: 74, value: 127,
  });
  await page.evaluate(() => window.__wirescapeTest.engine.midiLearn(false));
  await expect.poll(state).toBeUndefined();
  const sent = await page.evaluate(() => (window as any).__mockBridge.sent
    .filter((x: any) => x.type === 'midiLearn').map((x: any) => x.armed));
  expect(sent).toEqual([true, false]);
  await page.evaluate(() => (window as any).__mockBridge.exit());
  await expect.poll(state).toBeUndefined();
});

test('FakeEngine CC Learn is transient and cannot capture when disarmed', async ({ page }) => {
  await boot(page);
  const observed = await page.evaluate(() => {
    const e = window.__wirescapeTest.engine;
    const captured: unknown[] = [];
    e.onStatus((status: any) => captured.push(status.midi.learn));
    e.emitMidiCc('Arturia KeyStep|midi:0', 1, 21, 20);
    e.midiLearn(true);
    e.emitMidiCc('Arturia KeyStep|midi:0', 1, 21, 20);
    e.emitMidiCc('Arturia KeyStep|midi:0', 2, 30, 100);
    e.midiLearn(false);
    e.emitMidiCc('Arturia KeyStep|midi:0', 3, 40, 120);
    return captured;
  });
  expect(observed).toEqual([
    undefined, { armed: true },
    { armed: true, seq: '1', deviceId: 'Arturia KeyStep|midi:0', channel: 1, controller: 21, value: 20 },
    { armed: true, seq: '2', deviceId: 'Arturia KeyStep|midi:0', channel: 2, controller: 30, value: 100 },
    undefined,
  ]);
});

test('FakeEngine Learn never edits saved CC rows or opaque document fields', async ({ page }) => {
  await boot(page);
  const state = await page.evaluate(() => {
    const api = window.__wirescapeTest;
    api.load({ format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 12, module: 'midi.cc_to_cv',
        config: { rows: [{ channel: 1, controller: 19, bipolar: true }] } }],
      connections: [], ui: { opaque: { retain: 'unknown' } },
    });
    const before = JSON.stringify(api.state());
    api.engine.midiLearn(true);
    api.engine.emitMidiCc('Arturia KeyStep|midi:0', 1, 74, 127);
    api.engine.midiLearn(false);
    return { before, after: JSON.stringify(api.state()) };
  });
  expect(state.after).toEqual(state.before);
});

const learnKnob = async (page: import('@playwright/test').Page, id: number, param: string, controller: number) => {
  await page.getByTestId(`knob-${id}-${param}`).click({ button: 'right' });
  await page.getByTestId('context-midi').click();
  await expect(page.getByTestId('learn-bar')).toBeVisible();
  // The mock engine arms after its reply delay; a CC sent earlier is ignored, so retry until captured.
  await expect(async () => {
    await page.evaluate(c => (window as any).__mockBridge.captureCc('M8 MIDI 1|M8:M8 MIDI 1', 3, c, 64), controller);
    await expect(page.getByTestId('learn-message')).toHaveText(`CC ${controller} · ch 3 · M8 MIDI 1`, { timeout: 500 });
  }).toPass({ timeout: 10_000 });
};

test('Learn over the real bridge sends one loadGraph with the row, cable and MIDI In; Forget sends another', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await page.evaluate(() => {
    window.__wirescapeTest.load({ format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 2, module: 'essentials.vcf' }], connections: [],
      ui: { positions: { '2': [900, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } } }, 'Test');
    window.__wirescapeTest.zoomTo(1);
  });
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(1);
  await page.evaluate(() => { (window as any).__mockBridge.sent.length = 0; });

  await learnKnob(page, 2, 'cutoff', 74);
  await page.getByTestId('learn-confirm').click();
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(3);
  const wire = await page.evaluate(() => (window as any).__mockBridge.sent.filter((m: any) => m.type !== 'midiLearn'
    && !String(m.type).startsWith('list')));
  expect(wire.map((m: any) => m.type)).toEqual(['loadGraph']);
  const graph = await page.evaluate(() => (window as any).__mockBridge.getGraph());
  const cc = graph.modules.find((m: any) => m.module === 'midi.cc_to_cv');
  expect(cc.config).toEqual({ rows: [{ channel: 3, controller: 74 }] });
  expect(graph.connections.filter((c: any) => c.kind === 'modulation').map((c: any) => c.to.port)).toEqual(['cutoff']);
  expect((await state(page)).modules.find((m: any) => m.module === 'midi.cc_to_cv').config).toEqual(cc.config);
  const armed = await page.evaluate(() => (window as any).__mockBridge.sent.filter((m: any) => m.type === 'midiLearn').map((m: any) => m.armed));
  expect(armed).toEqual([true, false]);

  await page.getByTestId('knob-2-cutoff').click({ button: 'right' });
  await page.getByTestId('context-forget').click();
  await expect.poll(() => page.evaluate(() =>
    (window as any).__mockBridge.getGraph().connections.filter((c: any) => c.kind === 'modulation').length)).toBe(0);
});

test('a refused Learn leaves the document and the engine on the previous graph', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await page.evaluate(() => {
    window.__wirescapeTest.load({ format: 'wirescape.native.patch', version: 2,
      modules: [{ id: 2, module: 'essentials.vcf' }], connections: [],
      ui: { positions: { '2': [900, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } } }, 'Test');
    window.__wirescapeTest.zoomTo(1);
  });
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(1);
  await learnKnob(page, 2, 'cutoff', 74);
  await page.evaluate(() => (window as any).__mockBridge.refuseNext('loadGraph', 'refused'));
  await page.getByTestId('learn-confirm').click();
  await expect.poll(async () => (await state(page)).modules.length).toBe(1);
  expect((await state(page)).connections).toEqual([]);
  expect(await page.evaluate(() => (window as any).__mockBridge.getGraph().modules.length)).toBe(1);
});
