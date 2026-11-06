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

test('only absent metadata is defaulted; output edits preserve other routes and opaque values', () => {
  const source = { ...EMPTY_PATCH(), settings: { sample_rate: 96000, future: { opaque: [1, null] } },
    modules: [{ id: 1, module: 'essentials.output', version: 71, state: { opaque: true } },
      { id: 2, module: 'essentials.output' }],
    outputs: [{ module: 1, port: 'audio', bus_channel: 7, future: { value: 'keep' } },
      { module: 2, port: 'audio', bus_channel: 15, opaque: [false, 3] }] };
  const original = structuredClone(source);
  const doc = PatchDoc.from(source);
  expect(doc.toJSON().settings).toEqual(source.settings);
  expect(doc.toJSON().modules[0]).toMatchObject(source.modules[0]);
  expect(doc.toJSON().modules[1].version).toBe(catalogue.modules.find(m => m.id === 'essentials.output').version);
  doc.addModule('essentials.output', {}, 0, 0, 3);
  expect(doc.toJSON().outputs).toEqual([...source.outputs, { module: 3, port: 'audio', bus_channel: 0 }]);
  doc.removeModule(1);
  expect(doc.toJSON().outputs).toEqual([source.outputs[1], { module: 3, port: 'audio', bus_channel: 0 }]);
  expect(source).toEqual(original);
  const missing = PatchDoc.from({ ...EMPTY_PATCH(), modules: [{ id: 1, module: 'essentials.output' }] });
  expect(missing.toJSON().settings).toEqual({ sample_rate: 48000, max_block_frames: 256, max_channels: 16, feedback_block: 1 });
  expect(missing.toJSON().outputs).toEqual([{ module: 1, port: 'audio', bus_channel: 0 }]);
  missing.removeModule(1);
  expect(missing.toJSON().outputs).toEqual([]);
  const empty = PatchDoc.from({ ...EMPTY_PATCH(), settings: {}, outputs: [] });
  expect(empty.toJSON().settings).toEqual({});
  expect(empty.toJSON().outputs).toEqual([]);
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
    source.ui = { ...source.ui, future: structuredClone(source.future) };
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await boot(page);
    await page.evaluate(source => window.__wirescapeTest.load(source, 'Native round trip'), source);
    await settle(page);
    const saved = await page.evaluate(() => window.__wirescapeTest.save());
    expect(saved.future).toEqual(source.future);
    expect(saved.ui.future).toEqual(source.ui.future);
    if (source.version === 1) {
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
