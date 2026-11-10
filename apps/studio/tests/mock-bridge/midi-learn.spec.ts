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

// ---- #500 review: a Learn replan never outlives the document that made it ----

type Page = import('@playwright/test').Page;
const patchA = { format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 2, module: 'essentials.vcf' }], connections: [],
  ui: { positions: { '2': [900, 80] }, labels: {}, view: { x: 0, y: 0, zoom: 1 } } };
const patchB = { format: 'wirescape.native.patch', version: 2,
  modules: [{ id: 7, module: 'essentials.vco' }], connections: [], ownerB: { keep: true } };
const graph = (page: Page) => page.evaluate(() => (window as any).__mockBridge.getGraph());
const depth = (page: Page) => page.evaluate(() => window.__wirescapeTest.historyDepth());
const refuseHeld = (page: Page) => page.evaluate(() => (window as any).__mockBridge.refuseHeld('refused'));

async function heldLearn(page: Page) {
  await mockBridge(page); await boot(page);
  await page.evaluate(p => { window.__wirescapeTest.load(p, 'A'); window.__wirescapeTest.zoomTo(1); }, patchA);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  await expect.poll(async () => (await graph(page)).modules.length).toBe(1);
  await learnKnob(page, 2, 'cutoff', 74);
  const before = await state(page), graphBefore = await graph(page), historyBefore = await depth(page);
  await page.evaluate(() => (window as any).__mockBridge.holdNext('loadGraph'));
  await page.getByTestId('learn-confirm').click();
  await expect.poll(async () => (await state(page)).modules.length).toBe(3);
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
  await expect(page.getByTestId('midi-learn-applying')).toBeVisible();
  expect(await depth(page)).toEqual({ past: 1, future: 0 });
  return { before, graphBefore, historyBefore };
}

async function openB(page: Page) {
  await page.evaluate(b => {
    const m = (window as any).__mockBridge;
    m.setOpen({ name: 'B.wirescape.json', text: JSON.stringify(b) });
    m.menu('app.menuOpenPatch');
  }, patchB);
}

async function expectB(page: Page) {
  await expect(page.getByTestId('patch-name')).toHaveText('B.wirescape.json');
  await expect.poll(async () => (await graph(page)).modules.map((m: any) => m.id)).toEqual([7]);
  const s = await state(page);
  expect(s.modules.map((m: any) => m.id)).toEqual([7]);
  expect(s.ownerB).toEqual({ keep: true });
  expect(await depth(page)).toEqual({ past: 0, future: 0 });
  // B, opened from a file, keeps its own native Save target.
  await page.evaluate(() => (window as any).__mockBridge.menu('app.menuSavePatch'));
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.getSaves().length)).toBe(1);
  const save = await page.evaluate(() => (window as any).__mockBridge.getSaves()[0]);
  expect(save.saveAs).toBe(false);
  expect(JSON.parse(save.text).modules.map((m: any) => m.id)).toEqual([7]);
}

test('OPEN B while the Learn replan is held: the accepted Learn runs on A, then B opens untouched', async ({ page }) => {
  await heldLearn(page); await openB(page);
  await page.evaluate(() => (window as any).__mockBridge.acceptHeld());
  await expectB(page);
});

test('OPEN B while the Learn replan is held: its refusal never restores A over B', async ({ page }) => {
  await heldLearn(page); await openB(page);
  await refuseHeld(page);
  await expectB(page);
});

test('UNDO is refused during held Learn; after settlement undo and redo are exact', async ({ page }) => {
  const { before, graphBefore, historyBefore } = await heldLearn(page);
  const submitted = await state(page);
  await page.evaluate(() => window.__wirescapeTest.undo());
  expect(await state(page)).toEqual(submitted); // no intermediate document mutation
  expect(await depth(page)).toEqual({ past: 1, future: 0 });
  await refuseHeld(page);
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  expect(await state(page)).toEqual(before);
  await expect.poll(() => graph(page)).toEqual(graphBefore);
  expect(await depth(page)).toEqual(historyBefore);

  // A later, accepted Learn is a single undo step after the pending window.
  await learnKnob(page, 2, 'cutoff', 74);
  await page.getByTestId('learn-confirm').click();
  await expect.poll(async () => (await graph(page)).modules.length).toBe(3);
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await page.evaluate(() => window.__wirescapeTest.undo());
  await expect.poll(async () => (await graph(page)).modules.length).toBe(1);
  expect(await state(page)).toEqual(before);
  await page.evaluate(() => window.__wirescapeTest.redo());
  await expect.poll(async () => (await graph(page)).modules.length).toBe(3);
  expect((await state(page)).modules).toHaveLength(3);
});

test('held Learn rejects dependent edits, then restores exact graph, document and history on refusal', async ({ page }) => {
  const { before, graphBefore, historyBefore } = await heldLearn(page);
  const submitted = await state(page);
  const cc = submitted.modules.find((m: any) => m.module === 'midi.cc_to_cv')!;
  const originalGraph = await graph(page);
  await page.evaluate(id => {
    const api = window.__wirescapeTest;
    api.commitParam(2, 'resonance', 0.4);
    api.connect({ kind: 'modulation', from: { module: id, port: 'cc2' },
      to: { module: 2, port: 'resonance' }, amount: 1 });
    api.addModule('essentials.noise');
    api.undo();
  }, cc.id);
  expect(await state(page)).toEqual(submitted);
  expect(await graph(page)).toEqual(originalGraph);
  expect(await page.evaluate(() => window.__wirescapeTest.canEdit())).toBe(false);
  await refuseHeld(page);
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect(page.getByTestId('midi-learn-applying')).toBeHidden();
  expect(await state(page)).toEqual(before);
  await expect.poll(() => graph(page)).toEqual(graphBefore);
  expect(await depth(page)).toEqual(historyBefore);
  await expect(page.getByTestId('toast').last()).toContainText('taken back');

  // A change submitted AFTER refusal has settled is allowed.
  await page.evaluate(() => window.__wirescapeTest.commitParam(2, 'resonance', 0.4));
  await expect.poll(async () => (await graph(page)).modules[0]?.parameters?.resonance).toBe(0.4);
});

test('held Learn acknowledgment unlocks editing only after the atomic graph is accepted', async ({ page }) => {
  await heldLearn(page);
  await page.evaluate(() => window.__wirescapeTest.commitParam(2, 'resonance', 0.5));
  expect((await state(page)).modules.find((m: any) => m.id === 2).parameters?.resonance).toBeUndefined();
  await page.evaluate(() => (window as any).__mockBridge.acceptHeld());
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect(page.getByTestId('midi-learn-applying')).toBeHidden();
  await expect.poll(async () => (await graph(page)).modules.length).toBe(3);
  await page.evaluate(() => window.__wirescapeTest.commitParam(2, 'resonance', 0.5));
  await expect.poll(async () => (await graph(page)).modules.find((m: any) => m.id === 2).parameters.resonance).toBe(0.5);
});

test('Forget replan is also serialized and its refusal retains the learned cable', async ({ page }) => {
  await heldLearn(page);
  await page.evaluate(() => (window as any).__mockBridge.acceptHeld());
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  await expect.poll(async () => (await graph(page)).modules.length).toBe(3);
  const before = await state(page), graphBefore = await graph(page), historyBefore = await depth(page);
  await page.getByTestId('knob-2-cutoff').click({ button: 'right' });
  await page.evaluate(() => (window as any).__mockBridge.holdNext('loadGraph'));
  await page.getByTestId('context-forget').click();
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', true);
  const submitted = await state(page);
  expect(submitted.connections.filter((c: any) => c.kind === 'modulation')).toHaveLength(0);
  await page.evaluate(() => window.__wirescapeTest.commitParam(2, 'resonance', 0.3));
  expect(await state(page)).toEqual(submitted);
  await refuseHeld(page);
  await expect(page.getByTestId('app')).toHaveJSProperty('inert', false);
  expect(await state(page)).toEqual(before);
  await expect.poll(() => graph(page)).toEqual(graphBefore);
  expect(await depth(page)).toEqual(historyBefore);
});
