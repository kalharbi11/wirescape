// SPDX-License-Identifier: GPL-3.0-only
// Real AudioWorklet/WASM regressions for independently playable audio modules.
// These tests do not use FakeEngine, local audio endpoints, or device permissions.
import { expect, test, type Page } from '@playwright/test';
import { CURATED_PATCHES } from '../src/app/curated';

type WireReply = { re?: string; ok?: boolean; code?: string; gen?: string };
const webPeak = (page: Page) => page.evaluate(() =>
  (window as unknown as { __wirescapeWeb: { peak: number } }).__wirescapeWeb.peak);

const boot = async (page: Page) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await expect(page.getByTestId('boot-error')).toHaveCount(0);
};

const command = (page: Page, message: Record<string, unknown>) =>
  page.evaluate(cmd => new Promise<WireReply>(resolve => {
    const bridge = (window as unknown as { __wirescapeWeb: {
      onMessage(listener: (msg: WireReply) => void): () => void;
      send(msg: Record<string, unknown>): void;
    } }).__wirescapeWeb;
    const off = bridge.onMessage(reply => {
      if (reply.re !== cmd.id) return;
      off();
      resolve(reply);
    });
    bridge.send(cmd);
  }), message);

const outputMeter = (page: Page, node: number) => page.evaluate(id => {
  const sample = (window as unknown as { __wirescapeTest: { ticker: {
    last?: { meters?: Record<string, number[]>; scope?: Record<string, number[] | Record<string, number>> }
  } } }).__wirescapeTest.ticker.last;
  // Telemetry can cross the WASM bridge as a numeric-keyed object rather than
  // an Array; normalize the received snapshot without changing its values.
  const scope = sample?.scope?.[String(id)] ?? {};
  return { meter: sample?.meters?.[String(id)] ?? [], scope: Object.values(scope) };
}, node);

test('real WASM catalogue can load a curated CHAOS scene and report output-meter peaks', async ({ page }) => {
  await boot(page);
  const modules = await page.evaluate(() => new Promise<string[]>(resolve => {
    const bridge = (window as unknown as { __wirescapeWeb: {
      onMessage(listener: (msg: { type?: string; catalogue?: { modules: { id: string }[] } }) => void): () => void;
    } }).__wirescapeWeb;
    const off = bridge.onMessage(msg => {
      if (msg.type !== 'hello') return;
      off();
      resolve(msg.catalogue?.modules.map(m => m.id) ?? []);
    });
  }));
  for (const id of ['essentials.chaos_lite', 'essentials.output', 'fx.reverb']) {
    expect(modules, `missing real WASM module: ${id}`).toContain(id);
  }

  const scene = CURATED_PATCHES.find(p => p.id === 'hollow-chapel')!;
  await page.evaluate(p => (window as any).__wirescapeTest.load(p.patch, p.name), scene);
  await expect(page.getByTestId('patch-name')).toContainText('Hollow Chapel');
  await expect.poll(() => webPeak(page), { timeout: 15_000 }).toBeGreaterThan(0.0001);
  // Engine telemetry, not a locally manufactured FakeEngine meter.
  await expect.poll(async () => (await outputMeter(page, 5)).meter[0] ?? 0).toBeGreaterThan(0);
  const sample = await outputMeter(page, 5);
  expect(sample.meter).toHaveLength(2);
  expect(sample.meter.every(Number.isFinite)).toBe(true);
  expect(sample.scope).toHaveLength(64);
  expect(sample.scope.every(Number.isFinite)).toBe(true);
});

test('real WASM reverb stays audible on dry and fully wet paths', async ({ page }) => {
  await boot(page);
  const patch = {
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'essentials.vco', version: 2, parameters: { frequency: 220 } },
      { id: 2, module: 'fx.reverb', version: 1, parameters: { mix: 0 } },
      { id: 3, module: 'essentials.output', version: 2 },
    ],
    connections: [
      { kind: 'signal', from: { module: 1, port: 'audio' }, to: { module: 2, port: 'left' } },
      { kind: 'signal', from: { module: 2, port: 'left' }, to: { module: 3, port: 'audio' } },
    ],
    outputs: [
      { module: 3, port: 'audio', bus_channel: 0 },
      { module: 3, port: 'audio', bus_channel: 1 },
    ],
  };
  const loaded = await command(page, { v: 0, id: 'wasm-reverb-load', type: 'loadGraph', patch });
  expect(loaded.ok, JSON.stringify(loaded)).toBe(true);
  await expect.poll(() => webPeak(page), { timeout: 15_000 }).toBeGreaterThan(0.0001);
  const wet = await command(page, {
    v: 0, id: 'wasm-reverb-wet', type: 'setParam', moduleId: 2, paramId: 'mix', value: 1,
  });
  expect(wet.ok, JSON.stringify(wet)).toBe(true);
  await expect.poll(() => webPeak(page), { timeout: 15_000 }).toBeGreaterThan(0.0001);
  await expect.poll(async () => (await outputMeter(page, 3)).meter[0] ?? 0).toBeGreaterThan(0);
  expect((await outputMeter(page, 3)).meter.every(Number.isFinite)).toBe(true);
});

test('real WASM CHAOS accepts a new pitch event and zero-Fall recovery', async ({ page }) => {
  await boot(page);
  const patch = {
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'host.keyboard', version: 1 },
      { id: 2, module: 'essentials.chaos_lite', version: 1,
        parameters: { fall: 0.75, level: 0.3 } },
      { id: 3, module: 'essentials.output', version: 2 },
    ],
    connections: [
      { kind: 'signal', from: { module: 1, port: 'pitch' }, to: { module: 2, port: 'pitch' } },
      { kind: 'signal', from: { module: 2, port: 'left' }, to: { module: 3, port: 'audio' } },
    ],
    outputs: [
      { module: 3, port: 'audio', bus_channel: 0 },
      { module: 3, port: 'audio', bus_channel: 1 },
    ],
  };
  expect((await command(page, { v: 0, id: 'wasm-fall-load', type: 'loadGraph', patch })).ok).toBe(true);
  expect((await command(page, {
    v: 0, id: 'wasm-fall-note1', type: 'noteOn', note: 60, velocity: 1,
  })).ok).toBe(true);
  expect((await command(page, {
    v: 0, id: 'wasm-fall-note2', type: 'noteOn', note: 72, velocity: 1,
  })).ok).toBe(true);
  const reset = await command(page, {
    v: 0, id: 'wasm-fall-reset', type: 'setParam', moduleId: 2, paramId: 'fall', value: 0,
  });
  expect(reset.ok, JSON.stringify(reset)).toBe(true);
  await expect.poll(() => webPeak(page), { timeout: 15_000 }).toBeGreaterThan(0.0001);
  expect((await outputMeter(page, 3)).scope.every(Number.isFinite)).toBe(true);
  expect((await command(page, { v: 0, id: 'wasm-fall-off', type: 'noteOff', note: 72 })).ok).toBe(true);
  // Spectral/pitch-identity proof is the native CHAOS DSP regression; this
  // checks the actual browser command, replan and audible render path.
});
