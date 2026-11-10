// SPDX-License-Identifier: GPL-3.0-only
import { expect, test, type Page } from '@playwright/test';
import { boot, settle, state } from './helpers';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { spawnSync } from 'node:child_process';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { mkdtempSync, readFileSync, rmSync, writeFileSync, mkdirSync } from 'node:fs';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { tmpdir } from 'node:os';
// @ts-expect-error Node is provided by Playwright; Studio's app tsconfig is browser-only.
import { join, resolve } from 'node:path';
// Playwright runs this file in Node; no browser-global process type is needed.
declare const process: { env: Record<string, string | undefined> };
import { MODULE_PRESETS, resolvePreset } from '../src/app/modulePresets';

const menu = (page: Page) => page.getByTestId('module-presets-1');
const values = async (page: Page) => (await state(page)).modules.find((m: any) => m.id === 1)?.parameters ?? {};
const flush = (page: Page) => page.evaluate(() => window.__wirescapeTest.flushHistory());

test.describe('CHAOS preset UI', () => {
test.beforeEach(async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.addModule('essentials.chaos_lite'));
  await settle(page);
});

test('eight CHAOS presets are a UI-only header control, not module metadata', async ({ page }) => {
  await expect(menu(page)).toHaveCount(1);
  await expect(menu(page).locator('option')).toHaveText([
    'Custom', 'Warm fifth', 'Bent wire', 'Broken bell', 'Slow weather',
    'Basalt drone', 'Glass pluck', 'Iron rain', 'Tide choir',
  ]);
  const saved = await state(page);
  expect(saved.modules[0].module).toBe('essentials.chaos_lite');
  expect(JSON.stringify(saved)).not.toContain('modulePresets');
  expect(JSON.stringify(saved)).not.toContain('Warm fifth');
});

test('preset resets every omitted parameter, saves/reloads and is exactly one Undo', async ({ page }) => {
  await page.evaluate(() => window.__wirescapeTest.commitParam(1, 'cutoff', 1337));
  await flush(page);
  const previous = await values(page);
  const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
  await menu(page).selectOption('Bent wire');
  await flush(page);
  const next = await values(page);
  expect(next.route).toBe(2);
  expect(next.depth).toBe(0.8);
  expect(next.feedback).toBe(0.7);
  expect(next.cutoff).toBe(9000); // descriptor default, not the prior manual setting
  expect(Object.keys(next)).toHaveLength(26);
  expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth + 1);
  await page.getByTestId('undo').click();
  await expect.poll(async () => await values(page)).toEqual(previous);
  await page.getByTestId('redo').click();
  await expect.poll(async () => (await values(page)).cutoff).toBe(9000);
  const saved = await page.evaluate(() => JSON.stringify(window.__wirescapeTest.save()));
  await page.evaluate(p => window.__wirescapeTest.load(JSON.parse(p), 'Reloaded'), saved);
  await settle(page);
  expect(JSON.stringify(await state(page))).toBe(saved);
  await expect(menu(page)).toHaveValue('Bent wire');
});

test('invalid injected preset shows a toast and preserves the patch and history', async ({ page }) => {
  const before = await state(page);
  const depth = await page.evaluate(() => window.__wirescapeTest.historyDepth().past);
  await page.evaluate(() => {
    // Mutate only this test page's UI preset data; native descriptors stay intact.
    Object.assign(window.__wirescapeTest.modulePresets['essentials.chaos_lite'][1].params, { route: 99 });
  });
  await menu(page).selectOption('Bent wire');
  const toast = page.getByTestId('toast');
  await expect(toast).toContainText('Invalid preset value: essentials.chaos_lite:route');
  await expect(toast).toHaveClass(/error/);
  await flush(page);
  expect(await state(page)).toEqual(before);
  expect(await page.evaluate(() => window.__wirescapeTest.historyDepth().past)).toBe(depth);
  await expect(menu(page)).toHaveValue('');

  // Corrected data works again without reloading the app.
  await page.evaluate(() => { Object.assign(window.__wirescapeTest.modulePresets['essentials.chaos_lite'][1].params, { route: 2 }); });
  await menu(page).selectOption('Bent wire');
  await expect(menu(page)).toHaveValue('Bent wire');
});

test('manual parameter changes show Custom without writing preset names', async ({ page }) => {
  await menu(page).selectOption('Warm fifth');
  await flush(page);
  await expect(menu(page)).toHaveValue('Warm fifth');
  await page.evaluate(() => window.__wirescapeTest.commitParam(1, 'body_mix', 0.42));
  await expect(menu(page)).toHaveValue('');
  const saved = JSON.stringify(await state(page));
  expect(saved).not.toContain('Warm fifth');
  expect(saved).not.toContain('Custom');
  expect((await values(page)).body_mix).toBe(0.42);
});

test('all presets use finite descriptor-bounded values, preserving unknown module data and cables', async ({ page }) => {
  const base = await state(page);
  base.modules[0].futureField = { unsupported: true };
  base.modules[0].parameters = { futuristic: 0.314 };
  await page.evaluate(p => window.__wirescapeTest.load(p, 'Future patch'), base);
  await settle(page);
  const names = ['Warm fifth', 'Bent wire', 'Broken bell', 'Slow weather',
    'Basalt drone', 'Glass pluck', 'Iron rain', 'Tide choir'];
  const descriptor = await page.evaluate(() => window.__wirescapeTest.registry.get('essentials.chaos_lite'));
  for (const name of names) {
    await menu(page).selectOption(name);
    const p = await values(page);
    for (const param of descriptor.parameters) {
      expect(Number.isFinite(p[param.id]), name + ':' + param.id).toBe(true);
      expect(p[param.id], name + ':' + param.id).toBeGreaterThanOrEqual(param.minimum);
      expect(p[param.id], name + ':' + param.id).toBeLessThanOrEqual(param.maximum);
    }
    expect(p.futuristic).toBe(0.314);
    const m = (await state(page)).modules[0];
    expect(m.futureField).toEqual({ unsupported: true });
    expect((await state(page)).connections).toEqual(base.connections);
  }
});

});

/** Optional here; the task-acceptance native lane must set CHAOS_RENDER explicitly.
 * The renderer uses production app_engine + runtime_patch, never FakeEngine audio.
 */
test('offline WAV: all eight presets render finite, audible left/right bus levels', async () => {
  // Sixteen production process invocations exceed the 30 s browser-test default.
  // Each process still has its independent 60 s timeout and no test is skipped in native acceptance.
  test.setTimeout(5 * 60_000);
  test.skip(!process.env.CHAOS_RENDER, 'Native WAV renderer is built in the task acceptance lane');
  const renderer = resolve(process.env.CHAOS_RENDER!);
  const catalog = JSON.parse(readFileSync(resolve('public/wirescape_modules.json'), 'utf8'));
  const descriptor = catalog.modules.find((m: any) => m.id === 'essentials.chaos_lite');
  expect(descriptor).toBeTruthy();
  const directory = mkdtempSync(join(tmpdir(), 'chaos-preset-evidence-'));
  try {
    for (const [index, preset] of MODULE_PRESETS['essentials.chaos_lite']!.entries()) {
      for (const port of ['left', 'right']) {
        const name = index + '-' + port;
        const source = {
          'SPDX-License-Identifier': 'GPL-3.0-only',
          name: 'CHAOS-lite ' + preset.name + ' ' + port,
          rate: 48000, block: 128, seconds: 2, seed: 7, peak_limit: 3,
          patch: {
            format: 'wirescape.native.patch', version: 2, settings: { seed: 7 },
            name: 'CHAOS preset offline evidence',
            modules: [
              { id: 1, module: 'essentials.chaos_lite', version: 1,
                parameters: resolvePreset(descriptor, preset) },
              { id: 2, module: 'essentials.output', version: 2, parameters: {} },
            ],
            connections: [{
              kind: 'signal',
              from: { module: 1, port },
              to: { module: 2, port: 'audio' },
            }],
            outputs: [
              { module: 2, port: 'audio', bus_channel: 0 },
              { module: 2, port: 'audio', bus_channel: 1 },
            ],
          },
          events: [{ frame: 0, type: 'noteOn', node: 1, key: 60 }],
          windows: [{ kind: 'active', start: 16000, end: 76000 }],
          frequencies: [],
        };
        const casePath = join(directory, name + '.json'), out = join(directory, name);
        mkdirSync(out);
        writeFileSync(casePath, JSON.stringify(source));
        const child = spawnSync('python', [
          resolve('../../native/tools/render/evidence.py'), '--render', renderer,
          '--case', casePath, '--out', out, '--configuration', 'Release',
        ], { encoding: 'utf8', timeout: 60000 });
        expect(child.status, preset.name + '/' + port + ': ' + child.stderr).toBe(0);
        const metrics = JSON.parse(readFileSync(join(out, 'measurements.json'), 'utf8'));
        expect(metrics.status).toBe('passed');
        const active = metrics.assertions.find((x: any) => x.check === 'active');
        expect(active?.rms_fs, preset.name + '/' + port).toBeGreaterThan(0.01);
        console.log('CHAOS WAV', preset.name, port, 'rms_fs', active.rms_fs,
          'source_sha', metrics.source_sha, 'patch_sha256', metrics.patch_sha256);
      }
    }
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
});
