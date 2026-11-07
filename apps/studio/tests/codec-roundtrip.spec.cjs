// SPDX-License-Identifier: GPL-3.0-only
const { test, expect } = require('@playwright/test');
const { readFileSync, readdirSync, writeFileSync } = require('node:fs');
const { resolve } = require('node:path');
const { execFileSync } = require('node:child_process');
const { PatchDoc } = require('../src/patch/PatchDoc');
const { EMPTY_PATCH } = require('../src/patch/types');
const { STARTERS } = require('../src/app/starters');
const catalogue = require('../public/wirescape_modules.json');
const { boot, settle } = require('./helpers');

const root = resolve(process.cwd(), '../..');
test('native starter fixtures are actual UI exports; catalogue has no drift', () => {
  execFileSync(process.platform === 'win32' ? 'python' : 'python3', ['tools/gen_ui_catalogue.py', '--check'], { cwd: root });
  const exports = STARTERS.map(s => ({ id: s.id, patch: PatchDoc.from(s.patch()).toJSON() }));
  const fixture = resolve(root, 'native/document/ui_starters.json');
  if (process.env.WIRESCAPE_WRITE_FIXTURES === '1') {
    writeFileSync(fixture, '[\n' + exports.map(s => ' ' + JSON.stringify(s)).join(',\n') + '\n]\n');
  }
  expect(JSON.parse(readFileSync(fixture, 'utf8'))).toEqual(exports);
  for (const { patch } of exports) {
    expect(patch.settings).toBeDefined();
    expect(patch.outputs.length).toBeGreaterThan(0);
    for (const m of patch.modules) expect(m.version).toBeGreaterThan(0);
  }
});

test('imports preserve absent metadata; output edits preserve other routes and opaque values', () => {
  const source = { ...EMPTY_PATCH(), settings: { sample_rate: 96000, future: { opaque: [1, null] } },
    modules: [{ id: 1, module: 'essentials.output', version: 71, state: { opaque: true } },
      { id: 2, module: 'essentials.output' }],
    outputs: [{ module: 1, port: 'audio', bus_channel: 7, future: { value: 'keep' } },
      { module: 2, port: 'audio', bus_channel: 15, opaque: [false, 3] }] };
  const original = structuredClone(source);
  const doc = PatchDoc.from(source);
  expect(doc.toJSON().settings).toEqual(source.settings);
  expect(doc.toJSON().modules[0]).toMatchObject(source.modules[0]);
  expect(doc.toJSON()).toEqual(source);
  expect(Object.hasOwn(doc.toJSON().modules[1], 'version')).toBe(false);
  doc.addModule('essentials.output', {}, 0, 0, 3);
  const stereo = [0, 1].map(bus_channel => ({ module: 3, port: 'audio', bus_channel }));
  expect(doc.toJSON().outputs).toEqual([...source.outputs, ...stereo]);
  doc.removeModule(1);
  expect(doc.toJSON().outputs).toEqual([source.outputs[1], ...stereo]);
  doc.removeModule(3);
  expect(doc.toJSON().outputs).toEqual([source.outputs[1]]);
  expect(source).toEqual(original);
  const missing = PatchDoc.from({ ...EMPTY_PATCH(), modules: [{ id: 1, module: 'essentials.output' }] });
  expect(Object.hasOwn(missing.toJSON(), 'settings')).toBe(false);
  expect(Object.hasOwn(missing.toJSON(), 'outputs')).toBe(false);
  missing.addModule('essentials.output', {}, 0, 0, 2);
  expect(Object.hasOwn(missing.toJSON(), 'outputs')).toBe(false);
  missing.removeModule(1);
  expect(Object.hasOwn(missing.toJSON(), 'outputs')).toBe(false);
  const empty = PatchDoc.from({ ...EMPTY_PATCH(), settings: {}, outputs: [] });
  expect(empty.toJSON().settings).toEqual({});
  expect(empty.toJSON().outputs).toEqual([]);
  const authored = new PatchDoc();
  authored.addModule('essentials.output', {}, 0, 0, 7);
  expect(authored.toJSON().modules[0].version).toBe(catalogue.modules.find(m => m.id === 'essentials.output').version);
  expect(authored.toJSON().outputs).toEqual([0, 1].map(bus_channel => ({ module: 7, port: 'audio', bus_channel })));
});

test('pasting an imported module without parameters sends an empty parameter map', async ({ page }) => {
  await boot(page);
  const result = await page.evaluate(() => {
    const api = window.__wirescapeTest;
    const before = new Set(api.save().modules.map(m => m.id));
    localStorage.setItem('wirescape.clipboard.v1', JSON.stringify({
      modules: [{ id: 91, module: 'essentials.vco', pos: [0, 0], extra: {} }],
      connections: [], colors: {},
    }));
    const original = api.engine.addModule.bind(api.engine);
    let passed;
    api.engine.addModule = (id, type, params) => {
      passed = params;
      return original(id, type, params);
    };
    api.paste();
    const added = api.save().modules.find(m => !before.has(m.id));
    return { passed, params: added?.parameters, module: added?.module };
  });
  expect(result).toEqual({ passed: {}, params: {}, module: 'essentials.vco' });
});

test('opening and automatically fitting T02 does not edit the saved source', async ({ page }) => {
  await boot(page);
  const source = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output' }], connections: [], ui: { labels: { 7: 'Out' } } };
  await page.evaluate(p => window.__wirescapeTest.load(p, 'T02 source'), source);
  await settle(page);
  expect(await page.evaluate(() => window.__wirescapeTest.save())).toEqual(source);
  await page.evaluate(() => window.__wirescapeTest.doc.setLabel(7, 'Renamed'));
  expect(await page.evaluate(() => window.__wirescapeTest.save())).toEqual({ ...source, ui: { labels: { 7: 'Renamed' } } });
});

test('T02 source fields stay absent until authored; snapshots cannot mutate the document', () => {
  const source = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output', opaque: { bytes: ['λ', null] } }],
    connections: [], ui: { labels: { 7: 'Out' }, opaque: { future: true } } };
  const doc = PatchDoc.from(source);
  expect(doc.toJSON()).toEqual(source);
  const snapshot = doc.toJSON();
  snapshot.modules[0].opaque.bytes.push('snapshot only');
  snapshot.ui.opaque.future = false;
  expect(doc.toJSON()).toEqual(source);
  doc.setPosition(7, 11, 22);
  doc.setParam(7, 'gain', 0.5);
  expect(doc.toJSON()).toEqual({ ...source,
    modules: [{ ...source.modules[0], parameters: { gain: 0.5 } }],
    ui: { ...source.ui, positions: { 7: [11, 22] } } });
  expect(source.modules[0]).not.toHaveProperty('parameters');
});

test('no-ui and explicit empty execution fields round trip without healing', () => {
  const source = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 7, module: 'essentials.output' }], connections: [] };
  const doc = PatchDoc.from(source);
  expect(doc.toJSON()).toEqual(source);
  doc.removeModule(7);
  expect(doc.toJSON()).toEqual({ ...source, modules: [] });
  for (const outputs of [[], [{ module: 99, port: 'missing', bus_channel: 3 }]]) {
    const explicit = { ...source, settings: {}, outputs,
      modules: [{ ...source.modules[0], version: 71, parameters: {} }], ui: {} };
    expect(PatchDoc.from(explicit).toJSON()).toEqual(explicit);
  }
});

for (const filename of readdirSync(resolve(root, 'native/examples')).filter(p => p.endsWith('.json'))) {
  test(`native example ${filename} loads in the UI and preserves all opaque data`, async ({ page }) => {
    const source = JSON.parse(readFileSync(resolve(root, 'native/examples', filename), 'utf8'));
    source.future = { bytes: 'AAEC/w==', nested: [{ keep: false }, null, 'λ'] };
    source.modules[0].future = structuredClone(source.future);
    source.connections[0].future = structuredClone(source.future);
    if (source.version === 2) {
      source.settings.future = structuredClone(source.future);
      source.connections[0].from.future = structuredClone(source.future);
      source.outputs[0].future = structuredClone(source.future);
    }
    source.ui = { ...source.ui, future: structuredClone(source.future),
      view: { x: 0, y: 0, zoom: 1, future: structuredClone(source.future) } };
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await boot(page);
    await page.evaluate(source => window.__wirescapeTest.load(source, 'Native round trip'), source);
    await settle(page);
    const saved = await page.evaluate(() => window.__wirescapeTest.save());
    expect(saved.future).toEqual(source.future);
    expect(saved.ui.future).toEqual(source.ui.future);
    expect(saved.ui.view.future).toEqual(source.ui.view.future);
    if (source.version === 1) {
      expect(saved.outputs).toEqual([0, 1].map(bus_channel => ({ module: source.outputNode, port: 'audio', bus_channel })));
      expect(saved.preserved).toEqual(source);
      source.modules.forEach((m, i) => expect(saved.modules[i].preserved).toEqual(m));
      expect(saved.modules.every((m) => typeof m.module === 'string')).toBe(true);
    } else {
      for (const key of Object.keys(source).filter(k => k !== 'ui')) expect(saved[key], key).toEqual(source[key]);
    }
    expect(errors).toEqual([]);
    await expect(page.locator('[data-testid^="module-header-"]')).toHaveCount(source.modules.length);
  });
}
