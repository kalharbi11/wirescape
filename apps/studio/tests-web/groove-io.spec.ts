// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';

// Real AudioWorklet/WASM, not FakeEngine: a Groove CV/gate voice without MIDI.
const groovePatch = {
  format: 'wirescape.native.patch', version: 2,
  modules: [
    { id: 1, module: 'sequencing.groove', version: 2,
      config: { version: 2, bpm: 120, tracks: [{
        length: 8, armed: true,
        steps: Array.from({ length: 8 }, () => ({ kind: 'note', key: 60, velocity: 0.9, gate: 1 })),
      }] } },
    { id: 2, module: 'essentials.vco', version: 2, parameters: { frequency: 220 } },
    { id: 3, module: 'essentials.adsr', version: 2,
      parameters: { attack: 0.005, decay: 0.05, sustain: 0.8, release: 0.1 } },
    { id: 4, module: 'essentials.vca', version: 2, parameters: { gain: 0.6 } },
    { id: 5, module: 'essentials.output', version: 2 },
  ],
  connections: [
    { kind: 'signal', from: { module: 1, port: 't1_pitch' }, to: { module: 2, port: 'pitch' } },
    { kind: 'signal', from: { module: 1, port: 't1_gate' }, to: { module: 3, port: 'gate' } },
    { kind: 'signal', from: { module: 2, port: 'audio' }, to: { module: 4, port: 'audio' } },
    { kind: 'signal', from: { module: 3, port: 'envelope' }, to: { module: 4, port: 'gain_cv' } },
    { kind: 'signal', from: { module: 4, port: 'audio' }, to: { module: 5, port: 'audio' } },
  ],
  outputs: [{ module: 5, port: 'audio', bus_channel: 0 },
    { module: 5, port: 'audio', bus_channel: 1 }],
  ui: { positions: { '1': [20, 50], '2': [1350, 50], '3': [1350, 460],
    '4': [1750, 50], '5': [2100, 50] } },
};

const bootGroove = async (page: Page) => {
  await page.goto('./');
  await page.getByTestId('web-start').click();
  await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
  await page.evaluate(p => (window as any).__wirescapeTest.load(p, 'Web Groove'), groovePatch);
  await expect(page.getByTestId('groove-1')).toBeVisible();
};

test('WASM hello advertises real Groove output ports and web patch plays pitch/gate CV', async ({ page }) => {
  await bootGroove(page);
  const livePorts = await page.evaluate(() => new Promise<Array<{ id: string; direction: string }>>(resolve => {
    const bridge = (window as any).__wirescapeWeb;
    const off = bridge.onMessage((m: any) => {
      if (m.type !== 'hello') return;
      off();
      resolve(m.catalogue.modules.find((x: any) => x.id === 'sequencing.groove')?.ports ?? []);
    });
  }));
  for (const id of ['t1_pitch', 't1_gate', 't1_notes', 't2_pitch', 't2_gate']) {
    expect(livePorts).toContainEqual(expect.objectContaining({ id, direction: 'output' }));
  }
  for (const id of ['t1_pitch', 't1_gate', 't1_notes']) {
    await expect(page.getByTestId(`port-1-output-${id}`)).toBeVisible();
  }
  // The AudioWorklet gets a genuine low->high transport command, not a fake playhead.
  await page.evaluate(() => (window as any).__wirescapeWeb.send({
    v: 0, id: 'groove-run-web', type: 'setParam', moduleId: 1, paramId: 'run', value: 1,
  }));
  await expect.poll(() => page.evaluate(() => (window as any).__wirescapeTest.ticker.last
    ?.groove?.[1]?.[0]?.playing), { timeout: 15_000 }).toBe(true);
  await expect.poll(() => page.evaluate(() => (window as any).__wirescapeWeb.peak),
    { timeout: 15_000 }).toBeGreaterThan(0.0001);
});

test.describe('mobile Groove output jacks', () => {
  test.use({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true, deviceScaleFactor: 3 });

  test('keeps output rail visible and anchors cables on responsive card edge', async ({ page }) => {
    await bootGroove(page);
    for (const id of ['t1_pitch', 't1_gate', 't1_notes']) {
      await expect(page.getByTestId(`port-1-output-${id}`)).toHaveCSS('display', 'flex');
    }
    await expect(page.getByTestId('ports-more-1-output')).toBeVisible();
    const sizes = await page.evaluate(() => {
      const card = document.querySelector('[data-testid="module-1"]')!.getBoundingClientRect();
      const port = document.querySelector('[data-testid="port-1-output-t1_pitch"]')!.getBoundingClientRect();
      return { cardWidth: card.width, rightGap: Math.abs(port.right - card.right) };
    });
    expect(sizes.cardWidth).toBeLessThanOrEqual(390);
    expect(sizes.rightGap).toBeLessThan(28);
  });
});
