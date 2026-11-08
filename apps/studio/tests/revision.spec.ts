// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { Ticker } from '../src/runtime/Ticker';
import type { Telemetry } from '../src/engine/EngineClient';
import { boot, settle } from './helpers';
import { mockBridge } from './mock-bridge/bridge';
import { WebBridge } from '../src/web/WebBridge';

const packet = (effective: Telemetry['effective'], tick = 1): Telemetry => ({
  tick, time: tick / 30, effective, phase: {}, envelope: {}, meters: {},
  scope: {}, eventActivity: {}, heldNotes: [],
});

test('telemetry maps are complete snapshots, not deltas', () => {
  const ticker = new Ticker();
  const first = packet({ 1: { cutoff: 800, resonance: 0.2 }, 2: { rate: 3 } });
  first.phase = { 1: 0.25 };
  first.envelope = { 2: 0.8 };
  first.eventActivity = { '1:gate': 1 };
  ticker.push(first);

  const next = packet({ 1: { cutoff: 1200 } }, 2);
  next.phase = { 2: 0.5 };
  ticker.push(next);
  expect(ticker.last?.effective).toEqual({ 1: { cutoff: 1200 } });
  expect(ticker.last?.phase).toEqual({ 2: 0.5 });
  expect(ticker.last?.envelope).toEqual({});
  expect(ticker.last?.eventActivity).toEqual({});

  // A density rebuild preserves the most recent complete frame for new widgets.
  ticker.dropModule(1, true);
  expect(ticker.last?.effective[1]?.cutoff).toBe(1200);
  ticker.push(packet({}, 3));
  expect(ticker.last?.effective).toEqual({});
  // A new graph epoch cannot inherit values from the old graph.
  ticker.resetTelemetry();
  ticker.push(packet({ 1: { level: 0.5 } }, 4));
  expect(ticker.last?.effective).toEqual({ 1: { level: 0.5 } });
});

test('deleting a module clears retained telemetry without changing engine packets', () => {
  const ticker = new Ticker();
  const frame = packet({ 1: { level: 0.8 }, 10: { level: 0.4 } });
  frame.phase = { 1: 0.5, 10: 0.2 };
  frame.outputs = { '1:audio': 0.8, '10:audio': 0.4 };
  frame.eventActivity = { '1:gate': 1, '10:gate': 0.5 };
  ticker.push(frame);
  ticker.dropModule(1);
  expect(ticker.last?.effective[1]).toBeUndefined();
  expect(ticker.last?.phase[1]).toBeUndefined();
  expect(ticker.last?.outputs).toEqual({ '10:audio': 0.4 });
  expect(ticker.last?.eventActivity).toEqual({ '10:gate': 0.5 });
  expect(frame.effective[1]?.level).toBe(0.8);
  expect(frame.phase[1]).toBe(0.5);
  expect(frame.outputs['1:audio']).toBe(0.8);
});

test('fader live indicator clears when modulation is removed and returns when reconnected', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'essentials.scale_offset', parameters: { offset: 1 } },
      { id: 2, module: 'essentials.mixer', parameters: { level_a: 0.3 } },
    ],
    connections: [], ui: { positions: { '1': [20, 20], '2': [500, 20] } },
  }));
  await page.evaluate(() => window.__wirescapeTest.fit());
  const fader = page.getByTestId('fader-2-level_a');
  await expect(fader).not.toHaveClass(/modded/);
  const modulation = {
    kind: 'modulation' as const, from: { module: 1, port: 'control' },
    to: { module: 2, port: 'level_a' }, amount: 0.4,
  };
  await page.evaluate(c => window.__wirescapeTest.connect(c), modulation);
  await expect(fader).toHaveClass(/modded/);
  await page.evaluate(c => {
    const api = window.__wirescapeTest;
    api.engine.disconnect(c); api.doc.disconnect(c);
  }, modulation);
  await expect(fader).not.toHaveClass(/modded/);
  await page.evaluate(c => window.__wirescapeTest.connect(c), modulation);
  await expect(fader).toHaveClass(/modded/);
});

test('FakeEngine disconnect clears the VCF graph and knob within one telemetry tick', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => {
    window.__wirescapeTest.load({
      format: 'wirescape.native.patch', version: 2,
      modules: [
        { id: 1, module: 'essentials.scale_offset', parameters: { offset: 0.75 } },
        { id: 2, module: 'essentials.vcf', parameters: { cutoff: 1000, resonance: 0.3 } },
      ], connections: [],
      ui: { positions: { '1': [20, 20], '2': [490, 20] } },
    });
    window.__wirescapeTest.engine.stop();
    window.__wirescapeTest.engine.step();
    window.__wirescapeTest.fit();
  });
  await settle(page, 3);
  const filter = page.getByTestId('filter-2').locator('canvas');
  const knob = page.getByTestId('knob-2-cutoff');
  await expect(filter).toBeVisible();
  const pixels = () => filter.evaluate(c => (c as HTMLCanvasElement).toDataURL());
  const base = await pixels();
  const cable = {
    kind: 'modulation' as const, from: { module: 1, port: 'control' },
    to: { module: 2, port: 'cutoff' }, amount: 0.4,
  };
  await page.evaluate(c => {
    const api = window.__wirescapeTest;
    api.connect(c);
    api.engine.step();
  }, cable);
  await settle(page, 1);
  await expect(knob).toHaveClass(/has-mod/);
  expect(await pixels()).not.toBe(base);
  expect(await page.evaluate(() => window.__wirescapeTest.ticker.last?.effective[2]?.cutoff))
    .toBeDefined();

  // Even with the cable still attached, a missing effective key is unavailable.
  await page.evaluate(() => {
    const ticker = window.__wirescapeTest.ticker;
    ticker.push({ ...ticker.last, tick: ticker.last.tick + 1, effective: {} });
  });
  await settle(page, 1);
  expect(await pixels()).toBe(base);
  expect(await knob.getAttribute('data-live')).toBe('1000');
  // One genuine engine tick restores the live value before disconnecting.
  await page.evaluate(() => window.__wirescapeTest.engine.step());
  await settle(page, 1);
  expect(await pixels()).not.toBe(base);

  await page.evaluate(c => {
    const api = window.__wirescapeTest;
    api.engine.disconnect(c); api.doc.disconnect(c);
    api.engine.step(); // exactly one snapshot after the disconnect
  }, cable);
  await settle(page, 1);
  expect(await page.evaluate(() => window.__wirescapeTest.ticker.last?.effective[2])).toBeUndefined();
  await expect(knob).not.toHaveClass(/has-mod/);
  expect(await knob.getAttribute('data-live')).toBeNull();
  expect(await pixels()).toBe(base);
});

test('RealEngine ignores telemetry from a generation older than its committed graph', async ({ page }) => {
  await mockBridge(page);
  await boot(page);
  await expect.poll(() => page.evaluate(() => (window as any).__mockBridge.generation()))
    .not.toBe('0');
  const current = await page.evaluate(() => (window as any).__mockBridge.generation());
  await page.evaluate(gen => {
    const mock = (window as any).__mockBridge;
    mock.telemetry({ tick: 81, effective: { 1: { cutoff: 1234 } } }, gen);
    mock.telemetry({ tick: 82, effective: { 1: { cutoff: 9 } } }, String(BigInt(gen) - 1n));
  }, current);
  expect(await page.evaluate(() => window.__wirescapeTest.ticker.last?.tick)).toBe(81);
});

test('WebBridge uses the worklet generation and rejects older telemetry', () => {
  const bridge = new WebBridge();
  const received: Record<string, unknown>[] = [];
  bridge.onMessage(message => received.push(message));
  (bridge as any).gen = '10';
  (bridge as any).receive({
    kind: 'telemetry', gen: '9', peak: 0.9, telemetry: { effective: { 1: { cutoff: 9 } } },
  });
  expect(received).toHaveLength(0);
  expect(bridge.peak).toBe(0);
  (bridge as any).receive({
    kind: 'telemetry', gen: '10', peak: 0.3, telemetry: { effective: { 1: { cutoff: 1234 } } },
  });
  expect(received).toHaveLength(1);
  expect(received[0]?.gen).toBe('10');
  expect((received[0]?.telemetry as Telemetry).effective).toEqual({ 1: { cutoff: 1234 } });
});
