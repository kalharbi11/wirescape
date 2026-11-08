// SPDX-License-Identifier: GPL-3.0-only
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, unlinkSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { patchIndex, patchLibraryPlugin } from '../patch-library.mjs';

test('upload, update, delete are reflected by the next build index; opaque fields survive', () => {
  const dir = mkdtempSync(join(tmpdir(), 'wirescape-patches-'));
  try {
    const patch = { name: 'First', format: 'wirescape.native.patch', version: 2,
      modules: [], connections: [], outputs: [], future: { keep: true } };
    const path = join(dir, 'new-study.json');
    writeFileSync(path, JSON.stringify(patch));
    let index = JSON.parse(patchIndex(dir));
    assert.equal(index.patches[0].name, 'First');
    assert.deepEqual(index.patches[0].patch.future, { keep: true });
    writeFileSync(path, JSON.stringify({ ...patch, name: 'Updated' }));
    assert.equal(JSON.parse(patchIndex(dir)).patches[0].name, 'Updated');
    unlinkSync(path);
    assert.deepEqual(JSON.parse(patchIndex(dir)).patches, []);
    writeFileSync(path, '{bad json');
    assert.throws(() => patchIndex(dir));
  } finally { rmSync(dir, { recursive: true, force: true }); }
});

test('production plugin emits a relative, offline-compatible library asset', () => {
  const assets = [];
  patchLibraryPlugin().generateBundle.call({ emitFile: asset => assets.push(asset) });
  assert.equal(assets[0].fileName, 'patches/index.json');
  const index = JSON.parse(assets[0].source);
  assert.ok(index.patches.length > 0);
  assert.ok(index.patches.every(p => p.patch.version === 2));
});

