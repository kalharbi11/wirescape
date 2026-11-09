// SPDX-License-Identifier: GPL-3.0-only
const { test, expect } = require('@playwright/test');
const { readFileSync, readdirSync } = require('node:fs');
const { resolve } = require('node:path');
const { PatchDoc } = require('../src/patch/PatchDoc');
const { EMPTY_PATCH } = require('../src/patch/types');
const { boot } = require('./helpers');
const root = resolve(__dirname, '../../..');
const fixtures = resolve(root, 'native/tests/fixtures/legacy_documents');

for (const file of readdirSync(fixtures).filter(n => n.endsWith('.json'))) {
  test(`inspection-only ${file} refuses native import without replacing the current document`, () => {
    const bytes = readFileSync(resolve(fixtures, file), 'utf8');
    const source = JSON.parse(bytes), before = structuredClone(source);
    const current = EMPTY_PATCH();
    current.vendor = { opaque: ['keep', null, { nested: true }] };
    const doc = PatchDoc.from(current);
    doc.markClean();
    const saved = JSON.stringify(doc.toJSON());
    expect(() => doc.replace(source)).toThrow(/Unsupported patch format or version/);
    expect(JSON.stringify(doc.toJSON())).toBe(saved);
    expect(doc.dirty).toBe(false);
    expect(source).toEqual(before);
    expect(readFileSync(resolve(fixtures, file), 'utf8')).toBe(bytes);
  });
}

test('native future/invalid envelopes and containers refuse atomically', () => {
  const doc = PatchDoc.from(EMPTY_PATCH());
  const saved = doc.toJSON();
  for (const change of [{ version: 99 }, { version: true }, { version: 1.5 },
    { modules: {} }, { connections: false }, { format: 'vendor.document' }]) {
    expect(() => doc.replace({ ...saved, ...change })).toThrow();
    expect(doc.toJSON()).toEqual(saved);
  }
});

test('native v1 migration retains structural collisions and unknown payloads', () => {
  const source = { format: 'wirescape.native.patch', version: 1, outputNode: 1,
    modules: [{ id: 1, type: 'essentials.vco', values: { 0: 220, 999: { opaque: true } },
      vendor: [1, 'nested'], preserved: 'original collision' }],
    connections: [{ kind: 'control', fromNode: 1, fromPort: 'vendor.out', toNode: 99, toPort: 'vendor.in', vendor: true }],
    vendor: { retain: true }, preserved: 'top collision' };
  const migrated = PatchDoc.from(source).toJSON();
  expect(migrated.version).toBe(2);
  expect(migrated.preserved).toEqual(source);
  expect(migrated.modules[0].parameters.frequency).toBe(220);
  expect(migrated.modules[0].preserved).toEqual(source.modules[0]);
  expect(migrated.modules[0].vendor).toEqual(source.modules[0].vendor);
  expect(migrated.modules[0].state).toBeUndefined();
  expect(migrated.connections[0]).toEqual({ kind: 'signal', from: { module: 1, port: 'vendor.out' },
    to: { module: 99, port: 'vendor.in' }, vendor: true });
  expect(PatchDoc.from(migrated).toJSON()).toEqual(migrated);
  const unknown = structuredClone(source);
  unknown.modules[0].type = 'vendor.unknown';
  const opaque = PatchDoc.from(unknown).toJSON();
  expect(opaque.modules[0].inactive).toBe(true);
  expect(opaque.modules[0].preserved).toEqual(unknown.modules[0]);
  expect(PatchDoc.from(opaque).toJSON()).toEqual(opaque);
});

function invalidNativeShapes() {
  const base = { format: 'wirescape.native.patch', version: 2, modules: [], connections: [] };
  const cable = { kind: 'signal', from: { module: 1, port: 'future' }, to: { module: 99, port: 'future' } };
  return [{ modules: [{ id: 'bad', module: 'vendor.x' }] },
    { modules: [{ id: 1, module: 'x' }, { id: 1, module: 'y' }] }, { modules: [null] },
    { modules: [{ id: 1, module: null }] }, { connections: [null] },
    { ui: 4 }, { ui: { positions: 'bad' } }, { ui: { view: [] } }, { ui: { labels: false } },
    { ui: { cableColors: null } },
    { connections: [{ ...cable, kind: 'future' }] },
    { connections: [{ ...cable, to: { module: 'bad', port: 'future' } }] },
    { version: 1, modules: undefined }, { version: 1, connections: undefined },
    { version: 1, modules: [{ id: 1, type: null }] },
    { version: 1, modules: [{ id: 1, type: 'essentials.vco', values: 4 }] },
    { version: 1, connections: [{ fromNode: 1, fromPort: 'x', toNode: 99, kind: 'audio' }] }]
    .map(change => ({ ...base, ...change }));
}

test('malformed native shapes refuse in the UI without changing the current patch', async ({ page }) => {
  await boot(page);
  const before = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  for (const source of invalidNativeShapes()) {
    const doc = PatchDoc.from(EMPTY_PATCH());
    const saved = doc.toJSON();
    expect(() => doc.replace(source)).toThrow();
    expect(doc.toJSON()).toEqual(saved);
    expect(doc.dirty).toBe(false);
    await expect(page.evaluate(source => window.__wirescapeTest.load(source, 'Refused'), source)).rejects.toThrow();
    expect(await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()))).toBe(before);
    expect(await page.evaluate(() => window.__wirescapeTest.patchName())).toBe('Test');
  }
  const minimal = { format: 'wirescape.native.patch', version: 2, vendor: { keep: true } };
  await page.evaluate(source => window.__wirescapeTest.load(source, 'Minimal'), minimal);
  expect(await page.evaluate(() => window.__wirescapeTest.save())).toEqual({ ...minimal, modules: [], connections: [] });
});

test('unknown and dangling native graph data remain editable and round-trip intact', () => {
  const source = { format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'vendor.future', opaque: [1, null] }],
    connections: [{ kind: 'signal', from: { module: 1, port: 'future', vendor: true },
      to: { module: 99, port: 'missing' }, opaque: { retain: true } }] };
  expect(PatchDoc.from(source).toJSON()).toEqual(source);
});
