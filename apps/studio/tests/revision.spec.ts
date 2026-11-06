// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from '@playwright/test';
import { Ticker } from '../src/runtime/Ticker';
import type { Telemetry } from '../src/engine/EngineClient';
import { boot } from './helpers';

const packet = (effective: Telemetry['effective'], tick = 1): Telemetry => ({
  tick, time: tick / 30, effective, phase: {}, envelope: {}, meters: {},
  scope: {}, eventActivity: {}, heldNotes: [],
});

test('sparse telemetry retains effective changes across packets and offscreen redraws', () => {
  const ticker = new Ticker();
  ticker.push(packet({ 1: { cutoff: 800, resonance: 0.2 }, 2: { rate: 3 } }));
  ticker.push(packet({ 1: { cutoff: 1200 } }, 2));
  ticker.push(packet({}, 3));
  expect(ticker.last?.effective).toEqual({ 1: { cutoff: 1200, resonance: 0.2 }, 2: { rate: 3 } });
  // Density rebuilds replace widgets, but the engine need not resend a
  // stationary parameter. Keep its value for the replacement binding.
  ticker.dropModule(1, true);
  expect(ticker.last?.effective[1]?.cutoff).toBe(1200);
  // Graph replacement may reuse IDs for entirely different module types.
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
