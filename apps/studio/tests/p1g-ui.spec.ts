// SPDX-License-Identifier: GPL-3.0-only
import { expect, test } from '@playwright/test';
import { boot, settle } from './helpers';
import { lfoTraceValue } from '../src/panels/widgets/waveTrace';

const loadDisplays = async (page: import('@playwright/test').Page) => {
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [
      { id: 1, module: 'essentials.lfo' },
      { id: 2, module: 'essentials.adsr' },
    ],
    connections: [],
    ui: { positions: { 1: [100, 110], 2: [530, 110] },
      labels: {}, view: { x: 0, y: 0, zoom: 1 } },
  }, 'P1G displays'));
  await settle(page);
};

test('patch viewport is light grey while cards and chrome remain white', async ({ page }) => {
  await boot(page);
  const colors = await page.evaluate(() => ({
    canvas: getComputedStyle(document.querySelector('.viewport')!).backgroundColor,
    rail: getComputedStyle(document.querySelector('.library')!).backgroundColor,
    token: getComputedStyle(document.documentElement).getPropertyValue('--canvas-ground').trim(),
  }));
  expect(colors).toEqual({
    canvas: 'rgb(245, 245, 243)',
    rail: 'rgb(255, 255, 255)',
    token: '#f5f5f3',
  });
});

for (const scale of [1, 2]) {
  test.describe(`canvas resolution at DPR ${scale}`, () => {
    test.use({ deviceScaleFactor: scale });

    test('ADSR and LFO backing stores track 50/100/185% zoom and Pi cap', async ({ page }) => {
      await boot(page);
      await loadDisplays(page);
      await page.evaluate(() => window.__wirescapeTest.setPref({ profile: 'normal', autoProfile: false }));
      for (const zoom of [0.5, 1, 1.85]) {
        await page.evaluate(z => window.__wirescapeTest.zoomTo(z), zoom);
        await page.waitForTimeout(160); // one debounced bitmap refresh
        const sizes = await page.locator('.wave-canvas canvas, .env-canvas canvas')
          .evaluateAll(items => items.map(node => {
            const canvas = node as HTMLCanvasElement;
            return { w: canvas.width, h: canvas.height,
              cssW: canvas.parentElement!.offsetWidth,
              cssH: canvas.parentElement!.offsetHeight };
          }));
        expect(sizes).toHaveLength(2);
        for (const size of sizes) {
          expect(size.w).toBe(Math.round(size.cssW * Math.min(4, zoom * scale)));
          expect(size.h).toBe(Math.round(size.cssH * Math.min(4, zoom * scale)));
        }
      }
      await page.evaluate(() => window.__wirescapeTest.setPref({ profile: 'pi' }));
      await page.waitForTimeout(160);
      const capped = await page.locator('.wave-canvas canvas').evaluate(node => {
        const c = node as HTMLCanvasElement;
        return { px: c.width, css: c.parentElement!.offsetWidth };
      });
      expect(capped.px).toBe(Math.round(capped.css * Math.min(2, 1.85 * scale)));
    });
  });
}

test('LFO trace moves with telemetry instead of an independent animation', async ({ page }) => {
  await boot(page);
  await loadDisplays(page);
  await page.evaluate(() => window.__wirescapeTest.zoomTo(1));
  await settle(page);
  const wave = page.getByTestId('wave-1').locator('canvas');
  await expect(wave).toBeVisible();
  const a = await wave.evaluate(c => (c as HTMLCanvasElement).toDataURL());
  await page.waitForTimeout(240);
  const b = await wave.evaluate(c => (c as HTMLCanvasElement).toDataURL());
  expect(b).not.toEqual(a);
  await expect(page.getByTestId('wave-1')).not.toContainText('not available');
});

test('scope head spans a whole cycle at slow and fast LFO rates', () => {
  // An elapsed phase is independent of rate: 0..1 maps to one complete
  // oscillation at the newest end, not to the first half of a two-cycle view.
  for (const hz of [0.125, 20]) {
    const y = Array.from({ length: 241 }, (_, frame) => {
      const t = frame / (240 * hz);
      return lfoTraceValue((t * hz) % 1, 1, 1, 0);
    });
    expect(Math.min(...y)).toBeLessThan(-0.99);
    expect(Math.max(...y)).toBeGreaterThan(0.99);
    expect(lfoTraceValue(0.25, 1, 1, 0)).toBeCloseTo(1, 8);
    expect(lfoTraceValue(0.75, 1, 1, 0)).toBeCloseTo(-1, 8);
  }
});


test('MIDI In endpoint choices persist, including disconnected selections', async ({ page }) => {
  await boot(page);
  await page.evaluate(() => window.__wirescapeTest.load({
    format: 'wirescape.native.patch', version: 2,
    modules: [{ id: 1, module: 'midi.in', parameters: {},
      config: { endpoint: 'midi:disconnected', opaque: 'preserve' } }],
    connections: [],
    ui: { positions: { 1: [130, 130] }, labels: {},
      view: { x: 0, y: 0, zoom: 1 } },
  }, 'MIDI input'));
  await settle(page);
  const input = page.getByTestId('midi-endpoint-1');
  await expect(input).toHaveValue('midi:disconnected');
  await expect(input.locator('option:checked')).toContainText('(not connected)');
  await input.selectOption('midi:1');
  await expect(input).toHaveValue('midi:1');
  const saved = await page.evaluate(() => window.__wirescapeTest.save());
  expect(saved.modules[0].config).toEqual({ endpoint: 'midi:1', opaque: 'preserve' });
  await page.evaluate(p => window.__wirescapeTest.load(p), saved);
  await settle(page);
  await expect(page.getByTestId('midi-endpoint-1')).toHaveValue('midi:1');
});

test('Settings MIDI menu toggles enablement of the selected engine device', async ({ page }) => {
  await boot(page);
  await page.getByTestId('open-settings').click();
  const device = page.getByTestId('midi-device');
  const enabled = page.getByTestId('midi-enabled');
  await expect(device).toBeVisible();
  await expect(enabled).toHaveAttribute('aria-pressed', 'true');
  await enabled.click();
  await expect(enabled).toHaveAttribute('aria-pressed', 'false');
  expect((await page.evaluate(() => window.__wirescapeTest.engine.listMidiDevices()))
    .find((d: { id: string }) => d.id === 'midi:0').enabled).toBe(false);
  await device.selectOption('midi:1');
  await expect(enabled).toHaveAttribute('aria-pressed', 'true');
  await device.selectOption('midi:0');
  await expect(enabled).toHaveAttribute('aria-pressed', 'false');
  await enabled.click();
  await expect(enabled).toHaveAttribute('aria-pressed', 'true');
});
